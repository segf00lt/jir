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
#include "fmap.h"
#include "basic.h"
#include "stb_sprintf.h"
#include "stb_ds.h"

#undef sprintf
#undef snprintf
#define sprintf stbsp_sprintf
#define snprintf stbsp_snprintf

#define OPCODES         \
	X(EQ)           \
	X(NE)           \
	X(LE)           \
	X(GT)           \
	X(LT)           \
	X(GE)           \
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
	X(MEMSET)       \
	X(MOVE)         \
	X(BFRAME)       \
	X(EFRAME)       \
	X(ALLOC)        \
	X(HEAPALLOC)    \
	X(HEAPFREE)     \
	X(BITCAST)      \
	X(ITOF)         \
	X(FTOI)         \
	X(UTOF)         \
	X(FTOU)         \
	X(AND)          \
	X(OR)           \
	X(XOR)          \
	X(LSHIFT)       \
	X(RSHIFT)       \
	X(NOT)          \
	X(NEG)          \
	X(FNEG)         \
	X(ADD)          \
	X(SUB)          \
	X(MUL)          \
	X(DIV)          \
	X(MOD)          \
	X(FADD)         \
	X(FSUB)         \
	X(FMUL)         \
	X(FDIV)         \
	X(FMOD)         \
	X(CALL)         \
	X(RET)          \
	X(SETPORT)   \
	X(GETPORT)   \
	X(DUMPREG)      \
	X(DUMPREGRANGE) \
	X(DUMPMEM)      \
	X(DUMPMEMRANGE) \
	X(HALT)

#define TYPES  \
	X(S64) \
	X(U64) \
	X(S32) \
	X(U32) \
	X(S16) \
	X(U16) \
	X(S8)  \
	X(U8)  \
	X(F32) \
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
	4, // f32
	8, // f64
};

#define SEGMENTS   \
	X(LOCAL)   \
	X(GLOBAL)  \
	X(HEAP)

#define TOKEN_TO_OP(t) (JIROP)(t - TOKEN_EQ)
#define TOKEN_TO_TYPE(t) (JIRTYPE)((int)t - (int)TOKEN_S64)
#define TOKEN_TO_SEGMENT(t) (JIRSEG)(t - TOKEN_LOCAL)

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
	JIRTYPE type;
	JIRTYPE type2;
	JIRSEG seg;
	int a, b, c;
	bool immediate;
	union {
		s64 imm_s64;
		u64 imm_u64;
		s32 imm_s32;
		u32 imm_u32;
		s16 imm_s16;
		u16 imm_u16;
		s8 imm_s8;
		u8 imm_u8;
		float imm_f32;
		double imm_f64;
	};
};
typedef struct JIR JIR;

struct JIRIMAGE {
	JIR **proctab;
	u8 *global;
	// port_s are special registers for passing data between procedures, and in and out of the interpreter
	s64 *port_s64;
	u64 *port_u64;
	s32 *port_s32;
	u32 *port_u32;
	s16 *port_s16;
	u16 *port_u16;
	s8 *port_s8;
	u8 *port_u8;
	float *port_f32;
	double *port_f64;
};
typedef struct JIRIMAGE JIRIMAGE;

#include "lex.c"

