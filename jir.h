#ifndef JIR_INCLUDE_H
#define JIR_INCLUDE_H

/* TODO
 *
 * JIR_translate_c()
 * JIR_translate_nasm()
 * JIR_translate_x64()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include "basic.h"
#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

typedef struct JIR JIR;
typedef struct JIRIMAGE JIRIMAGE;
typedef union JIROPERAND JIROPERAND;

void JIR_print(JIR inst);
bool JIR_errortrace(char *msg, JIR *prog, u64 pc, char *proclabel, u64 *pcstack, char **proclabelstack, u64 calldepth);
bool JIR_preprocess(JIRIMAGE *image);
void JIR_exec(JIRIMAGE *image);
void JIRIMAGE_destroy(JIRIMAGE *i);
void JIRIMAGE_init(JIRIMAGE *i, JIR *prog, u64 proglen);
void JIR_translate_c(char **Cbuf, u64 *Cbuflen, JIR **proctab, u64 nprocs);

#define OPERAND(field, data) (JIROPERAND){ .field = data }
#define PORT(index) (JIROPERAND){ .r = index }
#define REG(index) (JIROPERAND){ .r = index }
#define PROCID(data) (JIROPERAND){ .imm_u64 = (data) }
#define IMMU64(data) (JIROPERAND){ .imm_u64 = (data) }
#define IMMS64(data) (JIROPERAND){ .imm_u64 = (data) }
#define IMMU32(data) (JIROPERAND){ .imm_u64 = (data & 0xffffffff) }
#define IMMS32(data) (JIROPERAND){ .imm_u64 = (data & 0xffffffff) }
#define IMMU16(data) (JIROPERAND){ .imm_u64 = (data & 0xffff) }
#define IMMS16(data) (JIROPERAND){ .imm_u64 = (data & 0xffff) }
#define IMMU8(data) (JIROPERAND){ .imm_u64 = (data & 0xff) }
#define IMMS8(data) (JIROPERAND){ .imm_u64 = (data & 0xff) }
#define IMMF32(data) (JIROPERAND){ .imm_f32 = (data) }
#define IMMF64(data) (JIROPERAND){ .imm_f64 = (data) }
#define PTR(data) (JIROPERAND){ .offset = (data) }
#define LABEL(l) (JIROPERAND){ .label = (l) }

#define DUMPMEMOP(t, imm_start, imm_end, start, end)\
	(JIR){\
		.opcode = JIROP_DUMPMEM,\
		.type = {JIRTYPE_##t, JIRTYPE_##t, JIRTYPE_##t},\
		.operand = { {0}, start, end },\
		.immediate = {0, imm_start, imm_end },\
		.debugmsg = NULL,\
	}

#define DEBUGMSGOP(msg)\
	(JIR){\
		.opcode = JIROP_DEBUGMSG,\
		.type = {0},\
		.operand = {0},\
		.immediate = {0},\
		.debugmsg = msg,\
	}

#define BINOP(op, t, dest, left, right)\
	(JIR){\
		.opcode = JIROP_##op,\
		.type = { JIRTYPE_##t, JIRTYPE_##t, JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = left,\
		.operand[2] = right,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define UNOP(op, t, dest, src)\
	(JIR){\
		.opcode = JIROP_##op,\
		.type = { JIRTYPE_##t, JIRTYPE_##t, 0 },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define BINOPIMM(op, t, dest, left, right)\
	(JIR){\
		.opcode = JIROP_##op,\
		.type = { JIRTYPE_##t, JIRTYPE_##t, JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = left,\
		.operand[2] = right,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define UNOPIMM(op, t, dest, src)\
	(JIR){\
		.opcode = JIROP_##op,\
		.type = { JIRTYPE_##t, JIRTYPE_##t, 0 },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = { false, true, false, },\
		.debugmsg = NULL,\
	}

#define CASTOP(op, t_to, t_from, reg_to, reg_from)\
	(JIR){\
		.opcode = JIROP_##op,\
		.type = { JIRTYPE_##t_to, JIRTYPE_##t_from, 0, },\
		.operand[0] = reg_to,\
		.operand[1] = reg_from,\
		.operand[2] = {0},\
	}

#define DEFLABEL(label)\
	(JIR){\
		.opcode = JIROP_LABEL,\
		.type = {0},\
		.operand[0] = label,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define DEFPROCLABEL(label)\
	(JIR){\
		.opcode = JIROP_PROCLABEL,\
		.type = {0},\
		.operand[0] = label,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define BRANCHOP(cond, label)\
	(JIR){\
		.opcode = JIROP_BRANCH,\
		.type = {0},\
		.operand[0] = cond,\
		.operand[1] = {0},\
		.operand[2] = label,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define JMPOP(label)\
	(JIR){\
		.opcode = JIROP_JMP,\
		.type = {0},\
		.operand[0] = {0},\
		.operand[1] = {0},\
		.operand[2] = label,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define LOADOP(t_dest, t_src, dest, src)\
	(JIR){\
		.opcode = JIROP_LOAD,\
		.type = { JIRTYPE_##t_dest, 0, JIRTYPE_##t_src },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = { false, false, false },\
		.debugmsg = NULL,\
	}

#define STOROP(t_dest, t_src, dest, src)\
	(JIR){\
		.opcode = JIROP_STOR,\
		.type = { JIRTYPE_##t_src, 0, JIRTYPE_##t_dest, },\
		.operand[0] = src,\
		.operand[1] = {0},\
		.operand[2] = dest,\
		.immediate = { false, false, false },\
		.debugmsg = NULL,\
	}

#define MOVEOP(t, dest, src)\
	(JIR){\
		.opcode = JIROP_MOVE,\
		.type = { JIRTYPE_##t, 0, JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
	}

#define HALTOP (JIR){ .opcode = JIROP_HALT, .type = {0}, .operand = {0}, .immediate = {0}, .debugmsg = NULL, }
#define RETOP (JIR){ .opcode = JIROP_RET, .type = {0}, .operand = {0}, .immediate = {0}, .debugmsg = NULL, }

#define MOVEOPIMM(t, dest, src)\
	(JIR){\
		.opcode = JIROP_MOVE,\
		.type = { JIRTYPE_##t, 0, JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define LOCALOP(t, dest)\
	(JIR){\
		.opcode = JIROP_LOCAL,\
		.type = { JIRTYPE_PTR_LOCAL, JIRTYPE_##t, 0 },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define LOCALOPIMM(size, dest)\
	(JIR){\
		.opcode = JIROP_LOCAL,\
		.type = { JIRTYPE_PTR_LOCAL, JIRTYPE_U64,0 },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = size,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define DEFGLOBAL(label, t, data)\
	(JIR){\
		.opcode = JIROP_GLOBAL,\
		.type = { 0, 0, JIRTYPE_##t },\
		.operand[0] = label,\
		.operand[1] = {0},\
		.operand[2] = data,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define DEFGLOBALZEROS(label, bytes)\
	(JIR){\
		.opcode = JIROP_GLOBAL,\
		.type = { 0, JIRTYPE_U64, 0 },\
		.operand[0] = label,\
		.operand[1] = bytes,\
		.operand[2] = {0},\
		.immediate = { false, true, false },\
		.debugmsg = NULL,\
	}

#define GROWHEAPOP(factor)\
	(JIR){\
		.opcode = JIROP_GROWHEAP,\
		.type = { 0, 0, JIRTYPE_U64 },\
		.operand[0] = {0},\
		.operand[1] = {0},\
		.operand[2] = factor,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define GROWHEAPOPINDIRECT(factor)\
	(JIR){\
		.opcode = JIROP_GROWHEAP,\
		.type = { 0, 0, JIRTYPE_U64 },\
		.operand[0] = {0},\
		.operand[1] = {0},\
		.operand[2] = factor,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define CALLOP(proclabel)\
	(JIR){\
		.opcode = JIROP_CALL,\
		.type = {0},\
		.operand[0] = {0},\
		.operand[1] = {0},\
		.operand[2] = proclabel,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define SYSCALLOP(proclabel)\
	(JIR){\
		.opcode = JIROP_SYSCALL,\
		.type = {0},\
		.operand[0] = {0},\
		.operand[1] = {0},\
		.operand[2] = proclabel,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

// TODO fix direct and indirect calls
#define CALLOPINDIRECT(proclabel)\
	(JIR){\
		.opcode = JIROP_CALL,\
		.type = {0},\
		.operand[0] = proclabel,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define SYSCALLOPINDIRECT(proclabel)\
	(JIR){\
		.opcode = JIROP_SYSCALL,\
		.type = {0},\
		.operand[0] = proclabel,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define SETARGOP(t, dest, src)\
	(JIR){\
		.opcode = JIROP_SETARG,\
		.type = { JIRTYPE_##t,0,JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define SETARGOPIMM(t, dest, src)\
	(JIR){\
		.opcode = JIROP_SETARG,\
		.type = { JIRTYPE_##t,0,JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define SETRETOP(t, dest, src)\
	(JIR){\
		.opcode = JIROP_SETRET,\
		.type = { JIRTYPE_##t,0,JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define SETRETOPIMM(what, t, dest, src)\
	(JIR){\
		.opcode = JIROP_SETRET,\
		.type = { JIRTYPE_##t,0,JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define GETARGOP(t, dest, src)\
	(JIR){\
		.opcode = JIROP_GETARG,\
		.type = { JIRTYPE_##t,0,JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define GETRETOP(t, dest, src)\
	(JIR){\
		.opcode = JIROP_GETRET,\
		.type = { JIRTYPE_##t,0,JIRTYPE_##t },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = src,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define DUMPREGOP(t, r)\
	(JIR){\
		.opcode = JIROP_DUMPREG,\
		.type = { JIRTYPE_##t,0,0 },\
		.operand[0] = r,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define DUMPPORTOP(t, r)\
	(JIR){\
		.opcode = JIROP_DUMPPORT,\
		.type = { JIRTYPE_##t,0,0 },\
		.operand[0] = r,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define OPCODES   \
	X(HALT)       \
	X(AND)        \
	X(OR)         \
	X(XOR)        \
	X(LSHIFT)     \
	X(RSHIFT)     \
	X(NOT)        \
	X(ADD)        \
	X(SUB)        \
	X(MUL)        \
	X(DIV)        \
	X(MOD)        \
	X(NEG)        \
	X(EQ)         \
	X(NE)         \
	X(LE)         \
	X(GT)         \
	X(LT)         \
	X(GE)         \
	X(FADD)       \
	X(FSUB)       \
	X(FMUL)       \
	X(FDIV)       \
	X(FMOD)       \
	X(FNEG)       \
	X(FEQ)        \
	X(FNE)        \
	X(FLE)        \
	X(FGT)        \
	X(FLT)        \
	X(FGE)        \
	X(BRANCH)     \
	X(JMP)        \
	X(LABEL)      \
	X(LOAD)       \
	X(STOR)       \
	X(MOVE)       \
	X(LOCAL)      \
	X(GLOBAL)     \
	X(PROCLABEL)  \
	X(CALL)       \
	X(RET)        \
	X(SYSCALL)    \
	X(SETARG)     \
	X(GETARG)     \
	X(SETRET)     \
	X(GETRET)     \
	X(SETPORT)    \
	X(GETPORT)    \
	X(GROWHEAP)   \
	X(BITCAST)    \
	X(TYPECAST)   \
	X(DUMPREG)    \
	X(DUMPPORT)   \
	X(DUMPMEM)    \
	X(DEBUGMSG)   \
	X(NOOP)

#define TYPES    \
	X(S64)       \
	X(S32)       \
	X(S16)       \
	X(S8)        \
	X(U64)       \
	X(U32)       \
	X(U16)       \
	X(U8)        \
	X(PTR_LOCAL) \
	X(PTR_GLOBAL)\
	X(PTR_HEAP)  \
	X(F32)       \
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

// NOTE floating point types are ignored in these tables
u64 TYPE_MASKS[] = {
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

u64 TYPE_BITS[] = {
	64, 32, 16, 8, 
	64, 32, 16, 8, 
	64, // PTR_LOCAL
	64, // PTR_GLOBAL
	64, // PTR_HEAP
	0, // F32
	0, // F64
};

#define SEGMENTS\
	X(LOCAL)    \
	X(GLOBAL)   \
	X(HEAP)

#define TOKEN_TO_OP(t) (JIROP)(t - TOKEN_HALT)
#define TOKEN_TO_TYPE(t) (JIRTYPE)((int)t - (int)TOKEN_S64)
#define TOKEN_TO_SEGMENT(t) (JIRSEG)(t - TOKEN_LOCAL)
#define PTRTYPE_TO_SEGMENT(pt) (JIRSEG)(pt - JIRTYPE_PTR_LOCAL)

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

const char *opcodesdebug[] = {
#define X(val) #val,
	OPCODES
#undef X
};

const char *typesdebug[] = {
#define X(val) #val,
	TYPES
#undef X
};

const char *segmentsdebug[] = {
#define X(val) #val,
	SEGMENTS
#undef X
};

union JIROPERAND {
	int r;
	u64 imm_u64;
	float imm_f32;
	double imm_f64;
	struct {
		union {
			s64 jump;
			u64 offset;
		};
		const char *label;
	};
};

struct JIR {
	JIROP opcode;
	JIRTYPE type[3];
	JIROPERAND operand[3];
	bool immediate[3];
	const char *debugmsg;
};

struct JIRIMAGE {
	JIR *prog;
	u64 proglen;
	u8 *local;
	u8 *global;
	u8 *heap;
	size_t localsize;
	size_t globalsize;
	size_t heapsize; // only the heap is allowed to grow
	u64 *localbase;
	size_t localbasesize;
	u64 local_pos;
	u64 localbase_pos;
	u64 *reg;
	u64 *reg_ptr;
	float *reg_f32;
	double *reg_f64;
	// ports are special registers for passing data between procedures, and in and out of the interpreter
	JIRTYPE *port_types;
	u64 *port;
	u64 *port_ptr;
	float *port_f32;
	double *port_f64;
};

#ifdef JIR_IMPL // implementation

JINLINE void JIR_print(JIR inst) {
	printf("(JIR) {\n\
	.opcode = JIROP_%s,\n\
	.type[0] = JIRTYPE_%s,\n\
	.operand[0] = { .r = %i, .imm_u64 = %lu, .imm_f32 = %f, .imm_f64 = %lf, .offset = %lu, .jump = %li, .label = %s },\n\
	.immediate[0] = %i,\n\
	.type[1] = JIRTYPE_%s,\n\
	.operand[1] = { .r = %i, .imm_u64 = %lu, .imm_f32 = %f, .imm_f64 = %lf, .offset = %lu, .jump = %li, .label = %s },\n\
	.immediate[1] = %i,\n\
	.type[2] = JIRTYPE_%s,\n\
	.operand[2] = { .r = %i, .imm_u64 = %lu, .imm_f32 = %f, .imm_f64 = %lf, .offset = %lu, .jump = %li, .label = %s },\n\
	.immediate[2] = %i,\n\
	.debugmsg = %s,\n}\n",
	opcodesdebug[inst.opcode],
	typesdebug[inst.type[0]],
	inst.operand[0].r,
	inst.operand[0].imm_u64,
	inst.operand[0].imm_f32,
	inst.operand[0].imm_f64,
	inst.operand[0].offset,
	inst.operand[0].jump,
	inst.operand[0].label,
	inst.immediate[0],
	typesdebug[inst.type[1]],
	inst.operand[1].r,
	inst.operand[1].imm_u64,
	inst.operand[1].imm_f32,
	inst.operand[1].imm_f64,
	inst.operand[1].offset,
	inst.operand[1].jump,
	inst.operand[1].label,
	inst.immediate[1],
	typesdebug[inst.type[2]],
	inst.operand[2].r,
	inst.operand[2].imm_u64,
	inst.operand[2].imm_f32,
	inst.operand[2].imm_f64,
	inst.operand[2].offset,
	inst.operand[2].jump,
	inst.operand[2].label,
	inst.immediate[2],
	inst.debugmsg);
}

JINLINE bool JIR_errortrace(char *msg, JIR *prog, u64 pc, char *proclabel, u64 *pcstack, char **proclabelstack, u64 calldepth) {
	fputs("JIR ERROR TRACE: ERROR TRACING\n", stdout);
	pcstack[calldepth] = pc + 1;
	proclabelstack[calldepth] = proclabel;
	JIR inst;
	for(u64 i = 0; i <= calldepth; ++i) {
		pc = pcstack[calldepth-i] - 1;
		proclabel = proclabelstack[calldepth-i];
		inst = prog[pc];
		printf("INSTRUCTION %lu, PROC '%s':\n", pc, proclabel);
		JIR_print(inst);
	}
	fputs(msg, stdout);
	return false;
}

bool JIR_preprocess(JIRIMAGE *image) {
	u64 pc = 0;
	JIR *prog = image->prog;
	u64 proglen = image->proglen;

	bool ret = true;

	while(pc < proglen && prog[pc].opcode != JIROP_HALT) {
		JIR inst = prog[pc++];

		bool type_0_int = (inst.type[0] >= JIRTYPE_S64 && inst.type[0] <= JIRTYPE_U8);
		bool type_1_int = (inst.type[1] >= JIRTYPE_S64 && inst.type[1] <= JIRTYPE_U8);
		bool type_0_f32 = (inst.type[0] == JIRTYPE_F32);
		bool type_1_f32 = (inst.type[1] == JIRTYPE_F32);
		bool type_0_f64 = (inst.type[0] == JIRTYPE_F64);
		bool type_1_f64 = (inst.type[1] == JIRTYPE_F64);
		bool type_0_ptr = (inst.type[0] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP);
		bool type_1_ptr = (inst.type[1] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP);

		if(inst.opcode < JIROP_HALT || inst.opcode > JIROP_NOOP) {
			printf("JIR PREPROCESS:\nINSTRUCTION %lu:\nERROR: INVALID OPCODE %u\n",pc,inst.opcode);
			ret = false;
			continue;
		}

		if(inst.opcode >= JIROP_FADD && inst.opcode <= JIROP_FGE && inst.type[0] < JIRTYPE_F32) {
			printf("JIR PREPROCESS:\nINSTRUCTION %lu:\n", pc);
			JIR_print(inst);
			printf("ERROR: INVALID TYPE TO FLOATING POINT INSTRUCTION\n");
			ret = false;
			continue;
		}

		if(inst.opcode >= JIROP_ADD && inst.opcode <= JIROP_GE && inst.type[0] >= JIRTYPE_F32) {
			printf("JIR PREPROCESS:\nINSTRUCTION %lu:\n", pc);
			JIR_print(inst);
			printf("ERROR: INVALID TYPE TO INTEGER INSTRUCTION\n");
			ret = false;
			continue;
		}

		if(inst.opcode == JIROP_MOVE && inst.type[0] != inst.type[2]) {
			printf("JIR PREPROCESS:\nINSTRUCTION %lu:\n", pc);
			JIR_print(inst);
			printf("ERROR: INCOMPATIBLE OPERAND TYPES TO MOVE INSTRUCTION\n");
			ret = false;
			continue;
		}

		if((inst.opcode == JIROP_BITCAST || inst.opcode == JIROP_TYPECAST)
		&& !(type_0_int && type_1_int) && !(type_0_f64 && type_1_int) && !(type_0_f32 && type_1_int)
		&& !(type_0_int && type_1_f64) && !(type_0_int && type_1_f32) && !(type_0_f32 && type_1_f64)
		&& !(type_0_f64 && type_1_f32) && !(type_0_ptr && type_1_int) && !(type_0_int && type_1_ptr))
		{
			printf("JIR PREPROCESS:\nINSTRUCTION %lu:\n", pc);
			JIR_print(inst);
			printf("ERROR: INVALID TYPE PAIR TO CAST\n");
			ret = false;
			continue;
		}
	}

	struct { const char *key; JIROP value; } *opcodes = NULL;
	struct { const char *key; u64 value; } *positions = NULL;

	sh_new_arena(opcodes);
	sh_new_arena(positions);
	u8 *global = image->global;
	u64 globalpos = 0;

	// define labels
	for(u64 pc = 0; pc < proglen && prog[pc].opcode != JIROP_HALT; ++pc) {
		JIR inst = prog[pc];

		if(inst.opcode != JIROP_LABEL && inst.opcode != JIROP_PROCLABEL && inst.opcode != JIROP_GLOBAL)
			continue;

		const char *label = inst.operand[0].label;
		ptrdiff_t i = shgeti(positions, label);

		if(i != -1) {
			printf("JIR PREPROCESS:\nINSTRUCTION %lu:\n", pc);
			JIR_print(inst);
			printf("ERROR: REDEFINITION OF LABEL '%s' PREVIOUSLY DEFINED AT INSTRUCTION %lu\n", label, positions[i].value-1);
			shfree(opcodes);
			shfree(positions);
			return false;
		}

		shput(opcodes, label, inst.opcode);
		switch(inst.opcode) {
			default:
				UNREACHABLE;
			case JIROP_PROCLABEL:
			case JIROP_LABEL:
				shput(positions, label, (pc+1));
				break;
			case JIROP_GLOBAL:
				shput(positions, label, globalpos);
				if(inst.immediate[1]) {
					memset(global+globalpos, 0, inst.operand[1].imm_u64);
					globalpos += inst.operand[1].imm_u64;
				} else {
					if(inst.type[2] == JIRTYPE_F64) {
						*(double*)(global+globalpos) = inst.operand[2].imm_f64;
					} else if(inst.type[2] == JIRTYPE_F32) {
						*(float*)(global+globalpos) = inst.operand[2].imm_f32;
					} else if(inst.type[2] >= JIRTYPE_PTR_LOCAL && inst.type[2] <= JIRTYPE_PTR_HEAP) {
						printf("JIR PREPROCESS:\nINSTRUCTION %lu:\n", pc);
						JIR_print(inst);
						printf("ERROR: ILLEGAL TO DEFINE GLOBAL POINTER DATA\n");
						shfree(opcodes);
						shfree(positions);
						return false;
					} else {
						*(u64*)(global+globalpos) = inst.operand[2].imm_u64 & TYPE_MASKS[inst.type[2]];
					}
					globalpos += TYPE_SIZES[inst.type[2]];
					if(globalpos >= image->globalsize) {
						printf("JIR PREPROCESS:\nINSTRUCTION %lu:\n", pc);
						JIR_print(inst);
						printf("ERROR: TOO MANY GLOBAL VARIABLES, PLEASE ALLOCATE A BIGGER GLOBAL SEGMENT\n");
						shfree(opcodes);
						shfree(positions);
						return false;
					}
				}
				break;
		}
	}

	// replace labels with jumps
	for(u64 pc = 0; pc < proglen && prog[pc].opcode != JIROP_HALT; ++pc) {
		JIR inst = prog[pc];

		if(inst.opcode != JIROP_BRANCH && \
		   inst.opcode != JIROP_CALL && \
		   inst.opcode != JIROP_JMP && \
		   !(inst.opcode == JIROP_STOR && inst.type[0] == JIRTYPE_PTR_GLOBAL && inst.operand[0].label) && \
		   !(inst.opcode == JIROP_LOAD && inst.type[1] == JIRTYPE_PTR_GLOBAL && inst.operand[1].label))
			continue;

		const char *label = inst.operand[2].label;
		ptrdiff_t i = shgeti(positions, label);

		if(i == -1) {
			printf("JIR PREPROCESS:\nINSTRUCTION %lu:\n", pc);
			JIR_print(inst);
			printf("ERROR: REFERENCE TO UNDEFINED LABEL '%s'\n", label);
			shfree(opcodes);
			shfree(positions);
			return false;
		}

		if(inst.opcode == JIROP_BRANCH || inst.opcode == JIROP_CALL || inst.opcode == JIROP_JMP)
			prog[pc].operand[2].jump = (positions[i].value > pc) ? (s64)(positions[i].value - pc) : (s64)(-pc + positions[i].value);
		else if(inst.opcode == JIROP_STOR)
			prog[pc].operand[0].offset = positions[i].value;
		else if(inst.opcode == JIROP_LOAD)
			prog[pc].operand[1].offset = positions[i].value;
		else
			UNREACHABLE;
	}

	shfree(opcodes);
	shfree(positions);

	return ret;
}

void JIR_exec(JIRIMAGE *image) {
	u64 *reg = image->reg;
	u64 *reg_ptr = image->reg_ptr;
	float *reg_f32 = image->reg_f32;
	double *reg_f64 = image->reg_f64;

	JIRTYPE *port_types = image->port_types;
	u64 *port = image->port;
	u64 *port_ptr = image->port_ptr;
	float *port_f32 = image->port_f32;
	double *port_f64 = image->port_f64;

	u8 *global = image->global;
	u8 *local = image->local;
	u8 *heap = image->heap;
	size_t segsize[] = {
		image->localsize,
		image->globalsize,
		image->heapsize,
	};
	size_t localbasesize = image->localbasesize;
	u64 *localbase = image->localbase;
	u64 local_pos = image->local_pos;
	u64 localbase_pos = image->localbase_pos;

	u8 *ptr = NULL;
	u8 *segtable[] = { local, global, heap };

	char *proclabelstack[64] = {0};
	u64 pcstack[64] = {0};
	u64 calldepth = 0;

	//u64 iocallval = 0;
	//char *iocallerror = NULL;
	s64 syscall_success = 0;
	bool run = true;
	bool issignedint = false;
	char *proclabel = 0;
	u64 pc = 0;
	u64 newpc = 0;
	
	JIR *prog = image->prog;

	while(run && pc < image->proglen) {
		pc = newpc;
		JIR inst = prog[pc];
#ifdef DEBUG
		printf("JIR EXCECUTION TRACE: EXECUTION LOG\nINSTRUCTION %lu, PROC '%s':\n", pc, proclabel);
		JIR_print(inst);
#endif
		newpc = pc + 1;
		u64 imask = TYPE_MASKS[inst.type[0]];
		u64 ileft = (inst.immediate[1] ? inst.operand[1].imm_u64 : reg[inst.operand[1].r]) & imask;
		u64 ptr_left = inst.immediate[1] ? inst.operand[1].offset : reg_ptr[inst.operand[1].r];
		u64 ptr_right = inst.immediate[2] ? inst.operand[2].offset : reg_ptr[inst.operand[2].r];
		u64 iright = (inst.immediate[2] ? inst.operand[2].imm_u64 : reg[inst.operand[2].r]) & imask;
		f32 f32right = inst.immediate[2] ? inst.operand[2].imm_f32 : reg_f32[inst.operand[2].r];
		f64 f64right = inst.immediate[2] ? inst.operand[2].imm_f64 : reg_f64[inst.operand[2].r];
		bool type_0_int = (inst.type[0] >= JIRTYPE_S64 && inst.type[0] <= JIRTYPE_U8);
		bool type_1_int = (inst.type[1] >= JIRTYPE_S64 && inst.type[1] <= JIRTYPE_U8);
		bool type_0_f32 = (inst.type[0] == JIRTYPE_F32);
		bool type_1_f32 = (inst.type[1] == JIRTYPE_F32);
		bool type_0_f64 = (inst.type[0] == JIRTYPE_F64);
		bool type_1_f64 = (inst.type[1] == JIRTYPE_F64);
		bool type_0_ptr = (inst.type[0] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP);
		bool type_1_ptr = (inst.type[1] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP);

		issignedint = (inst.type[0] >= JIRTYPE_S64 && inst.type[0] <= JIRTYPE_S8);
		if(issignedint) {
			ileft = SIGN_EXTEND(ileft, TYPE_BITS[inst.type[0]]);
			iright = SIGN_EXTEND(iright, TYPE_BITS[inst.type[0]]);
		}

		switch(inst.opcode) {
		default:
			UNREACHABLE;
			break;
		case JIROP_PROCLABEL:
		case JIROP_LABEL:
		case JIROP_NOOP:
		case JIROP_GLOBAL:
			break;
		case JIROP_DUMPMEM:
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[0])] <= ptr_left ||
			   segsize[PTRTYPE_TO_SEGMENT(inst.type[0])] <= ptr_right) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
			}
			ptr = segtable[PTRTYPE_TO_SEGMENT(inst.type[0])];
			printf("========================\n %s SEG: 0x%lx, 0x%lx\n\n",
					segmentsdebug[PTRTYPE_TO_SEGMENT(inst.type[0])],
					ptr_left, ptr_right);
			// NOTE range is inclusive
			for(u64 p = 0, l = ptr_right-ptr_left; p <= l; ++p) {
				printf(" %02X",ptr[p+ptr_left]);
				if(p < 7)
					continue;
				if(!((p+1) & 0x7))
					fputc('\n',stdout);
			}
			if((ptr_right-ptr_left) & 0x7)
				fputc('\n',stdout);
			fputs("========================\n",stdout);
			break;
		case JIROP_DEBUGMSG:
			printf("DEBUGMSG: %s\n", inst.debugmsg);
			break;
		case JIROP_DUMPPORT:
			if(inst.type[0] == JIRTYPE_F32)
				printf("F32 PORT %i: %f\n", inst.operand[0].r, port_f32[inst.operand[0].r]);
			else if(inst.type[0] == JIRTYPE_F64)
				printf("F64 PORT %i: %g\n", inst.operand[0].r, port_f64[inst.operand[0].r]);
			else if(inst.type[0] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP)
				printf("PTR PORT %i: 0x%lx\n", inst.operand[0].r, port_ptr[inst.operand[0].r]);
			else
				printf("PORT %i: 0x%lx\n", inst.operand[0].r, imask & port[inst.operand[0].r]);
			break;
		case JIROP_DUMPREG:
			if(inst.type[0] == JIRTYPE_F32)
				printf("F32 REG %i: %f\n", inst.operand[0].r, reg_f32[inst.operand[0].r]);
			else if(inst.type[0] == JIRTYPE_F64)
				printf("F64 REG %i: %lf\n", inst.operand[0].r, reg_f64[inst.operand[0].r]);
			else if(inst.type[0] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP)
				printf("PTR REG %i: 0x%lx\n", inst.operand[0].r, reg_ptr[inst.operand[0].r]);
			else
				printf("REG %i: 0x%lx\n", inst.operand[0].r,
						TYPE_MASKS[inst.type[0]]&reg[inst.operand[0].r]);
			break;
		case JIROP_BITCAST:
			if(type_0_int && type_1_int) {
				reg[inst.operand[0].r] = reg[inst.operand[1].r] & TYPE_MASKS[inst.type[1]];
			} else if(type_0_f64 && type_1_int) {
				iright = reg[inst.operand[1].r] & TYPE_MASKS[inst.type[1]];
				reg_f64[inst.operand[0].r] = *(double*)&iright;
			} else if(type_0_f32 && type_1_int) {
				iright = reg[inst.operand[1].r] & TYPE_MASKS[inst.type[1]];
				reg_f32[inst.operand[0].r] = *(float*)&iright;
			} else if(type_0_int && type_1_f64) {
				reg[inst.operand[0].r] = *(u64*)(reg_f64 + inst.operand[1].r) & TYPE_MASKS[inst.type[0]];
			} else if(type_0_int && type_1_f32) {
				reg[inst.operand[0].r] = *(u64*)(reg_f32 + inst.operand[1].r) & TYPE_MASKS[inst.type[0]];
			} else if(type_0_f32 && type_1_f64) {
				reg_f32[inst.operand[0].r] = *(float*)(reg_f64 + inst.operand[1].r);
			} else if(type_0_f64 && type_1_f32) {
				reg_f64[inst.operand[0].r] = *(double*)(reg_f32 + inst.operand[1].r);
			} else if(type_0_ptr && type_1_int) {
				reg_ptr[inst.operand[0].r] = reg[inst.operand[1].r] & TYPE_MASKS[inst.type[1]];
			} else if(type_0_int && type_1_ptr) {
				reg[inst.operand[0].r] = reg_ptr[inst.operand[1].r] & TYPE_MASKS[inst.type[0]];
			} else {
				UNREACHABLE;
			}
			break;
		case JIROP_TYPECAST:
			if(type_0_int && type_1_int) {
				reg[inst.operand[0].r] = reg[inst.operand[1].r] & TYPE_MASKS[inst.type[1]];
			} else if(type_0_f64 && type_1_int) {
				reg_f64[inst.operand[0].r] = (double)(reg[inst.operand[1].r] & TYPE_MASKS[inst.type[1]]);
			} else if(type_0_f32 && type_1_int) {
				reg_f32[inst.operand[0].r] = (float)(reg[inst.operand[1].r] & TYPE_MASKS[inst.type[1]]);
			} else if(type_0_int && type_1_f64) {
				reg[inst.operand[0].r] = (s64)reg_f64[inst.operand[1].r] & TYPE_MASKS[inst.type[0]];
			} else if(type_0_int && type_1_f32) {
				reg[inst.operand[0].r] = (s64)reg_f32[inst.operand[1].r] & TYPE_MASKS[inst.type[0]];
			} else if(type_0_f32 && type_1_f64) {
				reg_f32[inst.operand[0].r] = (float)reg_f64[inst.operand[1].r];
			} else if(type_0_f64 && type_1_f32) {
				reg_f64[inst.operand[0].r] = (double)reg_f32[inst.operand[1].r];
			} else if(type_0_ptr && type_1_int) {
				reg_ptr[inst.operand[0].r] = reg[inst.operand[1].r] & TYPE_MASKS[inst.type[1]];
			} else if(type_0_int && type_1_ptr) {
				reg[inst.operand[0].r] = reg_ptr[inst.operand[1].r] & TYPE_MASKS[inst.type[0]];
			} else {
				UNREACHABLE;
			}
			break;
		case JIROP_NOT:
			reg[inst.operand[0].r] = ~iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_NEG:
			reg[inst.operand[0].r] = -iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_FNEG:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = -f64right;
			else
				reg_f32[inst.operand[0].r] = -f32right;
			break;
		case JIROP_ADD:
			reg[inst.operand[0].r] = ileft + iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_SUB:
			reg[inst.operand[0].r] = ileft - iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_MUL:
			reg[inst.operand[0].r] = ileft * iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_DIV:
			reg[inst.operand[0].r] = ileft / iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_MOD:
			reg[inst.operand[0].r] = ileft % iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_AND:
			reg[inst.operand[0].r] = ileft & iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_OR:
			reg[inst.operand[0].r] = ileft | iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_XOR:
			reg[inst.operand[0].r] = ileft ^ iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_LSHIFT:
			reg[inst.operand[0].r] = ileft << iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_RSHIFT:
			reg[inst.operand[0].r] = ileft >> iright;
			reg[inst.operand[0].r] &= imask;
			break;
		case JIROP_EQ:
			reg[inst.operand[0].r] = (bool)(ileft == iright);
			break;
		case JIROP_NE:
			reg[inst.operand[0].r] = (bool)(ileft != iright);
			break;
		case JIROP_LE:
			if(issignedint)
				reg[inst.operand[0].r] = (bool)((s64)ileft <= (s64)iright);
			else
				reg[inst.operand[0].r] = ileft <= iright;
			break;
		case JIROP_GT:
			if(issignedint)
				reg[inst.operand[0].r] = (bool)((s64)ileft > (s64)iright);
			else
				reg[inst.operand[0].r] = ileft > iright;
			break;
		case JIROP_LT:
			if(issignedint)
				reg[inst.operand[0].r] = (bool)((s64)ileft < (s64)iright);
			else
				reg[inst.operand[0].r] = ileft < iright;
			break;
		case JIROP_GE:
			if(issignedint)
				reg[inst.operand[0].r] = (bool)((s64)ileft >= (s64)iright);
			else
				reg[inst.operand[0].r] = ileft >= iright;
			break;
		case JIROP_JMP:
			newpc = (s64)pc + inst.operand[2].jump;
			break;
		case JIROP_BRANCH:
			if(reg[inst.operand[0].r])
				newpc = (s64)pc + inst.operand[2].jump;
			break;
		case JIROP_MOVE:
			if(inst.type[0] == JIRTYPE_F32) {
				reg_f32[inst.operand[0].r] = f32right;
			} else if(inst.type[0] == JIRTYPE_F64) {
				reg_f64[inst.operand[0].r] = f64right;
			} else if(inst.type[0] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP) {
				reg_ptr[inst.operand[0].r] = ptr_right;
			} else {
				reg[inst.operand[0].r] = iright;
			}
			break;
		case JIROP_LOAD:
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[2])] <= reg_ptr[inst.operand[2].r]) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
			}
			ptr = segtable[PTRTYPE_TO_SEGMENT(inst.type[2])] + reg_ptr[inst.operand[2].r];
			if(inst.type[0] == JIRTYPE_F32) {
				reg_f32[inst.operand[0].r] = *(float*)(ptr);
			} else if(inst.type[0] == JIRTYPE_F64) {
				reg_f64[inst.operand[0].r] = *(double*)(ptr);
			} else {
				imask = TYPE_MASKS[inst.type[0]];
				reg[inst.operand[0].r] = *(u64*)(ptr) & imask;
				if(issignedint)
					reg[inst.operand[0].r] = SIGN_EXTEND(reg[inst.operand[0].r], TYPE_BITS[inst.type[0]]);
			}
			break;
		case JIROP_STOR:
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[2])] <= reg_ptr[inst.operand[2].r]) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
			}
			ptr = segtable[PTRTYPE_TO_SEGMENT(inst.type[2])] + reg_ptr[inst.operand[2].r];
			if(inst.type[0] == JIRTYPE_F32) {
				*(float*)(ptr) = reg_f32[inst.operand[0].r];
			} else if(inst.type[0] == JIRTYPE_F64) {
				*(double*)(ptr) = reg_f64[inst.operand[0].r];
			} else {
				imask = TYPE_MASKS[inst.type[0]];
				*(u64*)(ptr) &= ~imask;
				*(u64*)(ptr) |= reg[inst.operand[0].r] & imask;
			}
			break;
		case JIROP_LOCAL:
			if(inst.type[0] != JIRTYPE_PTR_LOCAL) {
				run = JIR_errortrace("ERROR: INVALID POINTER TYPE TO LOCAL\n",
					image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
			}
			if(inst.immediate[2]) {
				reg_ptr[inst.operand[0].r] = local_pos;
				local_pos += inst.operand[2].imm_u64;
			} else {
				reg_ptr[inst.operand[0].r] = local_pos;
				local_pos += TYPE_SIZES[inst.type[0]];
			}
			if(local_pos >= segsize[0]) {
				run = JIR_errortrace("ERROR: LOCAL OVERFLOW\n",
					image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
			}
			break;
		case JIROP_GROWHEAP:
			segtable[JIRSEG_HEAP] = realloc(segtable[JIRSEG_HEAP], segsize[JIRSEG_HEAP]*iright*sizeof(u8));
			memset(segtable[JIRSEG_HEAP]+segsize[JIRSEG_HEAP],0,segsize[JIRSEG_HEAP]*(iright-1));
			segsize[2] *= iright;
			break;
		case JIROP_FADD:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = reg_f64[inst.operand[1].r] + f64right;
			else
				reg_f32[inst.operand[0].r] = reg_f32[inst.operand[1].r] + f32right;
			break;
		case JIROP_FSUB:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = reg_f64[inst.operand[1].r] - f64right;
			else
				reg_f32[inst.operand[0].r] = reg_f32[inst.operand[1].r] - f32right;
			break;
		case JIROP_FMUL:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = reg_f64[inst.operand[1].r] * f64right;
			else
				reg_f32[inst.operand[0].r] = reg_f32[inst.operand[1].r] * f32right;
			break;
		case JIROP_FDIV:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = reg_f64[inst.operand[1].r] / f64right;
			else
				reg_f32[inst.operand[0].r] = reg_f32[inst.operand[1].r] / f32right;
			break;
		case JIROP_FMOD:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = fmod(reg_f64[inst.operand[1].r], f64right);
			else
				reg_f32[inst.operand[0].r] = fmodf(reg_f32[inst.operand[1].r], f32right);
			break;
		case JIROP_FEQ:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = (reg_f64[inst.operand[1].r] == f64right);
			else
				reg_f32[inst.operand[0].r] = (reg_f32[inst.operand[1].r] == f32right);
			break;
		case JIROP_FNE:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = (reg_f64[inst.operand[1].r] != f64right);
			else
				reg_f32[inst.operand[0].r] = (reg_f32[inst.operand[1].r] != f32right);
			break;
		case JIROP_FLE:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = reg_f64[inst.operand[1].r] <= f64right;
			else
				reg_f32[inst.operand[0].r] = reg_f32[inst.operand[1].r] <= f32right;
			break;
		case JIROP_FGT:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = reg_f64[inst.operand[1].r] > f64right;
			else
				reg_f32[inst.operand[0].r] = reg_f32[inst.operand[1].r] > f32right;
			break;
		case JIROP_FLT:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = reg_f64[inst.operand[1].r] < f64right;
			else
				reg_f32[inst.operand[0].r] = reg_f32[inst.operand[1].r] < f32right;
			break;
		case JIROP_FGE:
			if(inst.type[0] == JIRTYPE_F64)
				reg_f64[inst.operand[0].r] = reg_f64[inst.operand[1].r] >= f64right;
			else
				reg_f32[inst.operand[0].r] = reg_f32[inst.operand[1].r] >= f32right;
			break;
		case JIROP_SETPORT:
		case JIROP_SETARG:
		case JIROP_SETRET:
			if(inst.type[0] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP) {
				port_ptr[inst.operand[0].r] = ptr_right;
			} else if(inst.type[0] == JIRTYPE_F32) {
				port_f32[inst.operand[0].r] = f32right;
			} else if(inst.type[0] == JIRTYPE_F64) {
				port_f64[inst.operand[0].r] = f64right;
			} else {
				port[inst.operand[0].r] = iright;
			}
			port_types[inst.operand[0].r] = inst.type[0];
			break;
		case JIROP_GETPORT:
		case JIROP_GETARG:
		case JIROP_GETRET:
			iright = port[inst.operand[2].r] & imask;
			if(issignedint)
				iright = SIGN_EXTEND(iright, TYPE_BITS[inst.type[0]]);

			if(inst.type[0] >= JIRTYPE_PTR_LOCAL && inst.type[0] <= JIRTYPE_PTR_HEAP) {
				reg_ptr[inst.operand[0].r] = port_ptr[inst.operand[2].r];
			} else if(inst.type[0] == JIRTYPE_F32) {
				reg_f32[inst.operand[0].r] = port_f32[inst.operand[2].r];
			} else if(inst.type[0] == JIRTYPE_F64) {
				reg_f64[inst.operand[0].r] = port_f64[inst.operand[2].r];
			} else {
				reg[inst.operand[0].r] = iright;
			}
			port_types[inst.operand[0].r] = inst.type[0];
			break;
		case JIROP_CALL:
			proclabelstack[calldepth] = proclabel;
			pcstack[calldepth] = newpc; // store newpc so we return to the next inst
			++calldepth;
			newpc = pc + inst.operand[2].jump;
			localbase[localbase_pos++] = local_pos;
			if(localbase_pos >= localbasesize) {
				run = JIR_errortrace("ERROR: LOCAL BASE OVERFLOW\n",
					image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
			}
			break;
		case JIROP_RET:
			if(calldepth == 0) {
				run = false;
			} else {
				--calldepth;
				proclabel = proclabelstack[calldepth];
				newpc = pcstack[calldepth];
				local_pos = localbase[--localbase_pos];
			}
			break;
		case JIROP_SYSCALL:
			switch(iright) {
#if defined(__linux__) || defined(__APPLE__)
			/*
			 * Linux syscalls
			 *
			 * 0	sys_read	unsigned int fd      char *buf	        size_t count
			 * 1	sys_write	unsigned int fd      const char *buf	size_t count
			 * 2	sys_open	const char *filename int flags          int mode
			 * 3	sys_close	unsigned int fd
			 */
			case 0: // sys_read(unsigned int fd, char *buf, size_t count)
				if(segsize[PTRTYPE_TO_SEGMENT(port_types[1])] <= port_ptr[1]) {
					run = JIR_errortrace("ERROR: INVALID POINTER\n",
							image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
				}
				ptr = segtable[PTRTYPE_TO_SEGMENT(port_types[1])] + port_ptr[1];
				syscall_success = port[0] = read(
					(unsigned int)port[0],
					(char*)ptr,
					(size_t)port[2]);
				port_types[0] = JIRTYPE_U64;
				break;
			case 1: // sys_write(unsigned int fd, const char *buf, size_t count)
				if(segsize[PTRTYPE_TO_SEGMENT(port_types[1])] <= port_ptr[1]) {
					run = JIR_errortrace("ERROR: INVALID POINTER\n",
							image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
				}
				ptr = segtable[PTRTYPE_TO_SEGMENT(port_types[1])] + port_ptr[1];
				syscall_success = port[0] = write(
					(unsigned int)port[0],
					(char*)ptr,
					(size_t)port[2]);
				port_types[0] = JIRTYPE_U64;
				break;
			case 2: // sys_open(const char *filenamem, int flags, int mode)
				if(segsize[PTRTYPE_TO_SEGMENT(port_types[0])] <= port_ptr[0]) {
					run = JIR_errortrace("ERROR: INVALID POINTER\n",
							image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
				}
				ptr = segtable[PTRTYPE_TO_SEGMENT(port_types[0])] + port_ptr[0];
				syscall_success = port[0] = open(
					(char*)ptr,
					(int)port[1],
					(int)port[2]);
				port_types[0] = JIRTYPE_S32;
				break;
			case 3: // sys_close(unsigned int fd)
				syscall_success = close(
					(unsigned int)port[0]);
				break;
#else
			default:
				UNREACHABLE; // syscalls not implemented for this OS
#endif
			}
			break;
		case JIROP_HALT:
			run = false;
			break;
		}

		if(syscall_success == -1) {
			run = JIR_errortrace("ERROR: SYSCALL FAILED\n", image->prog, pc, proclabel, pcstack, proclabelstack, calldepth);
		}
	}

	image->local_pos = local_pos;
	image->localbase_pos = localbase_pos;
	image->heap = segtable[2];
	image->heapsize = segsize[2];
}

