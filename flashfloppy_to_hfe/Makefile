CFLAGS=-std=gnu99 -Wall -Werror

LIB_SRCS := data_logger.c kv_pair.c
ALGORITHM_SRCS := $(wildcard algorithm_*.c)

BINS=flashfloppy_to_hfe kv_test

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

flashfloppy_to_hfe: main.o $(LIB_SRCS:.c=.o) $(ALGORITHM_SRCS:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

kv_test: kv_test.o $(LIB_SRCS:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(BINS) *.o
.PHONY: clean