void parse(Lexer *l, JIR **instarr) {
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
			} else {
				assert(l->token == TOKEN_IDENTIFIER);
				assert(*l->start == '%');
				inst.a = atoi(l->start + 1);
			}
			arrput(*instarr, inst);
			break;
		case TOKEN_RET:
			inst.opcode = JIROP_RET;
			arrput(*instarr, inst);
			break;
		case TOKEN_HALT: // NOTE redundant?
			inst.opcode = JIROP_HALT;
			arrput(*instarr, inst);
			break;
		case TOKEN_DUMPREG:
			inst.opcode = JIROP_DUMPREG;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(l->token);
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
				inst.type = TOKEN_TO_TYPE(l->token);
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
					sscanf(l->start, "%li", &inst.imm_s64);
					break;
				case TOKEN_FLOATLIT:
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


			// TODO
			break;


			lex(l);
			inst.opcode = TOKEN_TO_OP(token);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(l->token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.a = atoi(l->start + 1);
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
				sscanf(l->start, "%li", &inst.imm_s64);
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
				sscanf(l->start, "%li", &inst.imm_s64);
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
				inst.type = TOKEN_TO_TYPE(l->token);
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
		case TOKEN_BFRAME: case TOKEN_EFRAME:
			inst.opcode = TOKEN_TO_OP(token);
			arrput(*instarr, inst);
			break;
		case TOKEN_ALLOC:
			inst.opcode = JIROP_ALLOC;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(l->token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			assert(*l->start == '%');
			inst.a = atoi(l->start + 1);
			arrput(*instarr, inst);
			break;
		case TOKEN_HEAPALLOC:
			break;
		case TOKEN_HEAPFREE:
			break;
		case TOKEN_MEMSET:
			break;
		case TOKEN_VAL:
			inst.opcode = JIROP_VAL;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(l->token);
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
				sscanf(l->start, "%li", &inst.imm_s64);
				break;
			case TOKEN_FLOATLIT:
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
				inst.type = TOKEN_TO_TYPE(l->token);
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
			arrput(*instarr, inst);
			break;        
		case TOKEN_SETPORT:
			inst.opcode = JIROP_SETPORT;
			lex(l);
			if(l->token == TOKEN_IMM) {
				inst.immediate = true;
				lex(l);
			}
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(l->token);
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
					sscanf(l->start, "%li", &inst.imm_s64);
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
				inst.b = atoi(l->start + 1);
			}
			arrput(*instarr, inst);
			break;
		case TOKEN_GETPORT:
			inst.opcode = JIROP_GETPORT;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(l->token);
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
		case TOKEN_INVALID:
			break;
		}
	}
}

void JIR_print(JIR inst) {
	printf("opcode: %s\ntype: %s\ntype2: %s\na: %i\nb: %i\nc: %i\nimmediate: %i\nimm_s64: %li\nimm_u64: %lu\nimm_f32: %f\nimm_f64: %lf\n",
	opcodesDebug[inst.opcode],
	typesDebug[inst.type],
	typesDebug[inst.type2],
	inst.a, inst.b, inst.c,
	inst.immediate,
	inst.imm_s64,
	inst.imm_u64,
	inst.imm_f32,
	inst.imm_f64);
}