JINLINE void JIRIMAGE_init(JIRIMAGE *i, JIR *prog, u64 proglen) {
	i->prog = prog;
	i->proglen = proglen;
	i->local = calloc(0x1000, sizeof(u8));
	i->global = calloc(0x1000, sizeof(u8));
	i->heap = calloc(0x1000, sizeof(u8));
	i->localsize = 0x1000;
	i->globalsize = 0x1000;
	i->heapsize = 0x1000;
	i->localbasesize = 0x80;
	i->localbase = calloc(0x80, sizeof(u64));
	i->localbase_pos = 0;
	i->local_pos = 0;
	i->reg = malloc(64 * sizeof(u64));
	i->reg_ptr = malloc(64 * sizeof(u64));
	i->reg_f32 = malloc(64 * sizeof(f32));
	i->reg_f64 = malloc(64 * sizeof(f64));
	i->port_types = malloc(64 * sizeof(JIRTYPE));
	i->port = malloc(64 * sizeof(u64));
	i->port_ptr = malloc(64 * sizeof(u64));
	i->port_f32 = malloc(64 * sizeof(f32));
	i->port_f64 = malloc(64 * sizeof(f64));
}

JINLINE void JIRIMAGE_destroy(JIRIMAGE *i) {
	free(i->local);
	free(i->global);
	free(i->heap);
	free(i->localbase);
	free(i->reg);
	free(i->reg_ptr);
	free(i->reg_f32);
	free(i->reg_f64);
	free(i->port_types);
	free(i->port);
	free(i->port_ptr);
	free(i->port_f32);
	free(i->port_f64);
	*i = (JIRIMAGE){0};
}

