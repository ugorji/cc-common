# cc-common

`cc-common` contains C/C++ utilities and support functions

- support libraries for encoding formats: `simple` and [binc](ugorji/binc)
- connection management for servers: 
  starting, accepting client connections, managing file descriptors, etc
- utilities including big endian support, buffered read and write,
  managing collections of locks, logging, etc

It is used by [ndb](ugorji/ndb) and other C/C++ projects.


