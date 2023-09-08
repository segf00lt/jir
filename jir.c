#define JLIB_FMAP_IMPL
#define STB_SPRINTF_IMPLEMENTATION
#define STB_DS_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <error.h>
#include <raylib.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include "fmap.h"
#include "basic.h"
#include "stb_sprintf.h"
#include "stb_ds.h"

#undef sprintf
#undef snprintf
#define sprintf stbsp_sprintf
#define snprintf stbsp_snprintf

#define OPCODES         \
	X(AND)          \
	X(OR)           \
	X(XOR)          \
	X(LSHIFT)       \
	X(RSHIFT)       \
	X(NOT)          \
	X(ADD)          \
	X(SUB)          \
	X(MUL)          \
	X(DIV)          \
	X(MOD)          \
	X(NEG)          \
	X(EQ)           \
	X(NE)           \
	X(LE)           \
	X(GT)           \
	X(LT)           \
	X(GE)           \
	X(FADD)         \
	X(FSUB)         \
	X(FMUL)         \
	X(FDIV)         \
	X(FMOD)         \
	X(FNEG)         \
	X(FEQ)          \
	X(FNE)          \
	X(FLE)          \
	X(FGT)          \
	X(FLT)          \
	X(FGE)          \
	X(BRANCH)       \
	X(JMP)          \
	X(LOAD)         \
	X(STOR)         \
	X(VAL)          \
	X(MOVE)         \
	X(BFRAME)       \
	X(EFRAME)       \
	X(ALLOC)        \
	X(CALL)         \
	X(RET)          \
	X(SETARG)       \
	X(GETARG)       \
	X(SETRET)       \
	X(GETRET)       \
	X(SETPORT)      \
	X(GETPORT)      \
	X(DUMPREG)      \
	X(IOREAD)       \
	X(IOWRITE)      \
	X(IOOPEN)       \
	X(IOCLOSE)      \
	X(MEMSET)       \
	X(HEAPALLOC)    \
	X(HEAPFREE)     \
	X(BITCAST)      \
	X(TYPECAST)     \
	X(DUMPREGRANGE) \
	X(DUMPMEM)      \
	X(DUMPMEMRANGE) \
	X(HALT)

/* TODO
 *
 * IOREAD
 * IOWRITE
 * IOOPEN
 * IOCLOSE
 * HEAPALLOC
 * HEAPFREE
 * MEMSET
 * DUMPREGRANGE
 * DUMPMEM
 * DUMPMEMRANGE
 *
 * interpreter backtrace
 */

#define TYPES         \
	X(S64)        \
	X(S32)        \
	X(S16)        \
	X(S8)         \
	X(U64)        \
	X(U32)        \
	X(U16)        \
	X(U8)         \
	X(PTR_LOCAL)  \
	X(PTR_GLOBAL) \
	X(PTR_HEAP)   \
	X(F32)        \
	X(F64)

size_t TYPE_SIZES[] = {
	8, // s64
	8, // u64
	4, // s32
	4, // u32
	2, // s16
	2, // u16
	1, // s8
	1, // u8
	8,8,8, // ptr
	4, // f32
	8, // f64
};

#define SEGMENTS   \
	X(LOCAL)   \
	X(GLOBAL)  \
	X(HEAP)

#define TOKEN_TO_OP(t) (JIROP)(t - TOKEN_AND)
#define TOKEN_TO_TYPE(t) (JIRTYPE)((int)t - (int)TOKEN_S64)
#define TOKEN_TO_SEGMENT(t) (JIRSEG)(t - TOKEN_LOCAL)

#define SIGN_EXTEND(x, l) (-(((x >> (l-1)) << l) - x))

enum JIROP {
#define X(val) JIROP_##val,
	OPCODES
#undef X
};
typedef enum JIROP JIROP;

enum JIRTYPE {
#define X(val) JIRTYPE_##val,
	TYPES
#undef X
};
typedef enum JIRTYPE JIRTYPE;

enum JIRSEG {
#define X(val) JIRSEG_##val,
	SEGMENTS
#undef X
};
typedef enum JIRSEG JIRSEG;

const char *opcodesDebug[] = {
#define X(val) #val,
	OPCODES
#undef X
};

const char *typesDebug[] = {
#define X(val) #val,
	TYPES
#undef X
};

const char *segmentsDebug[] = {
#define X(val) #val,
	SEGMENTS
#undef X
};

struct JIR {
	JIROP opcode;
	JIRTYPE type_a, type_b, type_c;
	JIRSEG seg;
	// NOTE the b index is only used in binary ops
	// meaning: 'not', 'neg' and 'fneg' only use 'a' and 'c'
	bool immediate;
	int a, b;
	union {
		int c;
		u64 imm_u64;
		s64 imm_s64;
		float imm_f32;
		double imm_f64;
		u64 procid;
	};
};
typedef struct JIR JIR;

struct JIRIMAGE {
	Arr(JIR) *proctab;
	u8 *global;
	// ports are special registers for passing data between procedures, and in and out of the interpreter
	JIRTYPE *port_types;
	u64 *port_u64;
	u64 *port_ptr_local;
	u64 *port_ptr_global;
	u8* *port_ptr_heap;
	float *port_f32;
	double *port_f64;
};
typedef struct JIRIMAGE JIRIMAGE;

#include "lex.c"

