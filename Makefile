CFLAGS = -g -Wall -Wpedantic -Werror
LDFLAGS = -lraylib -lm

all:
	$(CC) $(CFLAGS) jir.c -o jir $(LDFLAGS)

.PHONY: all
