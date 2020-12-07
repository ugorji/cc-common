# cc-common

`cc-common` contains C/C++ utilities and support functions

- support libraries for encoding formats: `simple` and [binc](https://github.com/ugorji/binc)
- connection management for servers: 
  starting, accepting client connections, managing file descriptors, etc
- utilities including big endian support, buffered read and write,
  managing collections of locks, logging, etc

It is used by [ndb](https://github.com/ugorji/ndb) and other C/C++ projects.

# Misc - valgrind
See https://www.valgrind.org/docs/manual/mc-manual.html

Be wary of alignment, by being specific about how fields are organized in classes or structs.

Valgrind
- alignment: int = 4 bytes. Use short(16), int(32), long long (64)., size_t for pointer size, start length, etc
- double vs float

One way to run valgrind is to use:
```
	valgrind -s --track-origins=yes --leak-check=full ...
	valgrind -s --track-origins=yes --leak-check=full --show-leak-kinds=definite,possible,reachable ...
	valgrind -s --track-origins=yes --leak-check=full --show-leak-kinds=all ...
```
