CFLAGS = -g -Wall -Wpedantic -Werror

all:
	$(CC) $(CFLAGS) jir.c -o jir

.PHONY: all
