# cc-common

`cc-common` contains C/C++ utilities and support functions

- support libraries for encoding formats: `simple` and [binc](https://github.com/ugorji/binc)
- connection management for servers: 
  starting, accepting client connections, managing file descriptors, etc
- utilities including big endian support, buffered read and write,
  managing collections of locks, logging, etc

It is used by [ndb](https://github.com/ugorji/ndb), simplehttpfileserver, and other C/C++ projects.

