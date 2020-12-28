#pragma once

#include <algorithm>
#include <iostream>
#include <iterator>
#include <streambuf>
#include <cstddef>
#include <unistd.h>

namespace ugorji {
namespace util { 

class fdbuf : public std::streambuf {
private:
    enum { bufsize = 1024 };
    char outbuf_[bufsize];
    char inbuf_[bufsize + 16 - sizeof(int)];
    int  fd_;
public:
    typedef std::streambuf::traits_type traits_type;

    explicit fdbuf(int fd);
    ~fdbuf();
    void open(int fd);
    void close();

protected:
    int overflow(int c);
    int underflow();
    int sync();
};

}
}