void parse(Lexer *l, Arr(JIR) *instarr) {
	char debugBuf[64];
	JIR inst;
	Token token;
	unsigned long index = 0;

	//assert(*instarr == NULL);

	/*
	 * grammar
	 *
	 * binop type dest_reg a_reg b_reg
	 * binop "imm" type dest_reg reg imm
	 * unop type dest_reg reg
	 * castop type cast_type dest_reg reg
	 * branch cond_reg offset_reg
	 * branch "imm" cond_reg offset_imm
	 * jmp offset_reg
	 * jmp "imm" offset_imm
	 * load segment type dest_reg addr_reg
	 * stor segment type dest_addr_reg val_reg
	 * load "imm" type dest_reg imm
	 * stor "imm" segment type dest_addr_reg imm
	 * val type reg imm
	 * move type dest_reg src_reg
	 * memset segment type start_reg count_reg val_imm
	 * alloc type
	 * alloc immediate imm_bytes
	 * heapalloc ptr_dest_reg size_reg
	 * heapalloc "imm" ptr_dest_reg size_imm
	 * heapfree ptr_reg
	 * call procid_reg
	 * call "imm" funcid_imm
	 * setport type arg_reg src_reg
	 * setport "imm" type arg_reg val_imm
	 * getport type dest_reg arg_index
	 * ret
	 * halt
	 */

	while(!l->eof) {
		lex(l);
		token = l->token;
		inst = (JIR){0};
		switch(token) {
		default:
			snprintf(debugBuf, l->debugInfo.end-l->debugInfo.start, "%s", l->debugInfo.start);
			error(1, 0, "unrecognized token %s at line %zu, col %zu\n>\t%s", 
					tokenDebug[l->token-TOKEN_INVALID], l->debugInfo.line, l->debugInfo.col, debugBuf);
			break;
		case TOKEN_TYPECAST:
		case TOKEN_BITCAST:
			inst.opcode = TOKEN_TO_OP(token);
			lex(l);
			assert(l->token >= TOKEN_S64 && l->token <= TOKEN_F64);
			inst.type_a = TOKEN_TO_TYPE(l->token);
			lex(l);
			assert(l->token >= TOKEN_S64 && l->token <= TOKEN_F64);
			inst.type_b = TOKEN_TO_TYPE(l->token);
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.a = atoi(l->start + 1);
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.c = atoi(l->start + 1);
			arrput(*instarr, inst);
			break;
		case TOKEN_HALT: // NOTE redundant?
		case TOKEN_RET:
		case TOKEN_BFRAME: case TOKEN_EFRAME:
			inst.opcode = TOKEN_TO_OP(token);
			arrput(*instarr, inst);
			break;
		case TOKEN_CALL:
			inst.opcode = JIROP_CALL;
			lex(l);
			if(l->token == TOKEN_IMM) {
				inst.immediate = true;
				lex(l);
				assert(l->token >= TOKEN_BINLIT && l->token <= TOKEN_INTLIT);
				switch(l->token) {
				case TOKEN_BINLIT:
					assert(0);
					break;
				case TOKEN_HEXLIT:
					sscanf(l->start, "%lx", &inst.procid);
					break;
				case TOKEN_OCTLIT:
					sscanf(l->start, "%lo", &inst.procid);
					break;
				case TOKEN_INTLIT:
					sscanf(l->start, "%lu", &inst.procid);
					break;
				default:
					assert(0);
					break;
				}
			} else {
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				inst.a = atoi(l->start + 1);
			}
			arrput(*instarr, inst);
			break;
		case TOKEN_DUMPREG:
			inst.opcode = JIROP_DUMPREG;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;
			arrput(*instarr, inst);
			break;
		case TOKEN_ADD: case TOKEN_FADD:
		case TOKEN_SUB: case TOKEN_FSUB:
		case TOKEN_MUL: case TOKEN_FMUL:
		case TOKEN_DIV: case TOKEN_FDIV:
		case TOKEN_MOD: case TOKEN_FMOD:
		case TOKEN_EQ: case TOKEN_NE: case TOKEN_LE: case TOKEN_GT:
		case TOKEN_LT: case TOKEN_GE:
		case TOKEN_FEQ: case TOKEN_FNE: case TOKEN_FLE: case TOKEN_FGT:
		case TOKEN_FLT: case TOKEN_FGE:
		case TOKEN_AND: case TOKEN_OR: case TOKEN_XOR:
		case TOKEN_LSHIFT: case TOKEN_RSHIFT:
			inst.opcode = TOKEN_TO_OP(token);
			lex(l);
			if(l->token == TOKEN_IMM) {
				inst.immediate = true;
				lex(l);
			}
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
			}

			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;

			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.b = index;
			
			if(inst.immediate) {
				lex(l);
				switch(l->token) {
				case TOKEN_BINLIT:
					assert(0);
					break;
				case TOKEN_HEXLIT:
					sscanf(l->start, "%lx", &inst.imm_u64);
					break;
				case TOKEN_OCTLIT:
					sscanf(l->start, "%lo", &inst.imm_u64);
					break;
				case TOKEN_INTLIT:
					sscanf(l->start, "%li", (s64*)&inst.imm_u64);
					break;
				case TOKEN_FLOATLIT:
					if(inst.type_a == JIRTYPE_F64)
						sscanf(l->start, "%lf", &inst.imm_f64);
					else
						sscanf(l->start, "%f", &inst.imm_f32);
					break;
				default:
					assert(0);
					break;
				}
			} else {
				lex(l);
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				index = atoi(l->start + 1);
				inst.c = index;
			}

			arrput(*instarr, inst);
			break;
		case TOKEN_NOT: case TOKEN_NEG: case TOKEN_FNEG:
			inst.opcode = TOKEN_TO_OP(token);
			lex(l);
			if(l->token == TOKEN_IMM) {
				inst.immediate = true;
				lex(l);
			}
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
			}

			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;

			if(inst.immediate) {
				lex(l);
				switch(l->token) {
				case TOKEN_BINLIT:
					assert(0);
					break;
				case TOKEN_HEXLIT:
					sscanf(l->start, "%lx", &inst.imm_u64);
					break;
				case TOKEN_OCTLIT:
					sscanf(l->start, "%lo", &inst.imm_u64);
					break;
				case TOKEN_INTLIT:
					sscanf(l->start, "%li", (s64*)&inst.imm_u64);
					break;
				case TOKEN_FLOATLIT:
					if(inst.type_a == JIRTYPE_F64)
						sscanf(l->start, "%lf", &inst.imm_f64);
					else
						sscanf(l->start, "%f", &inst.imm_f32);
					break;
				default:
					assert(0);
					break;
				}
			} else {
				lex(l);
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				index = atoi(l->start + 1);
				inst.c = index;
			}

			arrput(*instarr, inst);
			break;
		case TOKEN_BRANCH:
			inst.opcode = JIROP_BRANCH;
			lex(l);
			if(l->token == TOKEN_IMM) {
				inst.immediate = true;
				lex(l);
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				inst.a = atoi(l->start + 1);
				lex(l);
				assert(l->token == TOKEN_INTLIT);
				sscanf(l->start, "%li", (s64*)&inst.imm_u64);
			} else {
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				inst.a = atoi(l->start + 1);
				lex(l);
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				inst.a = atoi(l->start + 1);
			}
			arrput(*instarr, inst);
			break;     
		case TOKEN_JMP:
			inst.opcode = JIROP_JMP;
			lex(l);
			if(l->token == TOKEN_IMM) {
				inst.immediate = true;
				lex(l);
				assert(l->token == TOKEN_INTLIT);
				sscanf(l->start, "%li", (s64*)&inst.imm_u64);
			} else {
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				inst.a = atoi(l->start + 1);
			}
			arrput(*instarr, inst);
			break;        
		case TOKEN_LOAD: case TOKEN_STOR:
			inst.opcode = TOKEN_TO_OP(token);
			lex(l);
			assert(l->token >= TOKEN_LOCAL && l->token <= TOKEN_HEAP);
			inst.seg = TOKEN_TO_SEGMENT(l->token);
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.a = atoi(l->start + 1);
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.b = atoi(l->start + 1);
			arrput(*instarr, inst);
			break;       
		case TOKEN_ALLOC:
			inst.opcode = JIROP_ALLOC;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				inst.a = atoi(l->start + 1);
			} else if(l->token == TOKEN_IMM) {
				lex(l);
				assert(l->token >= TOKEN_BINLIT && l->token <= TOKEN_INTLIT);
				switch(l->token) {
				case TOKEN_BINLIT:
					assert(0);
					break;
				case TOKEN_HEXLIT:
					sscanf(l->start, "%lx", &inst.imm_u64);
					break;
				case TOKEN_OCTLIT:
					sscanf(l->start, "%lo", &inst.imm_u64);
					break;
				case TOKEN_INTLIT:
					sscanf(l->start, "%lu", &inst.imm_u64);
					break;
				default:
					assert(0);
					break;
				}
			}
			arrput(*instarr, inst);
			break;
		case TOKEN_HEAPALLOC:
			inst.opcode = JIROP_HEAPALLOC;
			break;
		case TOKEN_HEAPFREE:
			break;
		case TOKEN_MEMSET:
			break;
		case TOKEN_VAL:
			inst.opcode = JIROP_VAL;
			inst.immediate = true;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;
			lex(l);
			assert(l->token >= TOKEN_BINLIT && l->token <= TOKEN_FLOATLIT);
			switch(l->token) {
			case TOKEN_BINLIT:
				assert(0);
				break;
			case TOKEN_HEXLIT:
				sscanf(l->start, "%lx", &inst.imm_u64);
				break;
			case TOKEN_OCTLIT:
				sscanf(l->start, "%lo", &inst.imm_u64);
				break;
			case TOKEN_INTLIT:
				sscanf(l->start, "%li", (s64*)&inst.imm_u64);
				break;
			case TOKEN_FLOATLIT:
				if(inst.type_a == JIRTYPE_F64)
					sscanf(l->start, "%lf", &inst.imm_f64);
				else
					sscanf(l->start, "%f", &inst.imm_f32);
				break;
			default:
				assert(0);
				break;
			}
			arrput(*instarr, inst);
			break;        
		case TOKEN_MOVE:
			inst.opcode = JIROP_MOVE;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.c = index;
			arrput(*instarr, inst);
			break;        

		case TOKEN_IOREAD:
			inst.opcode = JIROP_IOREAD;
			inst.type_a = JIRTYPE_S64;
			inst.type_c = JIRTYPE_U64;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;
			lex(l);
			assert("expects pointer type"&&l->token >= TOKEN_PTR_LOCAL && l->token <= TOKEN_PTR_HEAP);
			inst.type_b = TOKEN_TO_TYPE(l->token);
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.b = index;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.c = index;
			arrput(*instarr, inst);
			break;
		case TOKEN_IOWRITE:
			inst.opcode = JIROP_IOWRITE;
			inst.type_a = JIRTYPE_U64;
			inst.type_c = JIRTYPE_U64;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;
			lex(l);
			assert("expects pointer type"&&l->token >= TOKEN_PTR_LOCAL && l->token <= TOKEN_PTR_HEAP);
			inst.type_b = TOKEN_TO_TYPE(l->token);
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.b = index;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.c = index;
			arrput(*instarr, inst);
			break;
		case TOKEN_IOOPEN:
			inst.opcode = JIROP_IOOPEN;
			inst.type_b = JIRTYPE_S64;
			inst.type_c = JIRTYPE_S64;
			lex(l);
			assert("expects pointer type"&&l->token >= TOKEN_PTR_LOCAL && l->token <= TOKEN_PTR_HEAP);
			inst.type_a = TOKEN_TO_TYPE(l->token);
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.b = index;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.c = index;
			arrput(*instarr, inst);
			break;
		case TOKEN_IOCLOSE:
			inst.opcode = JIROP_IOCLOSE;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			index = atoi(l->start + 1);
			inst.a = index;
			arrput(*instarr, inst);
			break;

		case TOKEN_SETPORT:
		case TOKEN_SETARG:
		case TOKEN_SETRET:
			inst.opcode = TOKEN_TO_OP(token);
			lex(l);
			if(l->token == TOKEN_IMM) {
				inst.immediate = true;
				lex(l);
			}
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.a = atoi(l->start + 1);
			lex(l);
			if(inst.immediate) {
				assert(l->token >= TOKEN_BINLIT && l->token <= TOKEN_FLOATLIT);
				switch(l->token) {
				case TOKEN_BINLIT:
					assert(0);
					break;
				case TOKEN_HEXLIT:
					sscanf(l->start, "%lx", &inst.imm_u64);
					break;
				case TOKEN_OCTLIT:
					sscanf(l->start, "%lo", &inst.imm_u64);
					break;
				case TOKEN_INTLIT:
					sscanf(l->start, "%li", (s64*)&inst.imm_u64);
					break;
				case TOKEN_FLOATLIT:
					sscanf(l->start, "%f", &inst.imm_f32);
					break;
				default:
					assert(0);
					break;
				}
			} else {
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				inst.c = atoi(l->start + 1);
			}
			arrput(*instarr, inst);
			break;
		case TOKEN_GETPORT:
		case TOKEN_GETARG:
		case TOKEN_GETRET:
			inst.opcode = TOKEN_TO_OP(token);
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type_a = TOKEN_TO_TYPE(l->token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.a = atoi(l->start + 1);
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.c = atoi(l->start + 1);
			arrput(*instarr, inst);
			break;
		case TOKEN_INVALID:
			break;
		}
	}
}

