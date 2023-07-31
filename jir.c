#define JLIB_FMAP_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "fmap.h"

int main(int argc, char **argv) {
	Fmap fm;
	fmapopen("jir.c", O_RDONLY, &fm);
	fwrite(fm.buf, 1, fm.size, stdout);
	fmapclose(&fm);
	return 0;
}
