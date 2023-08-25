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
	X(SETARG)       \
	X(GETARG)       \
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
	X(ARGUMENT)\
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
	uint64_t immui;
	int64_t immsi;
	float immf;
	double immf64;
};
typedef struct JIR JIR;

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
	 * call funcid_reg
	 * call "imm" funcid_imm
	 * setarg type arg_reg src_reg
	 * setarg "imm" type arg_reg val_imm
	 * getarg type dest_reg arg_index
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
		case TOKEN_HALT:
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
					sscanf(l->start, "%lx", &inst.immui);
					break;
				case TOKEN_OCTLIT:
					sscanf(l->start, "%lo", &inst.immui);
					break;
				case TOKEN_FLOATLIT:
					sscanf(l->start, "%f", &inst.immf);
					break;
				case TOKEN_INTLIT:
					sscanf(l->start, "%li", &inst.immsi);
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
				sscanf(l->start, "%li", &inst.immsi);
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
				sscanf(l->start, "%li", &inst.immsi);
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
			assert(l->token >= TOKEN_BINLIT && l->token <= TOKEN_INTLIT);
			switch(l->token) {
			case TOKEN_BINLIT:
				assert(0);
				break;
			case TOKEN_HEXLIT:
				sscanf(l->start, "%lx", &inst.immui);
				break;
			case TOKEN_OCTLIT:
				sscanf(l->start, "%lo", &inst.immui);
				break;
			case TOKEN_FLOATLIT:
				sscanf(l->start, "%f", &inst.immf);
				break;
			case TOKEN_INTLIT:
				sscanf(l->start, "%li", &inst.immsi);
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
		case TOKEN_SETARG:
			inst.opcode = JIROP_SETARG;
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
				assert(l->token >= TOKEN_BINLIT && l->token <= TOKEN_INTLIT);
				switch(l->token) {
				case TOKEN_BINLIT:
					assert(0);
					break;
				case TOKEN_HEXLIT:
					sscanf(l->start, "%lx", &inst.immui);
					break;
				case TOKEN_OCTLIT:
					sscanf(l->start, "%lo", &inst.immui);
					break;
				case TOKEN_FLOATLIT:
					sscanf(l->start, "%f", &inst.immf);
					break;
				case TOKEN_INTLIT:
					sscanf(l->start, "%li", &inst.immsi);
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
		case TOKEN_GETARG:
			inst.opcode = JIROP_GETARG;
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
	printf("opcode: %s\ntype: %s\ntype2: %s\na: %i\nb: %i\nc: %i\nimm: %i\nimmui: %lu\nimmsi: %li\nimmf: %f\nimmf64: %lf\n",
	opcodesDebug[inst.opcode],
	typesDebug[inst.type],
	typesDebug[inst.type2],
	inst.a, inst.b, inst.c,
	inst.immediate,
	inst.immui,
	inst.immsi,
	inst.immf,
	inst.immf64);
}

void JIR_exec(JIR *prog) {

	uint64_t regs[32] = {0};
	float fregs[32] = {0};
	double f64regs[32] = {0};

	uint64_t args[32] = {0};
	float fargs[32] = {0};
	double f64args[32] = {0};

	uint8_t *global = calloc(0x2000, sizeof(uint8_t));
	uint8_t *local = calloc(0x2000, sizeof(uint8_t));
	long *localbase = calloc(0x80, sizeof(long));
	long local_pos = 0;
	long localbase_pos = 0;

	uint8_t *segptr = NULL;

	bool run = true;
	bool dojump = false;
	int pc = 0;
	
	// TODO
	// add labels
	// add function call and return
	// type int arith ops
	// add floating point ops
	// refactor switch statements, interpreter should be <250 lines

	while(pc < arrlen(prog)) {
		JIR inst = prog[pc];
		int newpc = pc + 1;

		switch(inst.opcode) {
		default:
			printf("opcode %s unimplemeted\n", opcodesDebug[inst.opcode]);
			assert(0);
			break;
		case JIROP_DUMPREG:
			if(inst.type == JIRTYPE_F32)
				printf("float reg %i: %f\n", inst.a, fregs[inst.a]);
			else if(inst.type == JIRTYPE_F64)
				printf("float64 reg %i: %g\n", inst.a, f64regs[inst.a]);
			else
				printf("reg %i: %li\n", inst.a, regs[inst.a]);
			break;
		case JIROP_ADD:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] + inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] + regs[inst.c];
			}
			break;
		case JIROP_SUB:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] - inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] - regs[inst.c];
			}
			break;
		case JIROP_MUL:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] * inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] * regs[inst.c];
			}
			break;
		case JIROP_DIV:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] / inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] / regs[inst.c];
			}
			break;
		case JIROP_MOD:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] % inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] % regs[inst.c];
			}
			break;
		case JIROP_EQ:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] == inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] == regs[inst.c];
			}
			break;
		case JIROP_NE:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] != inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] != regs[inst.c];
			}
			break;
		case JIROP_LE:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] <= inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] <= regs[inst.c];
			}
			break;
		case JIROP_GT:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] > inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] > regs[inst.c];
			}
			break;
		case JIROP_LT:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] < inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] < regs[inst.c];
			}
			break;
		case JIROP_GE:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] >= inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] >= regs[inst.c];
			}
			break;
		case JIROP_AND:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] & inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] & regs[inst.c];
			}
			break;
		case JIROP_OR:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] | inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] | regs[inst.c];
			}
			break;
		case JIROP_XOR:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] ^ inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] ^ regs[inst.c];
			}
			break;
		case JIROP_LSHIFT:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] << inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] << regs[inst.c];
			}
			break;
		case JIROP_RSHIFT:
			if(inst.immediate) {
				regs[inst.a] = regs[inst.b] >> inst.immsi;
			} else {
				regs[inst.a] = regs[inst.b] >> regs[inst.c];
			}
			break;

		case JIROP_JMP:
			if(inst.immediate) {
				newpc = pc + inst.immsi;
			} else {
				newpc = pc + (int64_t)regs[inst.a];
			}
			break;
		case JIROP_BRANCH:
			dojump = (bool)regs[inst.a];
			if(dojump) {
				if(inst.immediate) {
					newpc = pc + inst.immsi;
				} else {
					newpc = pc + (int64_t)regs[inst.b];
				}
			}
			break;

		//TODO memory ops
		case JIROP_VAL:
			switch(inst.type) {
			case JIRTYPE_S64: case JIRTYPE_S32: case JIRTYPE_S16: case JIRTYPE_S8:
				regs[inst.a] = inst.immsi;
				break;
			case JIRTYPE_U64: case JIRTYPE_U32: case JIRTYPE_U16: case JIRTYPE_U8:
				regs[inst.a] = inst.immui;
				break;
			case JIRTYPE_F32:
				fregs[inst.a] = inst.immf;
				break;
			case JIRTYPE_F64:
				f64regs[inst.a] = inst.immf64;
				break;
			}
			break;
		case JIROP_MOVE:
			regs[inst.a] = regs[inst.b];
			break;
		case JIROP_LOAD:
			switch(inst.seg) {
			case JIRSEG_LOCAL:
				segptr = local;
				break;
			case JIRSEG_GLOBAL:
				segptr = global;
				break;
			case JIRSEG_ARGUMENT:
				assert("argument segment unimplemented" && 0);
				break;
			case JIRSEG_HEAP:
				assert("heap segment unimplemented" && 0);
				break;
			}
			switch(inst.type) {
			case JIRTYPE_S64:
				regs[inst.a] = *(int64_t*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_U64:
				regs[inst.a] = *(uint64_t*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_S32:
				regs[inst.a] = *(int32_t*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_U32:
				regs[inst.a] = *(uint32_t*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_S16:
				regs[inst.a] = *(int16_t*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_U16:
				regs[inst.a] = *(uint16_t*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_S8:
				regs[inst.a] = *(int8_t*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_U8:
				regs[inst.a] = *(uint8_t*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_F32:
				fregs[inst.a] = *(float*)(segptr + regs[inst.b]);
				break;
			case JIRTYPE_F64:
				f64regs[inst.a] = *(double*)(segptr + regs[inst.b]);
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
			case JIRSEG_ARGUMENT:
				assert("argument segment unimplemented" && 0);
				break;
			case JIRSEG_HEAP:
				assert("heap segment unimplemented" && 0);
				break;
			}
			switch(inst.type) {
			case JIRTYPE_S64:
				*(int64_t*)(segptr + regs[inst.a]) = regs[inst.b];
				break;
			case JIRTYPE_U64:
				*(uint64_t*)(segptr + regs[inst.a]) = regs[inst.b];
				break;
			case JIRTYPE_S32:
				*(int32_t*)(segptr + regs[inst.a]) = regs[inst.b];
				break;
			case JIRTYPE_U32:
				*(uint32_t*)(segptr + regs[inst.a]) = regs[inst.b];
				break;
			case JIRTYPE_S16:
				*(int16_t*)(segptr + regs[inst.a]) = regs[inst.b];
				break;
			case JIRTYPE_U16:
				*(uint16_t*)(segptr + regs[inst.a]) = regs[inst.b];
				break;
			case JIRTYPE_S8:
				*(int8_t*)(segptr + regs[inst.a]) = regs[inst.b];
				break;
			case JIRTYPE_U8:
				*(uint8_t*)(segptr + regs[inst.a]) = regs[inst.b];
				break;
			case JIRTYPE_F32:
				*(float*)(segptr + regs[inst.a]) = fregs[inst.b];
				break;
			case JIRTYPE_F64:
				*(double*)(segptr + regs[inst.a]) = f64regs[inst.b];
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
			regs[inst.a] = local_pos;
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

		case JIROP_SETARG:
			if(inst.immediate) {
				switch(inst.type) {
				case JIRTYPE_S64: case JIRTYPE_S32: case JIRTYPE_S16: case JIRTYPE_S8:
					args[inst.a] = inst.immsi;
					break;
				case JIRTYPE_U64: case JIRTYPE_U32: case JIRTYPE_U16: case JIRTYPE_U8:
					args[inst.a] = inst.immui;
					break;
				case JIRTYPE_F32:
					fargs[inst.a] = inst.immf;
					break;
				case JIRTYPE_F64:
					f64args[inst.a] = inst.immf64;
					break;
				}
			} else {
				switch(inst.type) {
				case JIRTYPE_S64: case JIRTYPE_S32: case JIRTYPE_S16: case JIRTYPE_S8:
					args[inst.a] = regs[inst.b];
					break;
				case JIRTYPE_U64: case JIRTYPE_U32: case JIRTYPE_U16: case JIRTYPE_U8:
					args[inst.a] = regs[inst.b];
					break;
				case JIRTYPE_F32:
					fargs[inst.a] = fregs[inst.b];
					break;
				case JIRTYPE_F64:
					f64args[inst.a] = f64regs[inst.b];
					break;
				}
			}
			break;
		case JIROP_GETARG:
			switch(inst.type) {
			case JIRTYPE_S64: case JIRTYPE_S32: case JIRTYPE_S16: case JIRTYPE_S8:
				regs[inst.a] = args[inst.b];
				break;
			case JIRTYPE_U64: case JIRTYPE_U32: case JIRTYPE_U16: case JIRTYPE_U8:
				regs[inst.a] = args[inst.b];
				break;
			case JIRTYPE_F32:
				fregs[inst.a] = fargs[inst.b];
				break;
			case JIRTYPE_F64:
				f64regs[inst.a] = f64args[inst.b];
				break;
			}
			break;

		case JIROP_HALT:
			run = false;
			break;
		}

		if(!run) break;

		pc = newpc;
	}

	free(global);
	free(local);
	free(localbase);
}

int main(int argc, char **argv) {
	Fmap fm;
	fmapopen("lexer_test", O_RDONLY, &fm);
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

	printf("\n###### testing parser ######\n\n");

	fmapopen("interpreter_test", O_RDONLY, &fm);
	for(char *s = fm.buf; *s; ++s)
		*s = toupper(*s);
	JIR *instarr = NULL;
	arrsetcap(instarr, 2);
	lexer = (Lexer) {
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

	JIR_exec(instarr);
	arrfree(instarr);

	return 0;
}
