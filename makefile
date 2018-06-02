BIN = overshadow
SRCS = main.c
CC = gcc
RM = rm -rf
MAKE = make
CP = cp -rf
TEST = make_test.sh
TESTDIR = ./test

ifeq ($(debug),)
	CFLAGS += -O3
else
	CFLAGS += -g -O0
endif

ifeq ($(avx),1)
	CFLAGS += -DAVX_2 -mavx2
endif

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean clr test
clean:
	$(RM) $(BIN) crypt_*

clr:
	$(RM) crypt_*

test: 
	$(MAKE) clean
	$(MAKE) avx=1
	$(CP) $(BIN) $(TESTDIR)/
	$(TESTDIR)/$(TEST) $(BIN) avx_test.log
	$(MAKE) clean
	$(MAKE) 
	$(CP) $(BIN) $(TESTDIR)/
	$(TESTDIR)/$(TEST) $(BIN) test.log
