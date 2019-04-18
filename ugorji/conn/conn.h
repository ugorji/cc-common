#ifndef _incl_ugorji_conn_conn_
#define _incl_ugorji_conn_conn_

#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <deque>
#include <set>
#include <thread>
#include <cstdint>
#include <vector>
#include <functional>

#include "../util/slice.h"
#include "../util/lockset.h"

namespace ugorji { 
namespace conn { 

class fdBufs {
public:
    std::mutex mu_;
    slice_bytes in_;
    slice_bytes out_;
    int fd_;
    fdBufs(int fd) : fd_(fd) {};
    ~fdBufs();
};

class reqFrame {
public:
    int fd_;
    slice_bytes r_;
    reqFrame(int fd, slice_bytes r) : fd_(fd), r_(r) {};
    ~reqFrame();
};

// typedef void (*req_handler)(slice_bytes in, slice_bytes* out);

class Server {
private:
    enum Status { S_INIT, S_STARTING, S_STOPPING, S_STOPPED };
    std::mutex mu_;
    std::mutex shutdownMu_;
    std::condition_variable shutdownCond_;
    std::deque<reqFrame*> reqQ_;
    std::mutex reqMu_; // handling reqQ_
    std::condition_variable reqCond_; // notified on addition to reqQ
    std::atomic<int> state_;
    std::thread* thr_;
    std::vector<std::thread*> workers_;
    std::unordered_map<int,fdBufs*> clientfds_;
    ugorji::util::LockSet locks_;
    std::function<void (slice_bytes, slice_bytes&, char**)> hdlr_;
    int portno_;
    int sockfd_;
    int numWorkers_;
    int interruptSig_;
    int efd_;
    void acceptClients(bool continueOnError, std::string* err);
    void work();
    void waitFor(bool joinThreads, bool untilStopped);
    void runLoop();
    bool shuttingDown();
    void addFd(int sockfd, std::string* err = nullptr);
    void removeFd(int sockfd, std::string* err = nullptr);
    // int popReadQ();
    void readClientFd(int fd);
    void writeClientFd(int fd, slice_bytes* sb);
public:
    void start(std::string* err);
    void stop();
    Server(int portno, int numWorkers, int interruptSig, 
           std::function<void (slice_bytes, slice_bytes&, char** err)> hdlr) : 
        portno_(portno), numWorkers_(numWorkers), hdlr_(hdlr), 
        interruptSig_(interruptSig), state_(S_INIT) { }
    ~Server();
};

}
}

#endif //_incl_ugorji_conn_conn_
