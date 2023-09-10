CC = clang
CFLAGS = -g -Wall -Wpedantic -Werror
LDFLAGS = -lm

all:
	$(CC) $(CFLAGS) jir.c -o jir $(LDFLAGS)
debug:
	$(CC) -DDEBUG $(CFLAGS) jir.c -o jir $(LDFLAGS)

.PHONY: all debug