void JIR_exec(JIRIMAGE image) {

	u64 reg[32] = {0};
	float reg_f[32] = {0};
	double reg_f64[32] = {0};

	s64 *port_s64 = image.port_s64;
	u64 *port_u64 = image.port_u64;
	float *port_f32 = image.port_f32;
	double *port_f64 = image.port_f64;

	u8 *global = image.global;
	u8 *local = calloc(0x1000, sizeof(u8));
	long *localbase = calloc(0x80, sizeof(long));
	long local_pos = 0;
	long localbase_pos = 0;

	u64 procidStack[64] = {0};
	u64 pcStack[64] = {0};
	u64 calldepth = 0;

	u8 *segptr = NULL;

	bool run = true;
	bool dojump = false;
	u64 procid = 0;
	u64 pc = 0;
	
	JIR *proc = image.proctab[procid];

	// TODO
	// refactor switch statements, interpreter should be <250 lines
	// add labels
	// type int arith ops
	// add floating point ops

	while(pc < arrlen(proc)) {
		JIR inst = proc[pc];
		u64 newpc = pc + 1;

		switch(inst.opcode) {
		default:
			printf("opcode %s unimplemeted\n", opcodesDebug[inst.opcode]);
			assert(0);
			break;
		case JIROP_DUMPREG:
			if(inst.type == JIRTYPE_F32)
				printf("float reg %i: %f\n", inst.a, reg_f[inst.a]);
			else if(inst.type == JIRTYPE_F64)
				printf("float64 reg %i: %g\n", inst.a, reg_f64[inst.a]);
			else
				printf("reg %i: %li\n", inst.a, reg[inst.a]);
			break;
		case JIROP_ADD:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] + inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] + reg[inst.c];
			}
			break;
		case JIROP_SUB:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] - inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] - reg[inst.c];
			}
			break;
		case JIROP_MUL:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] * inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] * reg[inst.c];
			}
			break;
		case JIROP_DIV:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] / inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] / reg[inst.c];
			}
			break;
		case JIROP_MOD:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] % inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] % reg[inst.c];
			}
			break;
		case JIROP_EQ:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] == inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] == reg[inst.c];
			}
			break;
		case JIROP_NE:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] != inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] != reg[inst.c];
			}
			break;
		case JIROP_LE:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] <= inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] <= reg[inst.c];
			}
			break;
		case JIROP_GT:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] > inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] > reg[inst.c];
			}
			break;
		case JIROP_LT:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] < inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] < reg[inst.c];
			}
			break;
		case JIROP_GE:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] >= inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] >= reg[inst.c];
			}
			break;
		case JIROP_AND:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] & inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] & reg[inst.c];
			}
			break;
		case JIROP_OR:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] | inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] | reg[inst.c];
			}
			break;
		case JIROP_XOR:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] ^ inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] ^ reg[inst.c];
			}
			break;
		case JIROP_LSHIFT:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] << inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] << reg[inst.c];
			}
			break;
		case JIROP_RSHIFT:
			if(inst.immediate) {
				reg[inst.a] = reg[inst.b] >> inst.imm_s64;
			} else {
				reg[inst.a] = reg[inst.b] >> reg[inst.c];
			}
			break;

		case JIROP_JMP:
			if(inst.immediate) {
				newpc = pc + inst.imm_s64;
			} else {
				newpc = pc + (s64)reg[inst.a];
			}
			break;
		case JIROP_BRANCH:
			dojump = (bool)reg[inst.a];
			if(dojump) {
				if(inst.immediate) {
					newpc = pc + inst.imm_s64;
				} else {
					newpc = pc + (s64)reg[inst.b];
				}
			}
			break;

		//TODO memory ops
		case JIROP_VAL:
			switch(inst.type) {
			case JIRTYPE_S64: case JIRTYPE_S32: case JIRTYPE_S16: case JIRTYPE_S8:
				reg[inst.a] = inst.imm_s64;
				break;
			case JIRTYPE_U64: case JIRTYPE_U32: case JIRTYPE_U16: case JIRTYPE_U8:
				reg[inst.a] = inst.imm_u64;
				break;
			case JIRTYPE_F32:
				reg_f[inst.a] = inst.imm_f32;
				break;
			case JIRTYPE_F64:
				reg_f64[inst.a] = inst.imm_f64;
				break;
			}
			break;
		case JIROP_MOVE:
			reg[inst.a] = reg[inst.b];
			break;
		case JIROP_LOAD:
			switch(inst.seg) {
			case JIRSEG_LOCAL:
				segptr = local;
				break;
			case JIRSEG_GLOBAL:
				segptr = global;
				break;
			case JIRSEG_HEAP:
				assert("heap segment unimplemented" && 0);
				break;
			}
			switch(inst.type) {
			case JIRTYPE_S64:
				reg[inst.a] = *(s64*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_U64:
				reg[inst.a] = *(u64*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_S32:
				reg[inst.a] = *(s32*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_U32:
				reg[inst.a] = *(u32*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_S16:
				reg[inst.a] = *(s16*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_U16:
				reg[inst.a] = *(u16*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_S8:
				reg[inst.a] = *(s8*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_U8:
				reg[inst.a] = *(u8*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_F32:
				reg_f[inst.a] = *(float*)(segptr + reg[inst.b]);
				break;
			case JIRTYPE_F64:
				reg_f64[inst.a] = *(double*)(segptr + reg[inst.b]);
				break;
			}
			break;
		case JIROP_STOR:
			switch(inst.seg) {
			case JIRSEG_LOCAL:
				segptr = local;
				break;
			case JIRSEG_GLOBAL:
				segptr = global;
				break;
			case JIRSEG_HEAP:
				assert("heap segment unimplemented" && 0);
				break;
			}
			switch(inst.type) {
			case JIRTYPE_S64:
				*(s64*)(segptr + reg[inst.a]) = reg[inst.b];
				break;
			case JIRTYPE_U64:
				*(u64*)(segptr + reg[inst.a]) = reg[inst.b];
				break;
			case JIRTYPE_S32:
				*(s32*)(segptr + reg[inst.a]) = reg[inst.b];
				break;
			case JIRTYPE_U32:
				*(u32*)(segptr + reg[inst.a]) = reg[inst.b];
				break;
			case JIRTYPE_S16:
				*(s16*)(segptr + reg[inst.a]) = reg[inst.b];
				break;
			case JIRTYPE_U16:
				*(u16*)(segptr + reg[inst.a]) = reg[inst.b];
				break;
			case JIRTYPE_S8:
				*(s8*)(segptr + reg[inst.a]) = reg[inst.b];
				break;
			case JIRTYPE_U8:
				*(u8*)(segptr + reg[inst.a]) = reg[inst.b];
				break;
			case JIRTYPE_F32:
				*(float*)(segptr + reg[inst.a]) = reg_f[inst.b];
				break;
			case JIRTYPE_F64:
				*(double*)(segptr + reg[inst.a]) = reg_f64[inst.b];
				break;
			}
			break;
		case JIROP_BFRAME:
			localbase[localbase_pos++] = local_pos;
			break;
		case JIROP_EFRAME:
			local_pos = localbase[--localbase_pos];
			break;
		case JIROP_ALLOC:
			reg[inst.a] = local_pos;
			local_pos += TYPE_SIZES[inst.type];
			break;

		//TODO floating ops
		//case JIROP_FADD:
		//	break;
		//case JIROP_FSUB:
		//	break;
		//case JIROP_FMUL:
		//	break;
		//case JIROP_FDIV:
		//	break;
		//case JIROP_FMOD:
		//	break;
		//case JIROP_FEQ:
		//	break;
		//case JIROP_FNE:
		//	break;
		//case JIROP_FLE:
		//	break;
		//case JIROP_FGT:
		//	break;
		//case JIROP_FLT:
		//	break;
		//case JIROP_FGE:
		//	break;

		case JIROP_SETPORT:
			if(inst.immediate) {
				switch(inst.type) {
				case JIRTYPE_S64: case JIRTYPE_S32: case JIRTYPE_S16: case JIRTYPE_S8:
					port_s64[inst.a] = inst.imm_s64;
					break;
				case JIRTYPE_U64: case JIRTYPE_U32: case JIRTYPE_U16: case JIRTYPE_U8:
					port_u64[inst.a] = inst.imm_u64;
					break;
				case JIRTYPE_F32:
					port_f32[inst.a] = inst.imm_f32;
					break;
				case JIRTYPE_F64:
					port_f64[inst.a] = inst.imm_f64;
					break;
				}
			} else {
				switch(inst.type) {
				case JIRTYPE_S64: case JIRTYPE_S32: case JIRTYPE_S16: case JIRTYPE_S8:
					port_s64[inst.a] = reg[inst.b];
					break;
				case JIRTYPE_U64: case JIRTYPE_U32: case JIRTYPE_U16: case JIRTYPE_U8:
					port_u64[inst.a] = reg[inst.b];
					break;
				case JIRTYPE_F32:
					port_f32[inst.a] = reg_f[inst.b];
					break;
				case JIRTYPE_F64:
					port_f64[inst.a] = reg_f64[inst.b];
					break;
				}
			}
			break;
		case JIROP_GETPORT:
			switch(inst.type) {
			case JIRTYPE_S64: case JIRTYPE_S32: case JIRTYPE_S16: case JIRTYPE_S8:
				reg[inst.a] = port_s64[inst.b];
				break;
			case JIRTYPE_U64: case JIRTYPE_U32: case JIRTYPE_U16: case JIRTYPE_U8:
				reg[inst.a] = port_u64[inst.b];
				break;
			case JIRTYPE_F32:
				reg_f[inst.a] = port_f32[inst.b];
				break;
			case JIRTYPE_F64:
				reg_f64[inst.a] = port_f64[inst.b];
				break;
			}
			break;

		case JIROP_CALL:
			procidStack[calldepth] = procid;
			pcStack[calldepth] = newpc; // store newpc so we return to the next inst
			++calldepth;
			if(inst.immediate) {
				procid = inst.imm_u64;
			} else {
				procid = reg[inst.a];
			}
			proc = image.proctab[procid];
			newpc = 0;
			break;
		case JIROP_RET:
			--calldepth;
			procid = procidStack[calldepth];
			newpc = pcStack[calldepth];
			proc = image.proctab[procid];
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
	i->port_s64 = malloc(64 * sizeof(s64));
	i->port_u64 = malloc(64 * sizeof(u64));
	i->port_s32 = malloc(64 * sizeof(s32));
	i->port_u32 = malloc(64 * sizeof(u32));
	i->port_s16 = malloc(64 * sizeof(s16));
	i->port_u16 = malloc(64 * sizeof(u16));
	i->port_s8 = malloc(64 * sizeof(s8));
	i->port_u8 = malloc(64 * sizeof(u8));
	i->port_f32 = malloc(64 * sizeof(f32));
	i->port_f64 = malloc(64 * sizeof(f64));
}

void JIRIMAGE_destroy(JIRIMAGE *i) {
	free(i->port_s64);
	free(i->port_u64);
	free(i->port_s32);
	free(i->port_u32);
	free(i->port_s16);
	free(i->port_u16);
	free(i->port_s8);
	free(i->port_u8);
	free(i->port_f32);
	free(i->port_f64);
	*i = (JIRIMAGE){0};
}

int compareProcFiles(const void *a, const void *b) {
	return strcmp(*(char**)a, *(char**)b);
}

int main(int argc, char **argv) {

	printf("\n###### testing lexer ######\n\n");
	{
		Fmap fm;
		fmapopen("test/lexer_test", O_RDONLY, &fm);
		for(char *s = fm.buf; *s; ++s)
			*s = toupper(*s);
		Lexer lexer = (Lexer) {
			.keywords = keywords,
			.keywordsCount = ARRLEN(keywords),
			.keywordLengths = keywordLengths,
			.src = fm.buf,
			.cur = fm.buf,
			.srcEnd = fm.buf + fm.size,
			.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
		};
		while(!lexer.eof) {
			lex(&lexer);
			printf("%s\n", tokenDebug[lexer.token-TOKEN_INVALID]);
		}
		fmapclose(&fm);
	}

	printf("\n###### testing parser ######\n\n");
	{
		Fmap fm;
		fmapopen("test/interpreter_test", O_RDONLY, &fm);
		for(char *s = fm.buf; *s; ++s)
			*s = toupper(*s);
		JIR *instarr = NULL;
		arrsetcap(instarr, 2);
		Lexer lexer = (Lexer) {
			.keywords = keywords,
			.keywordsCount = ARRLEN(keywords),
			.keywordLengths = keywordLengths,
			.src = fm.buf,
			.cur = fm.buf,
			.srcEnd = fm.buf + fm.size,
			.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
		};

		parse(&lexer, &instarr);
		printf("%zu\n", arrlen(instarr));
		for(int i = 0; i < arrlen(instarr); ++i) {
			printf("INST %i\n", i);
			JIR_print(instarr[i]);
			printf("\n");
		}
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
				.keywordsCount = ARRLEN(keywords),
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