void JIR_print(JIR inst) {
	printf("opcode: %s\ntype: %s\ntype2: %s\na: %i\nb: %i\nc: %i\nimmediate: %i\nimm_u64: %lu\nimm_s64: %li\nimm_f32: %f\nimm_f64: %lf\nprocid: %lu\n",
	opcodesDebug[inst.opcode],
	typesDebug[inst.type_a],
	typesDebug[inst.type_b],
	inst.a, inst.b, inst.c,
	inst.immediate,
	inst.imm_u64,
	inst.imm_s64,
	inst.imm_f32,
	inst.imm_f64,
	inst.procid);
}

void JIR_exec(JIRIMAGE image) {

	u64 reg[32] = {0};
	u64 reg_ptr_local[32] = {0};
	u64 reg_ptr_global[32] = {0};
	u8* reg_ptr_heap[32] = {0};
	float reg_f32[32] = {0};
	double reg_f64[32] = {0};

	//s64 *port_s64 = image.port_s64;
	JIRTYPE *port_types = image.port_types;
	u64 *port_u64 = image.port_u64;
	u64 *port_ptr_local = image.port_ptr_local;
	u64 *port_ptr_global = image.port_ptr_global;
	u8* *port_ptr_heap = image.port_ptr_heap;
	float *port_f32 = image.port_f32;
	double *port_f64 = image.port_f64;

	u64 iarithMasks[] = {
		0xffffffffffffffff, // S64
		0xffffffff, // S32
		0xffff, // S16
		0xff, // S8
		0xffffffffffffffff, // U64
		0xffffffff, // U32
		0xffff, // U16
		0xff, // U8
		0xffffffffffffffff, // PTR_LOCAL
		0xffffffffffffffff, // PTR_GLOBAL
		0xffffffffffffffff, // PTR_HEAP
		0x0, // F32
		0x0, // F64
	};

	u64 iarithBits[] = {
		64, 32, 16, 8, 
		64, 32, 16, 8, 
		64, // PTR_LOCAL
		64, // PTR_GLOBAL
		64, // PTR_HEAP
		0, // F32
		0, // F64
	};

	u8 *global = image.global;
	u8 *local = calloc(0x1000, sizeof(u8));
	long *localbase = calloc(0x80, sizeof(long));
	long local_pos = 0;
	long localbase_pos = 0;

	u8 *ptr = NULL;
	u8 *segTable[] = { local, global, NULL };
	u64 *ptrTable[] = { reg_ptr_local, reg_ptr_global, NULL };

	u64 procidStack[64] = {0};
	u64 pcStack[64] = {0};
	u64 calldepth = 0;


	bool run = true;
	bool dojump = false;
	bool issignedint = false;
	u64 procid = 0;
	u64 pc = 0;
	
	JIR *proc = image.proctab[procid];

	while(pc < arrlen(proc)) {
		JIR inst = proc[pc];
		u64 newpc = pc + 1;
		u64 imask = iarithMasks[inst.type_a];
		u64 ileft = reg[inst.b] & imask;
		u64 ptr_local_right = inst.immediate ? inst.imm_u64 : reg_ptr_local[inst.c];
		u64 ptr_global_right = inst.immediate ? inst.imm_u64 : reg_ptr_global[inst.c];
		u64 iright = (inst.immediate ? inst.imm_u64 : reg[inst.c]) & imask;
		f32 f32right = inst.immediate ? inst.imm_f32 : reg_f32[inst.c];
		f64 f64right = inst.immediate ? inst.imm_f64 : reg_f64[inst.c];
		bool type_a_int = (inst.type_a >= JIRTYPE_S64 && inst.type_a <= JIRTYPE_U8);
		bool type_c_int = (inst.type_b >= JIRTYPE_S64 && inst.type_b <= JIRTYPE_U8);
		bool type_a_f32 = (inst.type_a == JIRTYPE_F32);
		bool type_c_f32 = (inst.type_b == JIRTYPE_F32);
		bool type_a_f64 = (inst.type_a == JIRTYPE_F64);
		bool type_c_f64 = (inst.type_b == JIRTYPE_F64);
		bool type_a_ptr_local = (inst.type_a == JIRTYPE_PTR_LOCAL);
		bool type_c_ptr_local = (inst.type_b == JIRTYPE_PTR_LOCAL);
		bool type_a_ptr_global = (inst.type_a == JIRTYPE_PTR_GLOBAL);
		bool type_c_ptr_global = (inst.type_b == JIRTYPE_PTR_GLOBAL);
		bool type_a_ptr_heap = (inst.type_a == JIRTYPE_PTR_HEAP);
		bool type_c_ptr_heap = (inst.type_b == JIRTYPE_PTR_HEAP);

		issignedint = (inst.type_a >= JIRTYPE_S64 && inst.type_a <= JIRTYPE_S8);
		if(issignedint) {
			ileft = SIGN_EXTEND(ileft, iarithBits[inst.type_a]);
			iright = SIGN_EXTEND(iright, iarithBits[inst.type_a]);
		}

		switch(inst.opcode) {
		default:
			printf("opcode %s unimplemeted\n", opcodesDebug[inst.opcode]);
			assert(0);
			break;
		case JIROP_DUMPREG:
			if(inst.type_a == JIRTYPE_F32)
				printf("f32 reg %i: %f\n", inst.a, reg_f32[inst.a]);
			else if(inst.type_a == JIRTYPE_F64)
				printf("f64 reg %i: %g\n", inst.a, reg_f64[inst.a]);
			else
				printf("reg %i: 0x%lx\n", inst.a, reg[inst.a]);
			break;

		case JIROP_BITCAST:
			if(type_a_int && type_c_int) {
				reg[inst.a] = reg[inst.c] & iarithMasks[inst.type_b];
			} else if(type_a_f64 && type_c_int) {
				iright = reg[inst.c] & iarithMasks[inst.type_b];
				reg_f64[inst.a] = *(double*)&iright;
			} else if(type_a_f32 && type_c_int) {
				iright = reg[inst.c] & iarithMasks[inst.type_b];
				reg_f32[inst.a] = *(float*)&iright;
			} else if(type_a_int && type_c_f64) {
				reg[inst.a] = *(u64*)(reg_f64 + inst.c) & iarithMasks[inst.type_a];
			} else if(type_a_int && type_c_f32) {
				reg[inst.a] = *(u64*)(reg_f32 + inst.c) & iarithMasks[inst.type_a];
			} else if(type_a_f32 && type_c_f64) {
				reg_f32[inst.a] = *(float*)(reg_f64 + inst.c);
			} else if(type_a_f64 && type_c_f32) {
				reg_f64[inst.a] = *(double*)(reg_f32 + inst.c);
			// pointers
			} else if(type_a_ptr_local && type_c_int) {
				reg_ptr_local[inst.a] = reg[inst.c] & iarithMasks[inst.type_b];
			} else if(type_a_ptr_global && type_c_int) {
				reg_ptr_global[inst.a] = reg[inst.c] & iarithMasks[inst.type_b];
			} else if(type_a_ptr_heap && type_c_int) {
				reg_ptr_heap[inst.a] = (u8*)(reg[inst.c] & iarithMasks[inst.type_b]);
			} else if(type_a_int && type_c_ptr_local) {
				reg[inst.a] = reg_ptr_local[inst.c] & iarithMasks[inst.type_a];
			} else if(type_a_int && type_c_ptr_global) {
				reg[inst.a] = reg_ptr_global[inst.c] & iarithMasks[inst.type_a];
			} else if(type_a_int && type_c_ptr_heap) {
				reg[inst.a] = (u64)(reg_ptr_heap[inst.c]) & iarithMasks[inst.type_a];
			} else {
				assert("illegal bitcast" && 0);
			}
			break;

		case JIROP_TYPECAST:
			if(type_a_int && type_c_int) {
				reg[inst.a] = reg[inst.c] & iarithMasks[inst.type_b];
			} else if(type_a_f64 && type_c_int) {
				reg_f64[inst.a] = (double)(reg[inst.c] & iarithMasks[inst.type_b]);
			} else if(type_a_f32 && type_c_int) {
				reg_f32[inst.a] = (float)(reg[inst.c] & iarithMasks[inst.type_b]);
			} else if(type_a_int && type_c_f64) {
				reg[inst.a] = (s64)reg_f64[inst.c] & iarithMasks[inst.type_a];
			} else if(type_a_int && type_c_f32) {
				reg[inst.a] = (s64)reg_f32[inst.c] & iarithMasks[inst.type_a];
			} else if(type_a_f32 && type_c_f64) {
				reg_f32[inst.a] = (float)reg_f64[inst.c];
			} else if(type_a_f64 && type_c_f32) {
				reg_f64[inst.a] = (double)reg_f32[inst.c];
			// pointers
			} else if(type_a_ptr_local && type_c_int) {
				reg_ptr_local[inst.a] = reg[inst.c] & iarithMasks[inst.type_b];
			} else if(type_a_ptr_global && type_c_int) {
				reg_ptr_global[inst.a] = reg[inst.c] & iarithMasks[inst.type_b];
			} else if(type_a_ptr_heap && type_c_int) {
				reg_ptr_heap[inst.a] = (u8*)(reg[inst.c] & iarithMasks[inst.type_b]);
			} else if(type_a_int && type_c_ptr_local) {
				reg[inst.a] = reg_ptr_local[inst.c] & iarithMasks[inst.type_a];
			} else if(type_a_int && type_c_ptr_global) {
				reg[inst.a] = reg_ptr_global[inst.c] & iarithMasks[inst.type_a];
			} else if(type_a_int && type_c_ptr_heap) {
				reg[inst.a] = (u64)(reg_ptr_heap[inst.c]) & iarithMasks[inst.type_a];
			} else {
				assert("illegal bitcast" && 0);
			}
			break;

		case JIROP_NOT:
			reg[inst.a] = ~iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_NEG:
			reg[inst.a] = -iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_FNEG:
			assert(inst.type_a == JIRTYPE_F64 || inst.type_a == JIRTYPE_F32);
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = -f64right;
			else
				reg_f32[inst.a] = -f32right;
			break;

		case JIROP_ADD:
			reg[inst.a] = ileft + iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_SUB:
			reg[inst.a] = ileft - iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_MUL:
			reg[inst.a] = ileft * iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_DIV:
			reg[inst.a] = ileft / iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_MOD:
			reg[inst.a] = ileft % iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_AND:
			reg[inst.a] = ileft & iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_OR:
			reg[inst.a] = ileft | iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_XOR:
			reg[inst.a] = ileft ^ iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_LSHIFT:
			reg[inst.a] = ileft << iright;
			reg[inst.a] &= imask;
			break;
		case JIROP_RSHIFT:
			reg[inst.a] = ileft >> iright;
			reg[inst.a] &= imask;
			break;

		case JIROP_EQ:
			reg[inst.a] = (bool)(ileft == iright);
			break;
		case JIROP_NE:
			reg[inst.a] = (bool)(ileft != iright);
			break;
		case JIROP_LE:
			if(issignedint)
				reg[inst.a] = (bool)((s64)ileft <= (s64)iright);
			else
				reg[inst.a] = ileft <= iright;
			break;
		case JIROP_GT:
			if(issignedint)
				reg[inst.a] = (bool)((s64)ileft > (s64)iright);
			else
				reg[inst.a] = ileft > iright;
			break;
		case JIROP_LT:
			if(issignedint)
				reg[inst.a] = (bool)((s64)ileft < (s64)iright);
			else
				reg[inst.a] = ileft < iright;
			break;
		case JIROP_GE:
			if(issignedint)
				reg[inst.a] = (bool)((s64)ileft >= (s64)iright);
			else
				reg[inst.a] = ileft >= iright;
			break;

		case JIROP_JMP:
			if(inst.immediate)
				newpc = (s64)pc + (s64)inst.imm_u64;
			else
				newpc = (s64)pc + (s64)reg[inst.a];
			break;
		case JIROP_BRANCH:
			dojump = (bool)reg[inst.a];
			if(dojump) {
				if(inst.immediate)
					newpc = pc + (s64)inst.imm_u64;
				else
					newpc = pc + (s64)reg[inst.b];
			}
			break;

		case JIROP_VAL:
			if(inst.type_a == JIRTYPE_F32) {
				reg_f32[inst.a] = inst.imm_f32;
			} else if(inst.type_a == JIRTYPE_F64) {
				reg_f64[inst.a] = inst.imm_f64;
			} else if(inst.type_a == JIRTYPE_PTR_LOCAL) {
				reg_ptr_local[inst.a] = inst.imm_u64;
			} else if(inst.type_a == JIRTYPE_PTR_GLOBAL) {
				reg_ptr_global[inst.a] = inst.imm_u64;
			} else if(inst.type_a == JIRTYPE_PTR_HEAP) {
				reg_ptr_heap[inst.a] = (u8*)inst.imm_u64;
			} else {
				reg[inst.a] = iright;
			}
			break;
		case JIROP_MOVE:
			if(inst.type_a == JIRTYPE_F32) {
				reg_f32[inst.a] = reg_f32[inst.c];
			} else if(inst.type_a == JIRTYPE_F64) {
				reg_f64[inst.a] = reg_f64[inst.c];
			} else if(inst.type_a == JIRTYPE_PTR_LOCAL) {
				reg_ptr_local[inst.a] = reg_ptr_local[inst.c];
			} else if(inst.type_a == JIRTYPE_PTR_GLOBAL) {
				reg_ptr_global[inst.a] = reg_ptr_global[inst.c];
			} else if(inst.type_a == JIRTYPE_PTR_HEAP) {
				reg_ptr_heap[inst.a] = reg_ptr_heap[inst.c];
			} else {
				reg[inst.a] = iright;
			}
			break;
		case JIROP_LOAD:
			if(inst.seg == JIRSEG_HEAP) {
				assert("heap segment unimplemented" && 0);
				ptr = reg_ptr_heap[inst.b];
			} else {
				ptr = segTable[inst.seg] + ptrTable[inst.seg][inst.b];
			}

			if(inst.type_a == JIRTYPE_F32) {
				reg_f32[inst.a] = *(float*)(ptr);
			} else if(inst.type_a == JIRTYPE_F64) {
				reg_f64[inst.a] = *(double*)(ptr);
			} else {
				reg[inst.a] = *(u64*)(ptr) & imask;
				if(issignedint)
					reg[inst.a] = SIGN_EXTEND(reg[inst.a], iarithBits[inst.type_a]);
			}
			break;
		case JIROP_STOR:
			if(inst.seg == JIRSEG_HEAP) {
				assert("heap segment unimplemented" && 0);
				ptr = reg_ptr_heap[inst.b];
			} else {
				ptr = segTable[inst.seg] + ptrTable[inst.seg][inst.b];
			}

			if(inst.type_a == JIRTYPE_F32) {
				*(float*)(ptr) = reg_f32[inst.b];
			} else if(inst.type_a == JIRTYPE_F64) {
				*(double*)(ptr) = reg_f64[inst.b];
			} else {
				*(u64*)(ptr) &= ~imask;
				*(u64*)(ptr) |= reg[inst.b] & imask;
			}
			break;
		case JIROP_BFRAME:
			localbase[localbase_pos++] = local_pos;
			break;
		case JIROP_EFRAME:
			local_pos = localbase[--localbase_pos];
			break;
		case JIROP_ALLOC:
			if(inst.immediate) {
				reg_ptr_local[inst.a] = local_pos;
				local_pos += inst.imm_u64;
			} else {
				reg_ptr_local[inst.a] = local_pos;
				local_pos += TYPE_SIZES[inst.type_a];
			}
			break;

		case JIROP_FADD:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = reg_f64[inst.b] + f64right;
			else
				reg_f32[inst.a] = reg_f32[inst.b] + f32right;
			break;
		case JIROP_FSUB:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = reg_f64[inst.b] - f64right;
			else
				reg_f32[inst.a] = reg_f32[inst.b] - f32right;
			break;
		case JIROP_FMUL:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = reg_f64[inst.b] * f64right;
			else
				reg_f32[inst.a] = reg_f32[inst.b] * f32right;
			break;
		case JIROP_FDIV:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = reg_f64[inst.b] / f64right;
			else
				reg_f32[inst.a] = reg_f32[inst.b] / f32right;
			break;
		case JIROP_FMOD:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = fmod(reg_f64[inst.b], f64right);
			else
				reg_f32[inst.a] = fmodf(reg_f32[inst.b], f32right);
			break;
		case JIROP_FEQ:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = (reg_f64[inst.b] == f64right);
			else
				reg_f32[inst.a] = (reg_f32[inst.b] == f32right);
			break;
		case JIROP_FNE:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = (reg_f64[inst.b] != f64right);
			else
				reg_f32[inst.a] = (reg_f32[inst.b] != f32right);
			break;
		case JIROP_FLE:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = reg_f64[inst.b] <= f64right;
			else
				reg_f32[inst.a] = reg_f32[inst.b] <= f32right;
			break;
		case JIROP_FGT:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = reg_f64[inst.b] > f64right;
			else
				reg_f32[inst.a] = reg_f32[inst.b] > f32right;
			break;
		case JIROP_FLT:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = reg_f64[inst.b] < f64right;
			else
				reg_f32[inst.a] = reg_f32[inst.b] < f32right;
			break;
		case JIROP_FGE:
			if(inst.type_a == JIRTYPE_F64)
				reg_f64[inst.a] = reg_f64[inst.b] >= f64right;
			else
				reg_f32[inst.a] = reg_f32[inst.b] >= f32right;
			break;

		case JIROP_SETPORT:
		case JIROP_SETARG:
		case JIROP_SETRET:
			if(inst.type_a >= JIRTYPE_PTR_LOCAL && inst.type_a <= JIRTYPE_PTR_HEAP) {
				if(inst.type_a == JIRTYPE_PTR_HEAP)
					port_ptr_heap[inst.a] = reg_ptr_heap[inst.c];
				else if(inst.type_a == JIRTYPE_PTR_GLOBAL)
					port_ptr_global[inst.a] = ptr_global_right;
				else
					port_ptr_local[inst.a] = ptr_local_right;
			} else if(inst.type_a == JIRTYPE_F32) {
				port_f32[inst.a] = f32right;
			} else if(inst.type_a == JIRTYPE_F64) {
				port_f64[inst.a] = f64right;
			} else {
				port_u64[inst.a] = iright;
			}
			port_types[inst.a] = inst.type_a;
			break;
		case JIROP_GETPORT:
		case JIROP_GETARG:
		case JIROP_GETRET:
			iright = port_u64[inst.c] & imask;
			if(issignedint)
				iright = SIGN_EXTEND(iright, iarithBits[inst.type_a]);

			if(inst.type_a >= JIRTYPE_PTR_LOCAL && inst.type_a <= JIRTYPE_PTR_HEAP) {
				if(inst.type_a == JIRTYPE_PTR_HEAP)
					reg_ptr_heap[inst.a] = port_ptr_heap[inst.c];
				else if(inst.type_a == JIRTYPE_PTR_GLOBAL)
					reg_ptr_global[inst.a] = port_ptr_global[inst.c];
				else
					reg_ptr_local[inst.a] = port_ptr_local[inst.c];
			} else if(inst.type_a == JIRTYPE_F32) {
				reg_f32[inst.a] = port_f32[inst.b];
			} else if(inst.type_a == JIRTYPE_F64) {
				reg_f64[inst.a] = port_f64[inst.b];
			} else {
				reg[inst.a] = iright;
			}
			port_types[inst.a] = inst.type_a;
			break;

		case JIROP_CALL:
			procidStack[calldepth] = procid;
			pcStack[calldepth] = newpc; // store newpc so we return to the next inst
			++calldepth;
			if(inst.immediate)
				procid = inst.imm_u64;
			else
				procid = reg[inst.a];
			proc = image.proctab[procid];
			newpc = 0;
			break;
		case JIROP_RET:
			--calldepth;
			procid = procidStack[calldepth];
			newpc = pcStack[calldepth];
			proc = image.proctab[procid];
			break;

		case JIROP_IOREAD:
			ptr = segTable[inst.type_b-JIRTYPE_PTR_LOCAL];
			if(inst.type_b == JIRTYPE_PTR_HEAP) {
				ptr = reg_ptr_heap[inst.b];
			} else if(port_types[1] == JIRTYPE_PTR_GLOBAL) {
				ptr += reg_ptr_global[inst.b];
			} else {
				ptr += reg_ptr_local[inst.b];
			}
			assert("IOREAD FAILURE" &&
					read((unsigned int)reg[inst.a],(char*)ptr,(size_t)reg[inst.c])!=-1);
			break;
		case JIROP_IOWRITE:
			ptr = segTable[inst.type_b-JIRTYPE_PTR_LOCAL];
			if(inst.type_b == JIRTYPE_PTR_HEAP) {
				ptr = reg_ptr_heap[inst.b];
			} else if(port_types[1] == JIRTYPE_PTR_GLOBAL) {
				ptr += reg_ptr_global[inst.b];
			} else {
				ptr += reg_ptr_local[inst.b];
			}
			assert("IOWRITE FAILURE" &&
					write((unsigned int)reg[inst.a],(void*)ptr,(size_t)reg[inst.c])!=-1);
			break;
		case JIROP_IOOPEN:
			ptr = segTable[inst.type_a-JIRTYPE_PTR_LOCAL];
			if(inst.type_a == JIRTYPE_PTR_HEAP) {
				ptr = reg_ptr_heap[inst.a];
			} else if(port_types[1] == JIRTYPE_PTR_GLOBAL) {
				ptr += reg_ptr_global[inst.a];
			} else {
				ptr += reg_ptr_local[inst.a];
			}
			assert("IOOPEN FAILURE" && open((char*)ptr,(int)reg[inst.b],(int)reg[inst.c])!=-1);
			break;
		case JIROP_IOCLOSE:
			assert("IOCLOSE FAILURE" && close((unsigned int)reg[inst.a])!=-1);
			break;

		case JIROP_HALT:
			run = false;
			break;
		}

		if(!run) break;

		pc = newpc;
	}

	free(local);
	free(localbase);
}

