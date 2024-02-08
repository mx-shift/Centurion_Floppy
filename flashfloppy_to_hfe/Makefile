CFLAGS=-std=gnu99 -Wall -Werror

LIB_SRCS := kv_pair.c

ALGORITHMS_SRCS := \
	fdc9216.c \
	ff_master_greaseweazle_default_pll.c \
	ff_master_greaseweazle_fallback_pll.c \
	ff_master.c \
	ff_v341.c \
	nco_v1.c

BINS=flashfloppy_to_hfe kv_test

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

flashfloppy_to_hfe: main.o $(LIB_SRCS:.c=.o) $(ALGORITHMS_SRCS:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

kv_test: kv_test.o $(LIB_SRCS:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(BINS) *.o
.PHONY: clean