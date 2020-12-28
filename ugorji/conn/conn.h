#pragma once

#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <map>
#include <deque>
#include <set>
#include <thread>
#include <cstdint>
#include <vector>
#include <functional>
#include <atomic>
#include <ctime>
#include <csignal>

#include <stdio.h>
#include <sys/sysinfo.h>

#include "../util/slice.h"
#include "../util/lockset.h"

namespace ugorji { 
namespace conn { 

// CONN_BUF_INCR set to 128, as most simple requests can be delivered in 256 bytes.
//
// For example, the http request from curl -v localhost:9999/s/a.txt
// can be encapsulated in less than 128 bytes, e.g.
//
//     GET /s/a.txt HTTP/1.1
//     Host: localhost:9999
//     User-Agent: curl/7.68.0
//     Accept: */*
//
// To accomodate more expansive ones, we set this to 256.
// Treat this as a default. Applications should use a preferred values for their use-case.
const size_t CONN_BUF_INCR = 256;

enum ConnState { CONN_READY, CONN_READING, CONN_PROCESSING, CONN_WRITING };

int listenport(int port, std::string& err);

// Note:
//useShutdown=true causes abort of our program
//seems these fd that are non-block must be closed w/ close (not shutdown).
void closeFd(int fd, std::string& err,
             const std::string& cat = "", bool useShutdown = false, bool logIt = true);

std::string errnoStr(const std::string& prefix = "", const std::string& suffix = "");

// typedef void (*req_handler)(slice_bytes in, slice_bytes* out);

// Handler is an abstract class which defines
// the application-specific way that it reads or writes bytes and handles requests.
//
// For example, HTTP will read the headers until it sees a blank line,
// and can then interprete the HTTP headers, and respond.
//
// On the other hand, an RPC server will read the RPC command, process it and then respond.
//
// In typical use, Handler will keep a state machine for each file descriptor,
// allowing it divide its work into stages through
// ready -> reading -> processing -> writing -> ready.
class Handler {
protected:
    bool closed_;
    bool whatever_;
public:
    explicit Handler()
        : closed_(false), whatever_(false) {}
    virtual ~Handler() {};
    virtual void handleFd(int fd, std::string& err) = 0;
    virtual void unregisterFd(int fd, std::string& err) = 0;
};

// forward declaration, so Manager can see Server.
class Server;

// Manager/Supervisor
//
// Supervises the servers and their worker threads.
// Provides helper functions for both the main function that creates it,
// and the servers.
//
// Manager can open (by listening on socket), run (start servers), and close (send signal to shut all down).
class Manager {
    // friend Server::run(); // constructors and destructors can be friends
private:
    enum Status { S_INIT, S_STARTING, S_STOPPING, S_STOPPED };

    std::atomic<int> numWorkers_;
    std::atomic<int> state_;
    std::atomic<int> numServerErrors_;
    
    std::mutex mu_;
    std::condition_variable cond_;
    
    int portno_;
    int listenfd_;
    int maxWorkers_;
    int pollTimeoutMs_;    
    int interruptSig_;

    std::vector<std::unique_ptr<std::thread>> threads_;
public:
    Manager(int portno = 9999, int maxWorkers = -1,
            int pollTimeoutMs = -1, int interruptSig = SIGUSR1) :
        numWorkers_(0),
        state_(S_INIT),
        numServerErrors_(0),
        portno_(portno),
        listenfd_(-1),
        maxWorkers_(maxWorkers),
        pollTimeoutMs_(pollTimeoutMs),
        interruptSig_(interruptSig) {
        if(maxWorkers_ < 0) maxWorkers_ = ::get_nprocs();
    }
    ~Manager();
    void open(std::string& err) { listenfd_ = listenport(portno_, err); }
    void run(std::function<Handler&()> fn, bool useThisThread);
    void wait();
    void close(); // async
    bool closing() {
        int x = state_.load();
        return (x == S_STOPPING || x == S_STOPPED);
    }
    void serverError() { numServerErrors_++; };
    void serverDone() {
        numWorkers_--;
        cond_.notify_all();
    };
    bool hasServerErrors() { return numServerErrors_ > 0; };
    int listenfd() { return listenfd_; };
    int pollTimeoutMs() { return pollTimeoutMs_; };
};

// Server
// - accepts connections (all Servers listen on the same listen socket)
// - manages connections it successfully accepted, in its own queue
// - waits for non-blocking read/write signals and delegates appropriately
//
// Benefits
// - distribution of accept (for supporting one-off connections)
// - eliminate mutexes/shared structures: reads/writes are pinned to a single thread
// - simpler code
// - no more use of pointers: instead, use references, as objects created and managed using RIAA
//
// It delegates to an application-specific handler to
// read from the socket, and write to the socket.
//
// Whenever a socket connection is accepted, it registers the socket with the
// epoll_ctl, and when the socket signals that it is ready for read or write,
// it adds that socket file-descriptor to a task queue.
//
// The application-specific handler should be designed around a state machine,
// so that it can 4 independent phases: ready -> reading -> processing -> writing -> ready.
// This will allow efficient responding to IO signals without blocking, as you can just
// read and buffer, until you read the request fully, even over multiple calls.
// And you can do the same with writing.
//
// To have a server make one worker per CPU core, pass maxWorkers = -1
class Server {
private:
    std::set<int> clientfds_;

    // std::deque<int> reqQ_;
    Manager& mgr_;
    Handler& hdlr_;
    int efd_;
    
    void acceptClients(bool retryOnError, std::string& err);
    void addFd(int sockfd, std::string& err);
    void removeFd(int sockfd, std::string& err);
    void purgeFd(int sockfd, std::string& err);
    void workFd(int fd);
    // bool isRunQEmpty();
    // int popFromRunQ();
public:
    Server(Handler& hdlr, Manager& mgr) : 
        mgr_(mgr),
        hdlr_(hdlr),
        efd_(-1) {
    }
    ~Server() {};
    void run();
};

}
}

