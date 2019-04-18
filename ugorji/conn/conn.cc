// For now, just use ifdefs to delegate to either epoll implementation
// on linux, or the mock no-op implementation
//
// Later on, we will change this to use libuv which is cross platform,
// and remove the epoll implementation.
//
// This is a larger project that also changes the interface conn.h, etc.

#ifdef __linux__
#include "conn_epoll.cc"
#else
#include "conn_mock.cc"
#endif
