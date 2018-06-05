BIN = overshadow
SRCS = main.c
CC = gcc
RM = rm -rf
MAKE = make
CP = cp -rf
TEST = make_test.sh
TESTDIR = ./test
DFLAGS = -pthread
ifeq ($(debug),)
	CFLAGS += -O3
else
	CFLAGS += -g -O0 -DDEBUG
endif

ifeq ($(avx),1)
	CFLAGS += -DAVX_2 -mavx2
	TESTFLAGS += avx=1
endif

ifeq ($(real),1)
	CFLAGS += -DREALTIME
endif

# performance testing args
ifeq ($(thread),1)
	MODE += -m t
else
	MODE += -m p
endif

ifneq ($(workers),)
	WORKERS += -n $(workers)
endif


$(BIN): $(SRCS)
	$(CC) $(CFLAGS) $(DFLAGS) -o $@ $^

.PHONY: clean clr install test performance
clean:
	$(RM) $(BIN) crypt_* de_*

clr:
	$(RM) crypt_* de_*

install:
	$(CP) $(BIN) /usr/local/bin/

# performance testing
F1G = f1G
PLOG = ./test/performance.log
FMT= "\nuser %U\nsystem %S\nreal %e\nCPU %P\ntext %X, data %D, max %M Kbytes\n\
inputs %I, outputs %O\npagefaults: major %F, minor %R\nswaps %W\n\n"
performance:
	$(MAKE) clr
	$(MAKE) $(TESTFLAGS)
	truncate -s 0 $(PLOG) && \
	dd bs=2M count=512 if=/dev/urandom of=./$(F1G) 2>&1 1>/dev/null | tee -a $(PLOG) && \
	time -f $(FMT) ./$(BIN) -e $(WORKERS) $(MODE) -i ./$(F1G) -o crypt_$(F1G) 2>&1 | tee -a $(PLOG) && \
	time -f $(FMT) ./$(BIN) -d $(WORKERS) $(MODE) -i ./crypt_$(F1G) -o de_$(F1G) 2>&1 | tee -a $(PLOG) && \
	diff --speed-large-files de_$(F1G) $(F1G) | tail -n 3 >> $(PLOG) 
	$(RM) ./*$(F1G)

# functional testing
test: 
	$(MAKE) clr
	$(MAKE) avx=1
	$(CP) $(BIN) $(TESTDIR)/
	$(TESTDIR)/$(TEST) $(BIN) avx_test.log
	$(MAKE) clean
	$(MAKE) 
	$(CP) $(BIN) $(TESTDIR)/
	$(TESTDIR)/$(TEST) $(BIN) test.log
