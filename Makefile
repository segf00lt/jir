CFLAGS = -g -Wall -Wpedantic -Werror -Wno-missing-braces -Wno-parentheses -Wno-strict-prototypes -Wno-format
LDFLAGS = -lm

all:
	$(CC) $(CFLAGS) test_jir.c -o test_jir $(LDFLAGS)
debug:
	$(CC) -DDEBUG $(CFLAGS) test_jir.c -o test_jir $(LDFLAGS)

.PHONY: all debug
