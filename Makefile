COMMON = $(CURDIR)

include $(COMMON)/Makefile.include.mk

all: .common.all

clean: .common.clean

test: .common.test

check: .common.test.run

