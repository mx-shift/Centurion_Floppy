CFLAGS=-std=gnu99 -Wall -Werror

SRCS := $(wildcard *.c)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

flashfloppy_to_hfe: $(SRCS:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f flashfloppy_to_hfe *.o
.PHONY: clean