/*
void JIR_translate_c(char **Cbuf, u64 *Cbuflen, JIR **proctab, u64 nprocs) {
	assert(Cbuf && *Cbuf);
	size_t bytesleft = snprintf(*Cbuf, *Cbuflen, 
			"#include <stdio.h> \n\
			 #include <stdlib.h> \n\
			 #include <math.h> \n\
			 #include <stdint.h>\n\
			 
	X(HALT)    \
	X(AND)     \
	X(OR)      \
	X(XOR)     \
	X(LSHIFT)  \
	X(RSHIFT)  \
	X(NOT)     \
	X(ADD)     \
	X(SUB)     \
	X(MUL)     \
	X(DIV)     \
	X(MOD)     \
	X(NEG)     \
	X(EQ)      \
	X(NE)      \
	X(LE)      \
	X(GT)      \
	X(LT)      \
	X(GE)      \
	X(FADD)    \
	X(FSUB)    \
	X(FMUL)    \
	X(FDIV)    \
	X(FMOD)    \
	X(FNEG)    \
	X(FEQ)     \
	X(FNE)     \
	X(FLE)     \
	X(FGT)     \
	X(FLT)     \
	X(FGE)     \
	X(BRANCH)  \
	X(JMP)     \
	X(LABEL)   \
	X(LOAD)    \
	X(STOR)    \
	X(MOVE)    \
	X(LOCAL)   \
	X(CALL)    \
	X(RET)     \
	X(SYSCALL) \
	X(SETARG)  \
	X(GETARG)  \
	X(SETRET)  \
	X(GETRET)  \
	X(SETPORT) \
	X(GETPORT) \
	X(GROWHEAP)\
	X(BITCAST) \
	X(TYPECAST)\
	X(DUMPREG) \
	X(DUMPPORT)\
	X(DUMPMEM) \
	X(DEBUGMSG)\
	X(NOOP)
}
*/

#endif

#endif
