CFLAGS=-std=gnu99 -Wall -Werror

flashfloppy_to_hfe: *.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f flashfloppy_to_hfe
.PHONY: clean