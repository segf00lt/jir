#ifndef JLIB_BASIC_H
#define JLIB_BASIC_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define IS_POW_2(x) ((x & (x-1)) == 0)
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define ARRLEN(x) (sizeof(x)/sizeof(*x))
#define STRLEN(x) ((sizeof(x)/sizeof(*x))-1)
typedef int64_t s64;
typedef uint64_t u64;
typedef int32_t s32;
typedef uint32_t u32;
typedef int16_t s16;
typedef uint16_t u16;
typedef int8_t s8;
typedef uint8_t u8;
typedef float f32;
typedef double f64;
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
