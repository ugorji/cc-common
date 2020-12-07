#ifdef __linux__

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
#include "conn.h"

namespace ugorji { 
namespace conn { 

// NOTE:
//   - recv/send/shutdown vs read/write/close. 
//     It is documented that they are equivalent in Linux.
//     Also, under epoll err, shutdown may crash server.

const int MAXEVENTS = 1024;   // max epoll events to get at a time
const int STEAL_TIME_MS = 10; // time to try and run tasks withing runLoop
const bool ALLOW_CURR_THREAD_FOR_MGR_RUN = true; 
std::string errnoStr(const std::string& prefix, const std::string& suffix) {
    // char errbuf[32];
    // strerror_r(errno, errbuf, 32);
    // return prefix + "(" + std::to_string(errno) + ") " + errbuf + suffix;
    char errbuf[32];
    char* errbuf2 = strerror_r(errno, errbuf, sizeof(errbuf));
    return prefix + "(" + std::to_string(errno) + ") " + errbuf2 + suffix;
}

bool errnoChk(int fd, std::string* err, bool halt = false,
              const std::string& prefix = "", const std::string& suffix = "") {
    if(fd >= 0) return false;
    
    if(halt) {
        LOG(ERROR, "error: %s", errnoStr(prefix, suffix).c_str());
        throw errnoStr(prefix, suffix);
    }
    
    *err = errnoStr(prefix, suffix);
    return true;
}

std::string streamErrText(const std::string& cat, uint8_t expecting, uint8_t received) {
    char errbuf[64];
    snprintf(errbuf, 64, "Invalid tag for %s: Expect 0x%x, Got: 0x%x", cat.c_str(), expecting, received);
    return std::string(errbuf);
}

void closeFd(int fd, std::string* err, const std::string& cat, bool useShutdown, bool logIt) {
    //close(fd)
    //useShutdown=true causes abort of our program
    //seems these fd that are non-block must be closed w/ close (not shutdown).
    useShutdown = false;
    int i;
    if(useShutdown) i = ::shutdown(fd, SHUT_RDWR);
    else i = ::close(fd);
    if(i < 0) {
        *err = errnoStr("ERROR: shutdown " + cat + " fd " + std::to_string(fd) + ": ");
        if(logIt) LOG(ERROR, "%s", (*err).c_str());
    }
}

int listenport(int port, std::string* err) {
    // adapted from: http://www.linuxhowtos.org/data/6/server.c
    auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(errnoChk(sockfd, err, "ERROR: opening socket: ")) return -1;
    int on = 1;
    //Avoiding the "Address In Use"
    if(errnoChk(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)), 
                err, "ERROR: setting reuse-addr on socket: ")) return -1;
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(errnoChk(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)),
                err, "ERROR: binding socket: ")) return -1;
    //make it non-blocking before you start listening    
    int i = fcntl(sockfd, F_GETFL, 0);
    if(errnoChk(i, err, "ERROR: Getting fcntl flags: ")) return -1;
    i |= O_NONBLOCK;
    if(errnoChk(fcntl(sockfd, F_SETFL, i), err, "ERROR: Getting fcntl flags: ")) return -1;
    if(errnoChk(listen(sockfd, 5), err, "ERROR: listening on  socket: ")) return -1;    
    return sockfd;
}

void Manager::run(std::function<Handler&()> fn, bool useThisThread) {
    auto workers = maxWorkers_;
    if(workers<1) workers = 1;
    for(int i = 0; i < workers; i++) {
        auto svr = new Server(fn(), *this);
        servers_.push_back(svr);
        if(ALLOW_CURR_THREAD_FOR_MGR_RUN && useThisThread && i == workers-1) {
            svr->run();
        } else {
            auto thr = new std::thread(&Server::run, svr);
            threads_.push_back(thr);
        }
        numWorkers_++;
    }
}

