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
	TESTFLAGS += avx=1
endif

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean clr test performance
clean:
	$(RM) $(BIN) crypt_*

clr:
	$(RM) crypt_*

# performance testing
F1G = f1G
PLOG = ./test/performance.log
FMT= "\nuser %U\nsystem %S\nreal %e\nCPU %P\ntext %X, data %D, max %M Kbytes\n\
inputs %I, outputs %O\npagefaults: major %F, minor %R\nswaps %W\n\n"
performance:
	$(MAKE) clean
	$(MAKE) $(TESTFLAGS)
	@truncate -s 0 $(PLOG) && \
	dd bs=2M count=512 if=/dev/urandom of=./$(F1G) 2>&1 1>/dev/null | tee -a $(PLOG) && \
	time -f $(FMT) ./$(BIN) -e -i ./$(F1G) -o crypt_$(F1G) 2>&1 | tee -a $(PLOG) && \
	time -f $(FMT) ./$(BIN) -d -i ./crypt_$(F1G) -o de_$(F1G) 2>&1 | tee -a $(PLOG) && \
	diff --speed-large-files de_$(F1G) $(F1G) | tail -n 3 >> $(PLOG) 
	$(RM) ./*$(F1G)

# functional testing
test: 
	$(MAKE) clean
	$(MAKE) avx=1
	$(CP) $(BIN) $(TESTDIR)/
	$(TESTDIR)/$(TEST) $(BIN) avx_test.log
	$(MAKE) clean
	$(MAKE) 
	$(CP) $(BIN) $(TESTDIR)/
	$(TESTDIR)/$(TEST) $(BIN) test.log
