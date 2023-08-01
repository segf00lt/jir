#ifndef JLIB_BASIC_H
#define JLIB_BASIC_H

#include <stdlib.h>
#include <stdint.h>

#define IS_POW_2(x) ((x & (x-1)) == 0)
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define ARRLEN(x) (sizeof(x)/sizeof(*x))
#define STRLEN(x) ((sizeof(x)/sizeof(*x))-1)
uintptr_t align_ptr_forward(uintptr_t ptr, size_t align);

#ifdef JLIB_BASIC_IMPL

uintptr_t align_ptr_forward(uintptr_t ptr, size_t align) {
	uintptr_t p, a, mod;
	assert(IS_POW_2(align));
	p = ptr;
	a = (uintptr_t)align;
	mod = p & (a-1);
	if(mod) p += a - mod;
	return p;
}

#endif

#endif
