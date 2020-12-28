#include "fdbuf.h"

namespace ugorji { 
namespace util { 

fdbuf::fdbuf(int fd) : fd_(-1) {
    open(fd);
}

fdbuf::~fdbuf() {
    close();
}

void fdbuf::open(int fd) {
    close();
    fd_ = fd;
    setg(inbuf_, inbuf_, inbuf_);
    setp(outbuf_, outbuf_ + bufsize - 1);
}

void fdbuf::close() {
    if(fd_ >= 0) {
        sync();
        ::close(fd_);
    }
}

int fdbuf::overflow(int c) {
    if (!traits_type::eq_int_type(c, traits_type::eof())) {
        *pptr() = traits_type::to_char_type(c);
        pbump(1);
    }
    return sync() == -1
        ? traits_type::eof()
        : traits_type::not_eof(c);
}

int fdbuf::sync() {
    if (pbase() != pptr()) {
        std::streamsize size(pptr() - pbase());
        std::streamsize done(::write(fd_, outbuf_, size));
        // The code below assumes that it is success if the stream made
        // some progress. Depending on the needs it may be more
        // reasonable to consider it a success only if it managed to
        // write the entire buffer and, e.g., loop a couple of times
        // to try achieving this success.
        if (0 < done) {
            std::copy(pbase() + done, pptr(), pbase());
            setp(pbase(), epptr());
            pbump(size - done);
        }
    }
    return pptr() != epptr()? 0: -1;
}

int fdbuf::underflow() {
    if (gptr() == egptr()) {
        std::streamsize pback(std::min(gptr() - eback(),
                                       std::ptrdiff_t(16 - sizeof(int))));
        std::copy(egptr() - pback, egptr(), eback());
        int done(::read(fd_, eback() + pback, bufsize));
        setg(eback(),
                   eback() + pback,
                   eback() + pback + std::max(0, done));
    }
    return gptr() == egptr()
        ? traits_type::eof()
        : traits_type::to_int_type(*gptr());
}

} // close namespace util
} // close namespace ugorji
