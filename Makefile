SRC = $(CURDIR)
DIST = $(CURDIR)

CFLAGS = -std=c99 -w
CXXFLAGS = -std=c++11 -w
CPPFLAGS = -fPIC -I$(SRC) -g 
LDFLAGS = -lpthread -lglog -lsnappy -lbz2 -lz -lrocksdb -g

TESTFLAGS = -I$(SRC) -g -std=c++11 -Wall -Wextra -pthread # -isystem $(GTESTDIR)/include 

.PHONY: clean all .clean

objFiles = \
	$(SRC)/ugorji/util/lockset.o \
	$(SRC)/ugorji/util/logging.o \
	$(SRC)/ugorji/util/bigendian.o \
	$(SRC)/ugorji/util/slice.o \
	$(SRC)/ugorji/util/bufio.o \
	$(SRC)/ugorji/codec/codec.o \
	$(SRC)/ugorji/codec/simplecodec.o \
	$(SRC)/ugorji/codec/binc.o \
	$(SRC)/ugorji/conn/conn.o \

#	$(SRC)/ugorji/conn/conn_epoll.o \
#	$(SRC)/ugorji/conn/conn_mock.o \

testObjFiles = \
	$(SRC)/ugorji/util/util_test.o \
	$(SRC)/ugorji/codec/codec_test.o \
	$(SRC)/ugorji/codec/simplecodec_test.o \

test: .test
check: .test.run

all: $(objFiles)

clean:
	rm -f $(SRC)/ugorji/util/*.o $(SRC)/ugorji/codec/*.o $(SRC)/ugorji/conn/*.o

.test: $(DIST)/my_gtest

.test.run: .test
	$(DIST)/my_gtest

%_test.o: %_test.cc 
	$(CXX) $(TESTFLAGS) -c $^ -o $@

$(DIST)/my_gtest: $(objFiles) $(testObjFiles)
	$(CXX) $(TESTFLAGS) $^ -lgtest_main -lgtest -o $(DIST)/my_gtest

.DEFAULT:
	:

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
# 	rm -f $(SRC)/ugorji/$*/{*.o,*.a,*.so*}

# .%.app.clean:
# 	rm -f $(SRC)/app/$*/{*.o,*.a,*.so*}

# $(CXX) $(CPPFLAGS) $(CXXFLAGS) -lpthread $^ -o $@

# echo $(testObjDeps)
# echo $(testExes)

# NOTES:
# - With make, you can define a target only with pre-requisites separate from its own recipe.
#   Pre-req are evaluated bottom-to-top, left-to-right
# - When building/linking C++, order is very important
#         -o XXX [*.o] [*.c] [-lXXX]

# find $(SRC) $(DIST) \( -name '*.o' -o -name '*.a' -o -name '*.so' -o -name 'my_gtest' \) -delete # -print 
