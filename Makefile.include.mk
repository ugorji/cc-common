# expect the following variables set beforehand: COMMON

SRC   = $(CURDIR)
BUILD = $(CURDIR)/build

COMMON_SRC   = $(COMMON)
COMMON_BUILD = $(COMMON)/build

CFLAGS = -std=c11 # -w
CXXFLAGS = -std=c++20 -Wall # -w
CPPFLAGS = -fPIC -I$(CURDIR) -I$(COMMON) -g
# Need to surround -lpthread this way, else linking libstdc++ will bomb at runtime
LDFLAGS = -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -lglog -lzstd -g
# LDFLAGS = -lpthread -lglog -lzstd -g

TESTFLAGS = -I$(SRC) -g -std=c++17 -Wall -Wextra -pthread # -isystem $(GTESTDIR)/include 

commonObjFiles = \
	$(COMMON_BUILD)/ugorji/util/fdbuf.o \
	$(COMMON_BUILD)/ugorji/util/lockset.o \
	$(COMMON_BUILD)/ugorji/util/logging.o \
	$(COMMON_BUILD)/ugorji/util/bigendian.o \
	$(COMMON_BUILD)/ugorji/util/slice.o \
	$(COMMON_BUILD)/ugorji/util/bufio.o \
	$(COMMON_BUILD)/ugorji/codec/codec.o \
	$(COMMON_BUILD)/ugorji/codec/simplecodec.o \
	$(COMMON_BUILD)/ugorji/codec/binc.o \
	$(COMMON_BUILD)/ugorji/conn/conn_epoll.o \
	$(COMMON_BUILD)/ugorji/conn/conn_mock.o \

commonTestObjFiles = \
	$(COMMON_BUILD)/ugorji/util/util_test.o \
	$(COMMON_BUILD)/ugorji/codec/codec_test.o \
	$(COMMON_BUILD)/ugorji/codec/simplecodec_test.o \

.PHONY: clean all test check lint .common.all .common.clean .common.test .shlib server server.valgrind

#	cppcheck --enable=all $(SRC)
#	cppcheck --enable=warning,style,performance,portability,information,unusedFunction,missingInclude $(SRC)
#	echo $(CURDIR); cppcheck --suppressions-list=$(SRC)/cppcheck-suppressions.txt --enable=all $(SRC)
#
# lint:
# 	cppcheck --suppressions-list=$(SRC)/cppcheck-suppressions.txt --enable=all .

.common.all: $(commonObjFiles)

.common.clean:
	rm -f $(COMMON_BUILD)/ugorji/util/*.o $(COMMON_BUILD)/ugorji/codec/*.o $(COMMON_BUILD)/ugorji/conn/*.o \
		$(COMMON_BUILD)/__gtest

.common.test: $(COMMON_BUILD)/__gtest

.common.test.run: .common.test
	$(COMMON_BUILD)/__gtest

$(BUILD)/%_test.o: $(SRC)/%_test.cc | $(@D)
	mkdir -p $(@D) && \
	$(CXX) $(TESTFLAGS) -c $^ -o $@

$(BUILD)/%_main.o: $(SRC)/%_main.cc | $(@D)
	mkdir -p $(@D) && \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $^

$(BUILD)/%.o: $(SRC)/%.cc $(SRC)/%.h | $(@D)
	mkdir -p $(@D) && \
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $(SRC)/$*.cc

$(BUILD)/%.o: $(SRC)/%.c $(SRC)/%.h | $(@D)
	mkdir -p $(@D) && \
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $(SRC)/$*.c

$(COMMON_BUILD)/__gtest: $(commonObjFiles) $(commonTestObjFiles) | $(@D)
	mkdir -p $(@D) && \
	$(CXX) $(TESTFLAGS) $^ -lgtest_main -lgtest -o $@

.DEFAULT:
	:

# # culled from https://stackoverflow.com/questions/714100/os-detecting-makefile
# ifeq ($(OS),Windows_NT)
#     CONN_CONN = $(COMMON_BUILD)/ugorji/conn/conn_mock.o
# else
#     UNAME_S := $(shell uname -s)
#     ifeq ($(UNAME_S),Linux)
#         CONN_CONN = $(COMMON_BUILD)/ugorji/conn/conn_epoll.o
# 	else
# 		CONN_CONN = $(COMMON_BUILD)/ugorji/conn/conn_mock.o
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
# 	rm -f $(COMMON_BUILD)/ugorji/$*/{*.o,*.a,*.so*}

# .%.app.clean:
# 	rm -f $(COMMON_BUILD)/app/$*/{*.o,*.a,*.so*}

# $(CXX) $(CPPFLAGS) $(CXXFLAGS) -lpthread $^ -o $@

# echo $(testObjDeps)
# echo $(testExes)

# NOTES:
# - With make, you can define a target only with pre-requisites separate from its own recipe.
#   Pre-req are evaluated bottom-to-top, left-to-right
# - When building/linking C++, order is very important
#         -o XXX [*.o] [*.c] [-lXXX]

# find $(COMMON_SRC) $(COMMON_BUILD) \( -name '*.o' -o -name '*.a' -o -name '*.so' -o -name '__gtest' \) -delete # -print 