void JIRIMAGE_init(JIRIMAGE *i, JIR **proctab, u8 *global) {
	i->proctab = proctab;
	i->global = global;
	i->port_types = malloc(64 * sizeof(JIRTYPE));
	i->port_u64 = malloc(64 * sizeof(u64));
	i->port_ptr_local = malloc(64 * sizeof(u64));
	i->port_ptr_global = malloc(64 * sizeof(u64));
	i->port_ptr_heap = malloc(64 * sizeof(u8*));
	i->port_f32 = malloc(64 * sizeof(f32));
	i->port_f64 = malloc(64 * sizeof(f64));
}

void JIRIMAGE_destroy(JIRIMAGE *i) {
	free(i->port_types);
	free(i->port_u64);
	free(i->port_ptr_local);
	free(i->port_ptr_global);
	free(i->port_ptr_heap);
	free(i->port_f32);
	free(i->port_f64);
	*i = (JIRIMAGE){0};
}

int compareProcFiles(const void *a, const void *b) {
	return strcmp(*(char**)a, *(char**)b);
}

int main(int argc, char **argv) {
	printf("\n###### testing bitcasts ######\n\n");
	{
		Fmap fm;
		fmapopen("test/bitcast", O_RDONLY, &fm);
		for(char *s = fm.buf; *s; ++s)
			*s = toupper(*s);
		JIR *instarr = NULL;
		arrsetcap(instarr, 2);
		Lexer lexer = (Lexer) {
			.keywords = keywords,
			.keywordsCount = STATICARRLEN(keywords),
			.keywordLengths = keywordLengths,
			.src = fm.buf,
			.cur = fm.buf,
			.srcEnd = fm.buf + fm.size,
			.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
		};

		parse(&lexer, &instarr);
		fmapclose(&fm);

		JIR *proctab[8] = { instarr };
		u8 *global = calloc(0x2000, sizeof(u8));
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab, global);

		JIR_exec(image);
		free(global);
		JIRIMAGE_destroy(&image);
		arrfree(instarr);
		instarr = NULL;
	}

	printf("\n###### testing unaryops ######\n\n");
	{
		Fmap fm;
		fmapopen("test/unaryops", O_RDONLY, &fm);
		for(char *s = fm.buf; *s; ++s)
			*s = toupper(*s);
		JIR *instarr = NULL;
		arrsetcap(instarr, 2);
		Lexer lexer = (Lexer) {
			.keywords = keywords,
			.keywordsCount = STATICARRLEN(keywords),
			.keywordLengths = keywordLengths,
			.src = fm.buf,
			.cur = fm.buf,
			.srcEnd = fm.buf + fm.size,
			.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
		};

		parse(&lexer, &instarr);
		fmapclose(&fm);

		JIR *proctab[8] = { instarr };
		u8 *global = calloc(0x2000, sizeof(u8));
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab, global);

		JIR_exec(image);
		free(global);
		JIRIMAGE_destroy(&image);
		arrfree(instarr);
		instarr = NULL;
	}

	printf("\n###### testing syscall ######\n\n");
	{
		Fmap fm;
		fmapopen("test/syscall", O_RDONLY, &fm);
		for(char *s = fm.buf; *s; ++s)
			*s = toupper(*s);
		JIR *instarr = NULL;
		arrsetcap(instarr, 2);
		Lexer lexer = (Lexer) {
			.keywords = keywords,
			.keywordsCount = STATICARRLEN(keywords),
			.keywordLengths = keywordLengths,
			.src = fm.buf,
			.cur = fm.buf,
			.srcEnd = fm.buf + fm.size,
			.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
		};

		parse(&lexer, &instarr);
		fmapclose(&fm);

		JIR *proctab[8] = { instarr };
		u8 *global = calloc(0x2000, sizeof(u8));
		strcpy((char*)global, "Hello, World!\n");
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab, global);

		JIR_exec(image);
		free(global);
		JIRIMAGE_destroy(&image);
		arrfree(instarr);
		instarr = NULL;
	}

	printf("\n###### testing float ops ######\n\n");
	{
		Fmap fm;
		fmapopen("test/floats", O_RDONLY, &fm);
		for(char *s = fm.buf; *s; ++s)
			*s = toupper(*s);
		JIR *instarr = NULL;
		arrsetcap(instarr, 2);
		Lexer lexer = (Lexer) {
			.keywords = keywords,
			.keywordsCount = STATICARRLEN(keywords),
			.keywordLengths = keywordLengths,
			.src = fm.buf,
			.cur = fm.buf,
			.srcEnd = fm.buf + fm.size,
			.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
		};

		parse(&lexer, &instarr);
		fmapclose(&fm);

		JIR *proctab[8] = { instarr };
		u8 *global = calloc(0x2000, sizeof(u8));
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab, global);

		JIR_exec(image);
		free(global);
		JIRIMAGE_destroy(&image);
		arrfree(instarr);
		instarr = NULL;
	}

	printf("\n###### testing integer comparisons ######\n\n");
	{
		Fmap fm;
		fmapopen("test/compare", O_RDONLY, &fm);
		for(char *s = fm.buf; *s; ++s)
			*s = toupper(*s);
		JIR *instarr = NULL;
		arrsetcap(instarr, 2);
		Lexer lexer = (Lexer) {
			.keywords = keywords,
			.keywordsCount = STATICARRLEN(keywords),
			.keywordLengths = keywordLengths,
			.src = fm.buf,
			.cur = fm.buf,
			.srcEnd = fm.buf + fm.size,
			.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
		};

		parse(&lexer, &instarr);
		fmapclose(&fm);

		JIR *proctab[8] = { instarr };
		u8 *global = calloc(0x2000, sizeof(u8));
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab, global);

		JIR_exec(image);
		free(global);
		JIRIMAGE_destroy(&image);
		arrfree(instarr);
		instarr = NULL;
	}

	printf("\n###### testing procedure calls ######\n\n");
	{
		Fmap fm;
		FilePathList files = LoadDirectoryFiles("test/proc");
		JIR *proctab[8] = {0};
		u8 *global = calloc(0x2000, sizeof(u8));
		JIRIMAGE image;
		qsort(files.paths, files.count, sizeof(char*), compareProcFiles);

		for(int i = 0; i < files.count; ++i) {
			JIR *insts = NULL;
			arrsetcap(insts, 32);
			fmapopen(files.paths[i], O_RDONLY, &fm);
			for(char *s = fm.buf; *s; ++s)
				*s = toupper(*s);
			Lexer lexer = (Lexer) {
				.keywords = keywords,
				.keywordsCount = STATICARRLEN(keywords),
				.keywordLengths = keywordLengths,
				.src = fm.buf,
				.cur = fm.buf,
				.srcEnd = fm.buf + fm.size,
				.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
			};
			parse(&lexer, &insts);
			fmapclose(&fm);
			proctab[i] = insts;
		}

		JIRIMAGE_init(&image, proctab, global);

		JIR_exec(image);

		free(global);
		for(int i = 0; i < files.count; ++i)
			arrfree(proctab[i]);
		JIRIMAGE_destroy(&image);
		UnloadDirectoryFiles(files);
	}

	return 0;
}
