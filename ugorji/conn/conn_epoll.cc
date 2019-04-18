#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <thread>
#include <mutex>
#include <chrono>
#include <iostream>
#include <atomic>

//#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
//#include <sys/types.h>
#include <signal.h>
#include <sys/epoll.h>

#include "../util/bufio.h"
#include "../util/logging.h"
#include "../util/lockset.h"
#include "../util/slice.h"
#include "../util/bigendian.h"
#include "conn.h"

namespace ugorji { 
namespace conn { 

// NOTE:
//   - recv/send/shutdown vs read/write/close. 
//     It is documented that they are equivalent in Linux.
//     Also, under epoll err, shutdown may crash server.
// 
// How it works:
//   - I/O thread handles most of the reading/writing to sockets
//   - When a worker handles a job, it does first cut of writing to the socket.
//     This is because EAGAIN only comes again (for edge-triggered) if it already came once.

//using ugorji::util::Tracef;


std::atomic<size_t> SEQ;
const size_t FD_BUF_INCR = 256;
const bool GET_VIA_ITER = false; // Iteration doesn't support bloom filter optimization

struct StreamError {
    std::string S;
};

std::string errnoStr(std::string prefix = "", std::string suffix = "") {
    char errbuf[32];
    char* errbuf2 = strerror_r(errno, errbuf, 32);
    return prefix + "(" + std::to_string(errno) + ") " + errbuf2 + suffix;
}

bool errnoChk(int i, std::string* err, std::string prefix = "", std::string suffix = "") {
    if(i < 0) {
        *err = errnoStr(prefix, suffix);
        return true;
    }
    return false;
}

std::string streamErrText(std::string cat, uint8_t expecting, uint8_t received) {
    char errbuf[64];
    snprintf(errbuf, 64, "Invalid tag for %s: Expect 0x%x, Got: 0x%x", 
             cat.c_str(), expecting, received);
    return std::string(errbuf);
}

void closeFd(int fd, std::string cat = "", bool useShutdown = true, 
             bool logIt = true, std::string* err = nullptr) {
    //close(fd)
    //useShutdown=true causes abort of our program
    //seems these fd that are non-block must be closed w/ close (not shutdown).
    useShutdown = false;
    int i;
    if(useShutdown) i = ::shutdown(fd, SHUT_RDWR);
    else i = ::close(fd);
    if(i < 0) {
        std::string err2 = errnoStr("ERROR: shutdown " + cat + " fd " + std::to_string(fd) + ": ");
        if(logIt) LOG(ERROR, "%s", err2.c_str());
        else if(err != nullptr) *err = err2;
    }
}

void Server::work() {
    reqFrame* sb;
    while(true) {
        if(shuttingDown()) return;
        //get work. If none, just wait on condition.
        {
            std::unique_lock<std::mutex> fdlk(reqMu_);
            while(reqQ_.size() == 0) {
                //fprintf(stderr, "Waiting for work\n");
                if(shuttingDown()) return;
                reqCond_.wait(fdlk);
            }
            sb = reqQ_.front();
            reqQ_.pop_front();
        }
        slice_bytes sbOut;
        char* err = nullptr;
        hdlr_(sb->r_, sbOut, &err);
        if(err != nullptr) {
            LOG(ERROR, "<work>: Error handling: %s", err);
            continue;
        }
        writeClientFd(sb->fd_, &sbOut);
        LOG(TRACE, "<work>: DONE handling fd: %d", sb->fd_);
    }
}

void Server::waitFor(bool joinThreads, bool untilStopped) {
    if(joinThreads) {
        LOG(TRACE, "<waitfor> joining thread ...", 0);
        thr_->join();
        LOG(TRACE, "<waitfor> joined thread", 0);
        for(int i = 0; i < numWorkers_; ++i) workers_[i]->join();
    }
    if(untilStopped){
        std::unique_lock<std::mutex> lk(shutdownMu_);
        while(state_ != S_STOPPED) {
            LOG(TRACE, "<waitfor_stopped>: ...", 0);
            shutdownCond_.wait(lk);
            LOG(TRACE, "<waitfor_stopped>: condition returned. state_: %d", state_.load());
        }
    }
}
    
void Server::readClientFd(int fd) {
    LOG(TRACE, "<work>: reading fd: %d", fd);
    auto x = clientfds_.find(fd)->second;
    std::lock_guard<std::mutex> lk(x->mu_);
    
    while(true) {
        ::slice_bytes_expand(&x->in_, FD_BUF_INCR);
        int n2 = ::read(fd, &x->in_.bytes.v[x->in_.bytes.len], FD_BUF_INCR);
        if(n2 > 0) {
            x->in_.bytes.len += n2;
        } 
        if(errno != 0) {
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            LOG(ERROR, "<runLoop>: bytes read (%d) <= 0, with errno: %s", n2, errnoStr().c_str());
            removeFd(fd);
            return;
        }
    }
    
    while(true) {
        if(x->in_.bytes.len == 0) break;
        uint8_t numBytesForLen = x->in_.bytes.v[0];
        if(numBytesForLen > 7) {
            LOG(ERROR, "<runLoop>: invalid numBytesForLen: %d", numBytesForLen);
            removeFd(fd);
            return;
        }
        if(x->in_.bytes.len < (numBytesForLen+1)) break;
        slice_bytes fb;
        uint8_t arr2[8];
        memcpy(arr2, &x->in_.bytes.v[1], numBytesForLen);       
        fb.bytes.len = util_big_endian_read_uint64(arr2);
        fb.bytes.v = (char*)malloc(fb.bytes.len);
        memcpy(fb.bytes.v, &x->in_.bytes.v[1+numBytesForLen], fb.bytes.len);
        memmove(&x->in_.bytes.v[0], &x->in_.bytes.v[1+numBytesForLen+fb.bytes.len], x->in_.bytes.len-(1+numBytesForLen+fb.bytes.len));
        x->in_.bytes.len -= (1+numBytesForLen+fb.bytes.len);
        reqFrame* mf = new reqFrame(x->fd_, fb);
        {
            std::lock_guard<std::mutex> lk(reqMu_);
            reqQ_.push_back(mf);
        }
    }    
}

void Server::writeClientFd(int fd, slice_bytes* sb) {
    LOG(TRACE, "<work>: writing fd: %d", fd);
    auto x = clientfds_[fd];
    std::lock_guard<std::mutex> lk(x->mu_);
    while(x->out_.bytes.len > 0) {
        int n2 = ::write(fd, &x->out_.bytes.v, x->out_.bytes.len);
        if(n2 == x->out_.bytes.len) {
            x->out_.bytes.len = 0;
        } else if(n2 > 0) {
            ::memmove(&x->out_.bytes.v[0], &x->out_.bytes.v[n2], x->out_.bytes.len-n2);
            x->out_.bytes.len -= n2;
        }
        if(errno != 0) {
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            LOG(ERROR, "<runLoop>: error writing bytes after %d, with errno: %s", n2, errnoStr().c_str());
            removeFd(fd);
            return;
        }
    }
    
    if(sb == nullptr) return;
    
    while(x->out_.bytes.len == 0 && sb->bytes.len > 0) {
        //::slice_bytes_expand(x->out_, FD_BUF_INCR);
        int n2 = ::write(fd, &sb->bytes.v, sb->bytes.len);
        if(n2 > 0) {
            sb->bytes.v = &sb->bytes.v[n2];
            sb->bytes.len -= n2;
        }
        if(errno != 0) {
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            LOG(ERROR, "<runLoop>: error writing bytes after %d, with errno: %s", n2, errnoStr().c_str());
            removeFd(fd);
            return;
        }
    }
    if(sb->bytes.len > 0) slice_bytes_append(&x->out_, sb->bytes.v, sb->bytes.len);
}

void Server::runLoop() {
    std::string err;
    const int MAXEVENTS = 1024;
    epoll_event events[MAXEVENTS];
    //Can't get signals to work here. don't seem to understand it well.
    //sigset_t sigmask;
    //sigemptyset(&sigmask);
    //sigaddset(&sigmask, interruptSig_);
    while(true) {
        if(shuttingDown()) break;
        LOG(TRACE, "<runloop>: epoll_pwait ...", 0);
        //int n = epoll_pwait(efd_, events, MAXEVENTS, -1, &sigmask);
        int n = epoll_wait(efd_, events, MAXEVENTS, -1);
        LOG(TRACE, "<runloop>: epoll_pwait returned: %d", n);
        if(n == -1) {
            if(errno == EINTR) continue;
            err = errnoStr("ERROR: creating epoll: ");
            LOG(ERROR, "%s", err.c_str());
            return;
        }
        for(int i = 0; i < n; ++i) {
            LOG(TRACE, "<runloop>: i: %d, events: %d, fd: %d", i, events[i].events, events[i].data.fd);
            bool pErr = (events[i].events & EPOLLERR) != 0;
            bool pHup = (events[i].events & EPOLLHUP) != 0;
            bool pRdhup = (events[i].events & EPOLLRDHUP) != 0;
            if(pErr || pHup || pRdhup) {
                if(pErr || pHup) {
                    LOG(ERROR, "<runloop>: fd: %d, Epoll Events: 0x%x, ERR: %d, RDHUP: %d, HUP: %d", 
                        events[i].data.fd, int(events[i].events), pErr, pRdhup, pHup);
                } else {
                    LOG(INFO, "<runloop>: Remote socket closed: fd: %d, Epoll Events: 0x%x", 
                        events[i].data.fd, int(events[i].events));
                }
                removeFd(events[i].data.fd, &err);
                err = "";
                continue;
            }
            // if((events[i].events & EPOLLIN) == 0) continue;
            if(sockfd_ == events[i].data.fd) {
                if((events[i].events & EPOLLIN) != 0) acceptClients(true, &err);
                //no need to check for error, since we say continueOnError till EAGAIN
            } else if((events[i].events & EPOLLOUT) != 0) {
                readClientFd(events[i].data.fd);
            } else if((events[i].events & EPOLLOUT) != 0) {
                writeClientFd(events[i].data.fd, nullptr);
            }
        }
    }   
}

bool Server::shuttingDown() {
    int x = state_.load();
    return (x == S_STOPPING || x == S_STOPPED);
}

void Server::addFd(int sockfd, std::string* err) {
    std::lock_guard<std::mutex> lk(mu_);
    epoll_event evt{};
    evt.data.fd = sockfd;
    evt.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
    if(epoll_ctl(efd_, EPOLL_CTL_ADD, sockfd, &evt) == -1) {
        std::string err2 = errnoStr("ERROR: registering sockfd on epoll: ");
        if(err == nullptr) LOG(ERROR, "%s", err2.c_str());
        else if(err != nullptr) *err = err2;
        return;
    }
    if(sockfd != sockfd_) {
        auto fdbufs = new fdBufs(sockfd);
        clientfds_[sockfd] = fdbufs;
        LOG(INFO, "Adding Connection Socket fd: %d", sockfd);
    }
}

void Server::removeFd(int sockfd, std::string* err) {
    if(sockfd == sockfd_) return;
    std::lock_guard<std::mutex> lk(mu_);
    auto it = clientfds_.find(sockfd);
    if(it != clientfds_.end()) {
        LOG(INFO, "Removing socket fd: %d", sockfd);
        clientfds_.erase(it);
        closeFd(sockfd, "", true, false, err);
    }
}

void Server::acceptClients(bool continueOnError, std::string* err) {
    //acceptThreadId_ = gettid();
    for(int i = 0; ; ++i) {
        if(shuttingDown()) break;
        sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        LOG(TRACE, "<accept>: Waiting on accept ...", 0);
        int newsockfd = accept4(sockfd_, (struct sockaddr *) &cli_addr, &cli_len, SOCK_NONBLOCK);
        if(newsockfd == -1) {
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::string err2 = errnoStr("<accept>: Error accepting connection: ");
            if(continueOnError) {
                LOG(ERROR, "%s", err2.c_str());
                continue;
            }
            *err = err2;
            return;
        } 
        LOG(TRACE, "<Accept>: Accepted New Connection. Sockfd: %d", newsockfd);
        addFd(newsockfd, err);
    }   
    *err = "";
    LOG(TRACE, "<accept>: DONE", 0);
}

void Server::start(std::string* err) {
    // adapted from: http://www.linuxhowtos.org/data/6/server.c
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(errnoChk(sockfd_, err, "ERROR: opening socket: ")) return;
    int on = 1;
    //Avoiding the "Address In Use"
    if(errnoChk(setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)), 
                err, "ERROR: setting reuse-addr on socket: ")) return;
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno_);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(errnoChk(bind(sockfd_, (struct sockaddr *) &serv_addr, sizeof(serv_addr)),
                err, "ERROR: binding socket: ")) return;
    //make it non-blocking before you start listening    
    int i = fcntl(sockfd_, F_GETFL, 0);
    if(errnoChk(i, err, "ERROR: Getting fcntl flags: ")) return;
    i |= O_NONBLOCK;
    if(errnoChk(fcntl(sockfd_, F_SETFL, i), err, "ERROR: Getting fcntl flags: ")) return;
    if(errnoChk(listen(sockfd_, 5), err, "ERROR: listening on  socket: ")) return;    
    efd_ = epoll_create1(0);
    if(errnoChk(efd_, err, "ERROR: creating epoll: ")) return;

    addFd(sockfd_, err);
    if(err->size() > 0) return;
    state_ = S_STARTING;
    thr_ = new std::thread(&Server::runLoop, this);
    for(int i = 0; i < numWorkers_; ++i) {
        std::thread* thr = new std::thread(&Server::work, this);
        workers_.push_back(thr);
        //thr.detach();
    }
    LOG(TRACE, "<start> waiting for stopped ...", 0);
    waitFor(false, true); 
    LOG(TRACE, "<start> stopped ...", 0);
}

void Server::stop() {
    if(shuttingDown()) return;
    std::string err;
    state_ = S_STOPPING;
    //send interruptsig_, so it interrupts any on-going epoll_wait.
    pthread_kill(thr_->native_handle(), interruptSig_);
    reqCond_.notify_all();
    waitFor(true, false);
    closeFd(efd_, "", false);
    delete thr_;
    for(int i = 0; i < numWorkers_; ++i) delete workers_[i];
    for(auto it = clientfds_.begin(); it != clientfds_.end(); ++it) {
        closeFd(it->first);
        delete it->second;
    }
    clientfds_.clear();
    closeFd(sockfd_);
    state_ = S_STOPPED;
    shutdownCond_.notify_all();
    LOG(INFO, "<stop>: Server STOPPED", 0);
}

Server::~Server() {
    stop();
}

reqFrame::~reqFrame() { }
fdBufs::~fdBufs() {}

} //close namespace conn
} // close namespace ugorji