void Manager::close() {
    // Do not block or raise signals, as it is likely called by a signal handler.
    if(closing()) return;
    state_ = S_STOPPING;
    for(auto m : threads_) pthread_kill(m->native_handle(), interruptSig_);
    LOG(TRACE, "<stop> close called; %d worker threads notified by raising signal %d", threads_.size(), interruptSig_);
}

void Manager::wait() {
    if(state_ == S_STOPPED) return;
    LOG(TRACE, "<wait> ...", 0);
    std::unique_lock<std::mutex> lk(mu_);
    while(numWorkers_ != 0) {
        cond_.wait(lk);
        LOG(TRACE, "<wait> condition returned. state_: %d", state_.load());
    }
    for(auto m : threads_) m->join();
    
    LOG(INFO, "<stop>: STOPPED", 0);
}

Manager::~Manager() {
    for(auto m : threads_) delete m;
    for(auto m : servers_) delete m;
    std::string err = "";
    closeFd(listenfd_, &err);
    if(err.size() > 0) LOG(ERROR, "%s", err);
}

void Server::workFd(int fd) {
    std::string err = "";
    LOG(TRACE, "<work_fd> delegating to handler for work on fd: %d", fd);
    hdlr_.handleFd(fd, &err);
    if(err.size() != 0) {
        LOG(ERROR, "<work>: Closing connection (fd: %d) due to error: %s", fd, err.c_str());
        removeFd(fd, &err);
    } else {
        LOG(TRACE, "<work>: DONE handling fd %d", fd);
    }
}

// bool Server::isRunQEmpty() {
//     return reqQ_.empty();
// }

// int Server::popFromRunQ() {
//     if(reqQ_.empty()) return -1;
//     int fd = reqQ_.front();
//     reqQ_.pop_front();
//     return fd;
// }

#define CHKRUNERR if(err.size() > 0) { mgr_.serverError(); LOG(ERROR, "%s", err.data()); err = ""; }

void Server::run() {
    std::string err = "";
    
    efd_ = epoll_create1(0);
    errnoChk(efd_, &err, "ERROR: creating epoll: ");
    CHKRUNERR;
    LOG(TRACE, "<run> listen fd: %d", mgr_.listenfd());
    addFd(mgr_.listenfd(), &err);
    CHKRUNERR;

    epoll_event events[MAXEVENTS];
    
    //Can't get signals to work here. don't seem to understand it well.
    //sigset_t sigmask;
    //sigemptyset(&sigmask);
    //sigaddset(&sigmask, interruptSig_);
    while(true) {
        if(mgr_.closing()) break;
        //int n = epoll_pwait(efd_, events, MAXEVENTS, pollTimeoutMs_, &sigmask);
        int n = epoll_wait(efd_, events, MAXEVENTS, mgr_.pollTimeoutMs());
        if(n == 0) { // timeout occurred
            // // timeout with no events, so try to handle some tasks on the queue (up to 10ms)
            // using namespace std::chrono;
            // auto t1 = steady_clock::now();
            // for(int fd = popFromRunQ(); fd != -1; fd = popFromRunQ()) {
            //     workFd(fd);
            //     if(duration_cast<milliseconds>(steady_clock::now() - t1) >= milliseconds(STEAL_TIME_MS)) break;
            // }
            continue;
        }            
        LOG(TRACE, "<runloop> epoll_wait returned: %d events - %s", n, (errno == 0 ? "no errors" : errnoStr().c_str()));
        if(n < 0) { // error occurred, or signal interrupted it
            // interrupted by a signal, so continue - likely shutting down
            if(errno == EINTR) {
                LOG(TRACE, "<runloop> interrupted", 0);
                continue;
            }
            err = errnoStr("ERROR: creating epoll: ");
            CHKRUNERR;
            break;
        }

        for(int i = 0; i < n; ++i) {
            auto fd = events[i].data.fd;
            auto evt = events[i].events;
            LOG(TRACE, "<runloop> i: %d, data.fd: %d, events: %d", i, fd, evt);
            bool pErr = (evt & EPOLLERR) != 0;
            bool pHup = (evt & EPOLLHUP) != 0;
            bool pRdhup = (evt & EPOLLRDHUP) != 0;
            
            if(pErr || pHup || pRdhup) {
                if(pErr || pHup) {
                    std::string s2 = std::string("|")+(pErr?"err|":"")+(pRdhup?"rdHup|":"")+(pHup?"hup|":"");
                    LOG(ERROR, "<runloop> fd: %d, epoll events: 0x%x, %s", fd, int(evt), s2.c_str());
                } else {
                    LOG(INFO, "<runloop> remote socket closed: fd: %d, epoll events: 0x%x", fd, int(evt));
                }
                if(fd != mgr_.listenfd()) removeFd(fd, &err);
                err = "";
                continue;
            }

            bool pRead = (evt & EPOLLIN) != 0;
            bool pWrite = (evt & EPOLLOUT) != 0;

            // if((events[i].events & EPOLLIN) == 0) continue;
            err = "";
            if(mgr_.listenfd() == fd) {
                if(pRead) acceptClients(false, &err); // ignore error (for now)
                CHKRUNERR;
            } else if(pRead || pWrite) {
                workFd(fd);
            }
        }        
    }
    
    for(auto it = clientfds_.begin(); it != clientfds_.end(); it++) {
        purgeFd(*it, &err);
    }    
    clientfds_.clear();
    purgeFd(mgr_.listenfd(), &err);
    closeFd(efd_, &err, "", false);
      
    mgr_.serverDone();
}

