# expect the following variables set beforehand: COMMON

SRC = $(CURDIR)
DIST = $(CURDIR)

COMMON_DIST=$(COMMON)
COMMON_SRC=$(COMMON)

CFLAGS = -std=c99 -w
CXXFLAGS = -std=c++17 -w
CPPFLAGS = -fPIC -I$(CURDIR) -I$(COMMON) -g
# Need to surround -lpthread this way, else linking libstdc++ will bomb at runtime
LDFLAGS = -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -lglog -lzstd -g
# LDFLAGS = -lpthread -lglog -lzstd -g

TESTFLAGS = -I$(SRC) -g -std=c++17 -Wall -Wextra -pthread # -isystem $(GTESTDIR)/include 

commonObjFiles = \
	$(COMMON_SRC)/ugorji/util/lockset.o \
	$(COMMON_SRC)/ugorji/util/logging.o \
	$(COMMON_SRC)/ugorji/util/bigendian.o \
	$(COMMON_SRC)/ugorji/util/slice.o \
	$(COMMON_SRC)/ugorji/util/bufio.o \
	$(COMMON_SRC)/ugorji/codec/codec.o \
	$(COMMON_SRC)/ugorji/codec/simplecodec.o \
	$(COMMON_SRC)/ugorji/codec/binc.o \
	$(COMMON_SRC)/ugorji/conn/conn_epoll.o \
	$(COMMON_SRC)/ugorji/conn/conn_mock.o \


commonTestObjFiles = \
	$(COMMON_SRC)/ugorji/util/util_test.o \
	$(COMMON_SRC)/ugorji/codec/codec_test.o \
	$(COMMON_SRC)/ugorji/codec/simplecodec_test.o \

.PHONY: clean all test check .common.all .common.clean .common.test .shlib server server.valgrind

test: .test
check: .test.run

.common.all: $(commonObjFiles)

.common.clean:
	rm -f $(COMMON_SRC)/ugorji/util/*.o $(COMMON_SRC)/ugorji/codec/*.o $(COMMON_SRC)/ugorji/conn/*.o \
		$(COMMON_DIST)/__gtest

.common.test: $(COMMON_DIST)/__gtest

.common.test.run: .test
	$(COMMON_DIST)/__gtest

%_test.o: %_test.cc
	$(CXX) $(TESTFLAGS) -c $^ -o $@

$(COMMON_DIST)/__gtest: $(commonObjFiles) $(commonTestObjFiles)
	$(CXX) $(TESTFLAGS) $^ -lgtest_main -lgtest -o $(COMMON_DIST)/__gtest

.DEFAULT:
	:

# # culled from https://stackoverflow.com/questions/714100/os-detecting-makefile
# ifeq ($(OS),Windows_NT)
#     CONN_CONN = $(COMMON_SRC)/ugorji/conn/conn_mock.o
# else
#     UNAME_S := $(shell uname -s)
#     ifeq ($(UNAME_S),Linux)
#         CONN_CONN = $(COMMON_SRC)/ugorji/conn/conn_epoll.o
# 	else
# 		CONN_CONN = $(COMMON_SRC)/ugorji/conn/conn_mock.o
#     endif
# endif

# $(CONN_CONN) \

# CXX=g++
# LDFLAGS = -lpthread -lleveldb -lglog 
# LDFLAGS = -Wl,-v -lpthread -lglog -Wl,-Bstatic -lrocksdb -Wl,-Bdynamic
# static linking bombs. Use dynamic linking.
# LDFLAGS = -static -static-libgcc -lunwind -ltcmalloc -lsnappy -lleveldb 

# .ONESHELL:

# testObjDeps = $(wildcard *_test.o)
# testExes = $(testObjDeps:.o=)

# c/c++ targets
# .%.lib.clean:
# 	rm -f $(COMMON_SRC)/ugorji/$*/{*.o,*.a,*.so*}

# .%.app.clean:
# 	rm -f $(COMMON_SRC)/app/$*/{*.o,*.a,*.so*}

# $(CXX) $(CPPFLAGS) $(CXXFLAGS) -lpthread $^ -o $@

# echo $(testObjDeps)
# echo $(testExes)

# NOTES:
# - With make, you can define a target only with pre-requisites separate from its own recipe.
#   Pre-req are evaluated bottom-to-top, left-to-right
# - When building/linking C++, order is very important
#         -o XXX [*.o] [*.c] [-lXXX]

# find $(COMMON_SRC) $(COMMON_DIST) \( -name '*.o' -o -name '*.a' -o -name '*.so' -o -name '__gtest' \) -delete # -print 
