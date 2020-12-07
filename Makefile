COMMON = $(CURDIR)

include $(COMMON)/Makefile.include.mk

all: .common.all

clean: .common.clean

.test: $(DIST)/__gtest

.test.run: .test
	$(DIST)/__gtest