void Server::removeFd(int sockfd, std::string* err) {
    auto it = clientfds_.find(sockfd);
    if(it == clientfds_.end()) return;
    clientfds_.erase(it);
    purgeFd(sockfd, err);
}

void Server::purgeFd(int sockfd, std::string* err) {
    std::string err1(""), err2(""), err3("");
    if(epoll_ctl(efd_, EPOLL_CTL_DEL, sockfd, nullptr) == -1) {
        err1 = errnoStr("error removing sockfd on epoll: ");
    }
    if(sockfd != mgr_.listenfd()) {
        hdlr_.unregisterFd(sockfd, &err2);
        closeFd(sockfd, &err3);
    }
    if(err2.size() > 0) err1.append(" && ").append(err2);
    if(err3.size() > 0) err1.append(" && ").append(err3);
    *err = err1;
}

void Server::addFd(int sockfd, std::string* err) {
    epoll_event evt{};
    evt.data.fd = sockfd;
    evt.events = EPOLLIN | EPOLLOUT | EPOLLET; // | EPOLLHUP // EPOLLHUP is reported by default
    if(sockfd == mgr_.listenfd()) evt.events |= EPOLLEXCLUSIVE;
    else evt.events |= EPOLLRDHUP;
    if(epoll_ctl(efd_, EPOLL_CTL_ADD, sockfd, &evt) == -1) {
        *err = errnoStr("error adding sockfd on epoll: ");
        return;
    }
    if(sockfd != mgr_.listenfd()) clientfds_.insert(sockfd);
}

void Server::acceptClients(bool retryOnError, std::string* err) {
    //acceptThreadId_ = gettid();
    sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    LOG(TRACE, "<accept> waiting on accept ...", 0);
    while(true) {
        if(mgr_.closing()) break;
        int newsockfd = accept4(mgr_.listenfd(), (struct sockaddr *) &cli_addr, &cli_len, SOCK_NONBLOCK);
        if(newsockfd < 0) {
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            if(retryOnError) continue;
            std::string err2 = errnoStr("<accept>: Error accepting connection: ");
            LOG(ERROR, "%s", err2.c_str());
            *err = err2;
        } else {
            LOG(TRACE, "<Accept>: Accepted New Connection with sockfd: %d", newsockfd);
            addFd(newsockfd, err);
        }
    }
}

} //close namespace conn
} // close namespace ugorji

#endif
