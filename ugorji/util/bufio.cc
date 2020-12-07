#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <thread>

#include "bufio.h"
#include "logging.h"

namespace ugorji { 
namespace util { 

void BufReader::fill(bool continueOnInterrupt) { 
	// Read new data. Only called if all read bytes already consumed.
    // return -1 if error and read buffer empty. store it somewhere.

    //don't make errno_ sticky. Let new request to fill do it again.
    if(fd_ < 0) return;
    if(eof_) return;
    int n;
    while(true) {
        n = ::read(fd_, &buf_[w_], bufsize_-w_);
        errno_ = errno;
        if(n < 0 && errno == EINTR && continueOnInterrupt) continue;
        if(n == 0) {
            eof_ = true;
        } else if(n > 0) {
            w_ += n;
        }
        break;
    }
    return;
}

// May Return: 0 = EOF, -1 = Error. Check errno_ and eof_ for official status.
int BufReader::readn(int n, char** b, bool continueOnInterrupt) {
    if(eof_) return 0;
    if(r_ == w_) {
        r_ = w_ = 0;
        fill(continueOnInterrupt);
    }
    if(w_ == 0) {
        if(errno_ != 0) return -1;
        return 0;
    }
    if(n > (w_ - r_)) {
        n = w_ - r_;
    }
    *b = &buf_[r_];
    r_ = r_ + n;
    return n;    
}

//if return value < n, there was error, and check errno_.
//It can do retries on EAGAIN/EWOULDBLOCK based on numRetriesOnBlock parameter.
//  - retry and sleep for 10us, and double the sleep time each retry.
int BufReader::readfull(char* b, int n, bool continueOnInterrupt, int numRetriesOnBlock) {
    int r = 0;
    int bk = 0;
    auto bt = std::chrono::microseconds(10);
    while(r < n) {
        char* c;
        int i = readn(n-r, &c, continueOnInterrupt);
        if(i <= 0) {
            //eagain/ewouldblock can happen if read sent in separate packets, etc.
            //To work around temp eagain, sleep and retry.
            if(eof_) break;
            if((bk < numRetriesOnBlock) && (errno_ == EAGAIN || errno_ == EWOULDBLOCK)) {
                std::this_thread::sleep_for(bt);
                ++bk;
                bt *= 2;
                errno_ = 0;
                continue;
            }
            break;
        }
        memcpy(&b[r], c, i);
        r += i;
    }
    if(bk > 0) LOG(ERROR, "<bufio.readfull> #retries on block: %d, errno_: %d", bk, errno_);
    // if(bk > 0) std::cerr << "<readfull> #retries on block: " << bk << ", errno_: " << errno_ << std::endl;
    return r;
}

int BufWriter::flush() {
    if(fd_ < 0) return -1;
    int n = n_, w = 0;
    while(n != 0) {
        int i = ::write(fd_, &buf_[w], n);
        if(i < 0) {
            if(w != 0) ::memmove(&buf_[0], &buf_[w], n);
            n_ -= w;
            return i;
        }
        w += i;
        n -= i;
    }
    n_ = 0;
    return 0;
}

int BufWriter::writen(const char* b, int n) {
    if(fd_ < 0) return 0;
    if((n + n_) > bufsize_) {
        int w = flush();
        if(w < 0) return w;
        if(n > bufsize_) {
            w = write(fd_, b, n);
            return w;
        } 
    } 
    ::memcpy(&buf_[n_], b, n);
    n_ += n;
    return n;
}

} // end namespace util
} // end namespace ugorji
