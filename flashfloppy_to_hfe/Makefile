CFLAGS=-std=gnu99 -Wall -Werror

LIB_SRCS := kv_pair.c

ALGORITHMS_SRCS := \
	fdc9216.c \
	ff_master_greaseweazle_default_pll.c \
	ff_master_greaseweazle_fallback_pll.c \
	ff_master.c \
	ff_v341.c \
	nco_178k.c \
	nco_358k.c \
	nco_715k.c \
	nco_1440k_0p2.c \
	nco_1440k_0p25.c \
	nco_2160k_0p1.c \
	nco_2160k_0p2.c \
	nco_2160k_0p5.c \
	nco_2160k_0p25.c \
	nco_2160k_1p0.c \
	nco_generic.c

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