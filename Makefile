CFLAGS = -g -Wall -Wpedantic -Werror
LDFLAGS = -lraylib

all:
	$(CC) $(CFLAGS) jir.c -o jir $(LDFLAGS)

.PHONY: all
