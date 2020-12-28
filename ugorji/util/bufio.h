#pragma once

namespace ugorji { 
namespace util { 

class BufReader {
private:
    void fill(bool continueOnInterrupt);
    enum { bufsize_ = 4096 };
    char buf_[bufsize_] = {};
    int fd_ = -1;
    int r_ = 0;
    int w_ = 0;
public:
    int errno_ = 0; // last errno
    bool eof_ = false;
    int readn(int n, char** b, bool continueOnInterrupt); 
    //numRetriesOnBlock=18 means we try for up to 10*2^18 seconds (=5 seconds).
    int readfull(char* b, int n, bool continueOnInterrupt, int numRetriesOnBlock = 18); 
    void reset(int fd) { fd_ = fd; errno_ = r_ = w_ = 0; eof_ = false; }
    BufReader() {}
    explicit BufReader(int fd) { reset(fd); }
};

class BufWriter {
private:
    enum { bufsize_ = 4096 };
    int fd_ = -1;
    int n_ = 0;
    char buf_[bufsize_] = {};
public:
    int flush();
    int writen(const char* b, int n);
    void reset(int fd) { fd_ = fd; n_ = 0; }
    BufWriter() { }
    explicit BufWriter(int fd) { reset(fd); }
};

} // end namespace util
} // end namespace ugorji

