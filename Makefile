CFLAGS = -g -Wall -Wpedantic -Werror -Wno-missing-braces
LDFLAGS = -lm

all:
	$(CC) $(CFLAGS) jir.c -o jir $(LDFLAGS)
debug:
	$(CC) -DDEBUG $(CFLAGS) jir.c -o jir $(LDFLAGS)

.PHONY: all debug
