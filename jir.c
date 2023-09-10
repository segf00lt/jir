#define JLIB_FMAP_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include "basic.h"

#define OPCODES         \
	X(HALT)         \
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
	X(IOREAD)       \
	X(IOWRITE)      \
	X(IOOPEN)       \
	X(IOCLOSE)      \
	X(GROWHEAP)     \
	X(BITCAST)      \
	X(TYPECAST)     \
	X(DUMPREG)      \
	X(DUMPPORT)     \
	X(DUMPMEM)      \
	X(DEBUGMSG)

/* TODO
 *
 * better testing
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

#define TOKEN_TO_OP(t) (JIROP)(t - TOKEN_HALT)
#define TOKEN_TO_TYPE(t) (JIRTYPE)((int)t - (int)TOKEN_S64)
#define TOKEN_TO_SEGMENT(t) (JIRSEG)(t - TOKEN_LOCAL)
#define PTRTYPE_TO_SEGMENT(pt) (JIRSEG)(pt - JIRTYPE_PTR_LOCAL)

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
	u64 procid;
	u64 offset;
};
typedef union JIROPERAND JIROPERAND;

struct JIR {
	JIROP opcode;
	JIRTYPE type[3];
	JIROPERAND operand[3];
	bool immediate[3];
	const char *debugmsg;
};
typedef struct JIR JIR;

struct JIRIMAGE {
	Arr(JIR) *proctab;
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
	// ports are special registers for passing data between procedures, and in and out of the interpreter
	JIRTYPE *port_types;
	u64 *port;
	u64 *port_ptr;
	float *port_f32;
	double *port_f64;
};
typedef struct JIRIMAGE JIRIMAGE;

#define OPERAND(field, data) (JIROPERAND){ .field = data }
#define PORT(index) (JIROPERAND){ .r = index }
#define REG(index) (JIROPERAND){ .r = index }
#define IMMU64(data) (JIROPERAND){ .imm_u64 = data }
#define IMMS64(data) (JIROPERAND){ .imm_s64 = data }
#define IMMF32(data) (JIROPERAND){ .imm_f32 = data }
#define IMMF64(data) (JIROPERAND){ .imm_f64 = data }
#define PTR(data) (JIROPERAND){ .offset = data }

#define DUMPMEMOP(t, imm_start, imm_end, start, end)\
	(JIR){\
		.opcode = JIROP_DUMPMEM,\
		.type = {JIRTYPE_##t, JIRTYPE_##t, JIRTYPE_##t},\
		.operand = { {0}, start, end },\
		.immediate = {0, imm_start, imm_end },\
		.debugmsg = NULL,\
	}\

#define DEBUGMSGOP(msg)\
	(JIR){\
		.opcode = JIROP_DEBUGMSG,\
		.type = {0},\
		.operand = {0},\
		.immediate = {0},\
		.debugmsg = msg,\
	}\

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

#define BRANCHOP(cond, offset)\
	(JIR){\
		.opcode = JIROP_BRANCH,\
		.type = {0},\
		.operand[0] = cond,\
		.operand[1] = {0},\
		.operand[2] = offset,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define BRANCHOPIMM(cond, offset)\
	(JIR){\
		.opcode = JIROP_BRANCH,\
		.type = {0},\
		.operand[0] = cond,\
		.operand[1] = {0},\
		.operand[2] = offset,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define JMPOP(offset)\
	(JIR){\
		.opcode = JIROP_JMP,\
		.type = {0},\
		.operand[0] = {0},\
		.operand[1] = {0},\
		.operand[2] = offset,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define JMPOPIMM(offset)\
	(JIR){\
		.opcode = JIROP_JMP,\
		.type = {0},\
		.operand[0] = {0},\
		.operand[1] = {0},\
		.operand[2] = offset,\
		.immediate = { false, false, true },\
		.debugmsg = NULL,\
	}

#define LOADOP(t_dest, t_src, dest, src)\
	(JIR){\
		.opcode = JIROP_LOAD,\
		.type = { JIRTYPE_##t_dest, JIRTYPE_##t_src, 0 },\
		.operand[0] = dest,\
		.operand[1] = src,\
		.operand[2] = {0},\
		.immediate = { false, false, false },\
		.debugmsg = NULL,\
	}

#define STOROP(t_dest, t_src, dest, src)\
	(JIR){\
		.opcode = JIROP_STOR,\
		.type = { JIRTYPE_##t_dest, JIRTYPE_##t_src, 0 },\
		.operand[0] = dest,\
		.operand[1] = src,\
		.operand[2] = {0},\
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

#define BFRAMEOP (JIR){ .opcode = JIROP_BFRAME, .type = {0}, .operand = {0}, .immediate = {0}, .debugmsg = NULL, }
#define EFRAMEOP (JIR){ .opcode = JIROP_EFRAME, .type = {0}, .operand = {0}, .immediate = {0}, .debugmsg = NULL, }
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

#define ALLOCOP(t, dest)\
	(JIR){\
		.opcode = JIROP_ALLOC,\
		.type = { JIRTYPE_PTR_LOCAL, JIRTYPE_##t, 0 },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define ALLOCOPIMM(size, dest)\
	(JIR){\
		.opcode = JIROP_ALLOC,\
		.type = { JIRTYPE_PTR_LOCAL, JIRTYPE_U64,0 },\
		.operand[0] = dest,\
		.operand[1] = {0},\
		.operand[2] = size,\
		.immediate = { false, false, true },\
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

#define CALLOP(procid)\
	(JIR){\
		.opcode = JIROP_CALL,\
		.type = {0},\
		.operand[0] = procid,\
		.operand[1] = {0},\
		.operand[2] = {0},\
		.immediate = { true, false, false },\
		.debugmsg = NULL,\
	}

#define CALLOPINDIRECT(procid)\
	(JIR){\
		.opcode = JIROP_CALL,\
		.type = {0},\
		.operand[0] = procid,\
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

#define IOREADOP(t_ptr, fd, ptr, count)\
	(JIR){\
		.opcode = JIROP_IOREAD,\
		.type = { JIRTYPE_S64, JIRTYPE_##t_ptr, JIRTYPE_U64 },\
		.operand[0] = fd,\
		.operand[1] = ptr,\
		.operand[2] = count,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define IOWRITEOP(t_ptr, fd, ptr, count)\
	(JIR){\
		.opcode = JIROP_IOWRITE,\
		.type = { JIRTYPE_S64, JIRTYPE_##t_ptr, JIRTYPE_U64 },\
		.operand[0] = fd,\
		.operand[1] = ptr,\
		.operand[2] = count,\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define IOOPENOP(t_ptr, ptr, flags)\
	(JIR){\
		.opcode = JIROP_IOOPEN,\
		.type = { JIRTYPE_##t_ptr, JIRTYPE_S64,0 },\
		.operand[0] = ptr,\
		.operand[1] = flags,\
		.operand[2] = {0},\
		.immediate = {0},\
		.debugmsg = NULL,\
	}

#define IOCLOSEOP(fd)\
	(JIR){\
		.opcode = JIROP_IOCLOSE,\
		.type = { JIRTYPE_U64,0,0 },\
		.operand[0] = fd,\
		.operand[1] = {0},\
		.operand[2] = {0},\
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

void JIR_print(JIR inst) {
	printf("(JIR) {\n\
	.opcode = JIROP_%s,\n\
	.type[0] = JIRTYPE_%s,\n\
	.operand[0] = { .r = %i, .imm_u64 = %lu, .imm_f32 = %f, .imm_f64 = %lf, .procid = %lu },\n\
	.immediate[0] = %i,\n\
	.type[1] = JIRTYPE_%s,\n\
	.operand[1] = { .r = %i, .imm_u64 = %lu, .imm_f32 = %f, .imm_f64 = %lf, .procid = %lu },\n\
	.immediate[1] = %i,\n\
	.type[2] = JIRTYPE_%s,\n\
	.operand[2] = { .r = %i, .imm_u64 = %lu, .imm_f32 = %f, .imm_f64 = %lf, .procid = %lu },\n\
	.immediate[2] = %i,\n\
	.debugmsg = %s,\n}\n",
	opcodesdebug[inst.opcode],
	typesdebug[inst.type[0]],
	inst.operand[0].r,
	inst.operand[0].imm_u64,
	inst.operand[0].imm_f32,
	inst.operand[0].imm_f64,
	inst.operand[0].procid,
	inst.immediate[0],
	typesdebug[inst.type[1]],
	inst.operand[1].r,
	inst.operand[1].imm_u64,
	inst.operand[1].imm_f32,
	inst.operand[1].imm_f64,
	inst.operand[1].procid,
	inst.immediate[1],
	typesdebug[inst.type[2]],
	inst.operand[2].r,
	inst.operand[2].imm_u64,
	inst.operand[2].imm_f32,
	inst.operand[2].imm_f64,
	inst.operand[2].procid,
	inst.immediate[2],
	inst.debugmsg);
}

bool JIR_errortrace(char *msg, JIR **proctab, u64 pc, u64 procid, u64 *pcstack, u64 *procidstack, u64 calldepth) {
	printf("JIR TRACE: ERROR TRACING\n");
	pcstack[calldepth] = pc + 1;
	procidstack[calldepth] = procid;
	JIR inst;
	for(u64 i = 0; i <= calldepth; ++i) {
		pc = pcstack[calldepth-i] - 1;
		procid = procidstack[calldepth-i];
		inst = proctab[procid][pc];
		printf("INSTRUCTION %lu, PROC %lu:\n", pc, procid);
		JIR_print(inst);
	}
	fputs(msg, stdout);
	return false;
}

void JIR_exec(JIRIMAGE *image) {

	u64 reg[32] = {0};
	u64 reg_ptr[32] = {0};
	float reg_f32[32] = {0};
	double reg_f64[32] = {0};

	JIRTYPE *port_types = image->port_types;
	u64 *port = image->port;
	u64 *port_ptr = image->port_ptr;
	float *port_f32 = image->port_f32;
	double *port_f64 = image->port_f64;

	u64 iarithmasks[] = {
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

	u64 iarithbits[] = {
		64, 32, 16, 8, 
		64, 32, 16, 8, 
		64, // PTR_LOCAL
		64, // PTR_GLOBAL
		64, // PTR_HEAP
		0, // F32
		0, // F64
	};

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

	u64 procidstack[64] = {0};
	u64 pcstack[64] = {0};
	u64 calldepth = 0;

	u64 iocallval = 0;
	char *iocallerror = NULL;
	bool run = true;
	bool issignedint = false;
	u64 procid = 0;
	u64 pc = 0;
	u64 newpc = 0;
	
	JIR *proc = image->proctab[procid];

	while(run) {
		pc = newpc;
		JIR inst = proc[pc];
#ifdef DEBUG
		printf("JIR TRACE: EXECUTION LOG\nINSTRUCTION %lu, PROC %lu:\n", pc, procid);
		JIR_print(inst);
#endif
		// NOTE we should write a JIR_verify() function to do checks (like these ones)
		if(inst.opcode >= JIROP_FADD && inst.opcode <= JIROP_FGE && inst.type[0] < JIRTYPE_F32) {
			run = JIR_errortrace("ERROR: INVALID TYPE TO FLOATING POINT INSTRUCTION\n",
				image->proctab, pc, procid, pcstack, procidstack, calldepth);
		}
		if(inst.opcode >= JIROP_ADD && inst.opcode <= JIROP_GE && inst.type[0] >= JIRTYPE_F32) {
			run = JIR_errortrace("ERROR: INVALID TYPE TO INTEGER INSTRUCTION\n",
				image->proctab, pc, procid, pcstack, procidstack, calldepth);
		}
		newpc = pc + 1;
		u64 imask = iarithmasks[inst.type[0]];
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
			ileft = SIGN_EXTEND(ileft, iarithbits[inst.type[0]]);
			iright = SIGN_EXTEND(iright, iarithbits[inst.type[0]]);
		}

		switch(inst.opcode) {
		default:
			run = JIR_errortrace("ERROR: INVALID OPCODE\n",
				image->proctab, pc, procid, pcstack, procidstack, calldepth);
			break;
		case JIROP_DUMPMEM:
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[0])] <= ptr_left ||
			   segsize[PTRTYPE_TO_SEGMENT(inst.type[0])] <= ptr_right) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
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
					printf("\n");
			}
			if((ptr_right-ptr_left) & 0x7)
				printf("\n");
			printf("========================\n");
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
						iarithmasks[inst.type[0]]&reg[inst.operand[0].r]);
			break;

		case JIROP_BITCAST:
			if(type_0_int && type_1_int) {
				reg[inst.operand[0].r] = reg[inst.operand[1].r] & iarithmasks[inst.type[1]];
			} else if(type_0_f64 && type_1_int) {
				iright = reg[inst.operand[1].r] & iarithmasks[inst.type[1]];
				reg_f64[inst.operand[0].r] = *(double*)&iright;
			} else if(type_0_f32 && type_1_int) {
				iright = reg[inst.operand[1].r] & iarithmasks[inst.type[1]];
				reg_f32[inst.operand[0].r] = *(float*)&iright;
			} else if(type_0_int && type_1_f64) {
				reg[inst.operand[0].r] = *(u64*)(reg_f64 + inst.operand[1].r) & iarithmasks[inst.type[0]];
			} else if(type_0_int && type_1_f32) {
				reg[inst.operand[0].r] = *(u64*)(reg_f32 + inst.operand[1].r) & iarithmasks[inst.type[0]];
			} else if(type_0_f32 && type_1_f64) {
				reg_f32[inst.operand[0].r] = *(float*)(reg_f64 + inst.operand[1].r);
			} else if(type_0_f64 && type_1_f32) {
				reg_f64[inst.operand[0].r] = *(double*)(reg_f32 + inst.operand[1].r);
			} else if(type_0_ptr && type_1_int) {
				reg_ptr[inst.operand[0].r] = reg[inst.operand[1].r] & iarithmasks[inst.type[1]];
			} else if(type_0_int && type_1_ptr) {
				reg[inst.operand[0].r] = reg_ptr[inst.operand[1].r] & iarithmasks[inst.type[0]];
			} else {
				run = JIR_errortrace("ERROR: ILLEGAL BITCAST\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
			break;

		case JIROP_TYPECAST:
			if(type_0_int && type_1_int) {
				reg[inst.operand[0].r] = reg[inst.operand[1].r] & iarithmasks[inst.type[1]];
			} else if(type_0_f64 && type_1_int) {
				reg_f64[inst.operand[0].r] = (double)(reg[inst.operand[1].r] & iarithmasks[inst.type[1]]);
			} else if(type_0_f32 && type_1_int) {
				reg_f32[inst.operand[0].r] = (float)(reg[inst.operand[1].r] & iarithmasks[inst.type[1]]);
			} else if(type_0_int && type_1_f64) {
				reg[inst.operand[0].r] = (s64)reg_f64[inst.operand[1].r] & iarithmasks[inst.type[0]];
			} else if(type_0_int && type_1_f32) {
				reg[inst.operand[0].r] = (s64)reg_f32[inst.operand[1].r] & iarithmasks[inst.type[0]];
			} else if(type_0_f32 && type_1_f64) {
				reg_f32[inst.operand[0].r] = (float)reg_f64[inst.operand[1].r];
			} else if(type_0_f64 && type_1_f32) {
				reg_f64[inst.operand[0].r] = (double)reg_f32[inst.operand[1].r];
			} else if(type_0_ptr && type_1_int) {
				reg_ptr[inst.operand[0].r] = reg[inst.operand[1].r] & iarithmasks[inst.type[1]];
			} else if(type_0_int && type_1_ptr) {
				reg[inst.operand[0].r] = reg_ptr[inst.operand[1].r] & iarithmasks[inst.type[0]];
			} else {
				run = JIR_errortrace("ERROR: ILLEGAL BITCAST\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
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
			if(inst.type[0] < JIRTYPE_F32) {
				run = JIR_errortrace("ERROR: INVALID TYPE TO FLOATING POINT INSTRUCTION\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
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
			newpc = pc + iright;
			break;
		case JIROP_BRANCH:
			if(reg[inst.operand[0].r] == 0)
				iright = 1;
			newpc = pc + iright;
			break;

		case JIROP_MOVE:
			if(inst.type[0] != inst.type[2]) {
				run = JIR_errortrace("ERROR: INCOMPATIBLE OPERAND TYPES TO MOVE INSTRUCTION\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
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
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[1])] <= reg_ptr[inst.operand[1].r]) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
			ptr = segtable[PTRTYPE_TO_SEGMENT(inst.type[1])] + reg_ptr[inst.operand[1].r];
			if(inst.type[0] == JIRTYPE_F32) {
				reg_f32[inst.operand[0].r] = *(float*)(ptr);
			} else if(inst.type[0] == JIRTYPE_F64) {
				reg_f64[inst.operand[0].r] = *(double*)(ptr);
			} else {
				imask = iarithmasks[inst.type[0]];
				reg[inst.operand[0].r] = *(u64*)(ptr) & imask;
				if(issignedint)
					reg[inst.operand[0].r] = SIGN_EXTEND(reg[inst.operand[0].r], iarithbits[inst.type[0]]);
			}
			break;
		case JIROP_STOR:
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[0])] <= reg_ptr[inst.operand[0].r]) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
			ptr = segtable[PTRTYPE_TO_SEGMENT(inst.type[0])] + reg_ptr[inst.operand[0].r];
			if(inst.type[1] == JIRTYPE_F32) {
				*(float*)(ptr) = reg_f32[inst.operand[1].r];
			} else if(inst.type[1] == JIRTYPE_F64) {
				*(double*)(ptr) = reg_f64[inst.operand[1].r];
			} else {
				imask = iarithmasks[inst.type[1]];
				*(u64*)(ptr) &= ~imask;
				*(u64*)(ptr) |= reg[inst.operand[1].r] & imask;
			}
			break;
		case JIROP_BFRAME:
			localbase[localbase_pos++] = local_pos;
			if(localbase_pos >= localbasesize) {
				run = JIR_errortrace("ERROR: LOCAL BASE OVERFLOW\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
			break;
		case JIROP_EFRAME:
			local_pos = localbase[--localbase_pos];
			break;
		case JIROP_ALLOC:
			if(inst.type[0] != JIRTYPE_PTR_LOCAL) {
				run = JIR_errortrace("ERROR: INVALID POINTER TYPE TO ALLOC\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
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
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
			break;
		case JIROP_GROWHEAP:
			segtable[2] = realloc(segtable[2], segsize[2]*iright*sizeof(u8));
			memset(segtable[2]+segsize[2],0,segsize[2]*(iright-1));
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
				iright = SIGN_EXTEND(iright, iarithbits[inst.type[0]]);

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
			procidstack[calldepth] = procid;
			pcstack[calldepth] = newpc; // store newpc so we return to the next inst
			++calldepth;
			if(inst.immediate[0])
				procid = inst.operand[0].imm_u64;
			else
				procid = reg[inst.operand[0].r];
			proc = image->proctab[procid];
			newpc = 0;
			break;
		case JIROP_RET:
			--calldepth;
			procid = procidstack[calldepth];
			newpc = pcstack[calldepth];
			proc = image->proctab[procid];
			break;

		case JIROP_IOREAD:
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[1])] <= reg_ptr[inst.operand[1].r]) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
			ptr = segtable[PTRTYPE_TO_SEGMENT(inst.type[1])] + reg_ptr[inst.operand[1].r];
			iocallval = read((unsigned int)reg[inst.operand[0].r],(char*)ptr,(size_t)reg[inst.operand[2].r]);
			iocallerror = "ERROR: IOREAD FAILED\n";
			break;
		case JIROP_IOWRITE:
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[1])] <= reg_ptr[inst.operand[1].r]) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
			ptr = segtable[PTRTYPE_TO_SEGMENT(inst.type[1])] + reg_ptr[inst.operand[1].r];
			iocallval = write((unsigned int)reg[inst.operand[0].r],(void*)ptr,(size_t)reg[inst.operand[2].r]);
			iocallerror = "ERROR: IOWRITE FAILED\n";
			break;
		case JIROP_IOOPEN:
			if(segsize[PTRTYPE_TO_SEGMENT(inst.type[0])] <= reg_ptr[inst.operand[0].r]) {
				run = JIR_errortrace("ERROR: INVALID POINTER\n",
					image->proctab, pc, procid, pcstack, procidstack, calldepth);
			}
			ptr = segtable[PTRTYPE_TO_SEGMENT(inst.type[0])] + reg_ptr[inst.operand[0].r];
			iocallval = open((char*)ptr,(int)reg[inst.operand[1].r]);
			iocallerror = "ERROR: IOOPEN FAILED\n";
			break;
		case JIROP_IOCLOSE:
			iocallval = close((unsigned int)reg[inst.operand[0].r]);
			iocallerror = "ERROR: IOCLOSE FAILED\n";
			break;

		case JIROP_HALT:
			run = false;
			break;
		}

		if(iocallval == -1) {
			run = JIR_errortrace(iocallerror, image->proctab, pc, procid, pcstack, procidstack, calldepth);
		}
	}

	image->local_pos = local_pos;
	image->localbase_pos = localbase_pos;
	image->heap = segtable[2];
	image->heapsize = segsize[2];
}

void JIRIMAGE_init(JIRIMAGE *i, JIR **proctab) {
	i->proctab = proctab;
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
	i->port_types = malloc(64 * sizeof(JIRTYPE));
	i->port = malloc(64 * sizeof(u64));
	i->port_ptr = malloc(64 * sizeof(u64));
	i->port_f32 = malloc(64 * sizeof(f32));
	i->port_f64 = malloc(64 * sizeof(f64));
}

void JIRIMAGE_destroy(JIRIMAGE *i) {
	free(i->local);
	free(i->global);
	free(i->heap);
	free(i->localbase);
	free(i->port_types);
	free(i->port);
	free(i->port_ptr);
	free(i->port_f32);
	free(i->port_f64);
	*i = (JIRIMAGE){0};
}

int main(int argc, char **argv) {
	printf("\n###### testing bitcasts ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(F32, REG(2), IMMF32(112.4367)),
			CASTOP(BITCAST,U64,F32,REG(1),REG(2)),
			DUMPREGOP(F32, REG(2)),
			DUMPREGOP(U64, REG(1)),
			MOVEOPIMM(U64,REG(0),IMMU64(0xffffffffff000000)),
			CASTOP(BITCAST,F32,U64,REG(1),REG(0)),
			DUMPREGOP(U64, REG(0)),
			DUMPREGOP(F32, REG(1)),
			MOVEOPIMM(F64,REG(0),IMMF64(0.55512316)),
			CASTOP(BITCAST,F32,F64,REG(1),REG(0)),
			DUMPREGOP(F64, REG(0)),
			DUMPREGOP(F32, REG(1)),
			CASTOP(BITCAST,U64,F64,REG(1),REG(0)),
			DUMPREGOP(U64, REG(1)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab);
		JIR_exec(&image);
		JIRIMAGE_destroy(&image);
	}

	printf("\n###### testing unaryops ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(S8, REG(0), IMMU64(1)),
			UNOP(NEG,S8,REG(0),REG(0)),
			UNOP(NOT,U16,REG(0),REG(0)),
			DUMPREGOP(S8, REG(0)),
			DUMPREGOP(U16, REG(0)),
			MOVEOPIMM(F32,REG(1),IMMF32(35.412)),
			UNOP(FNEG,F32,REG(2),REG(1)),
			BINOP(FSUB,F32,REG(2),REG(2),REG(1)),
			DUMPREGOP(F32, REG(2)),
			BINOPIMM(FMUL,F32,REG(1),REG(1),IMMF32(2.0)),
			BINOP(FADD,F32,REG(3),REG(1),REG(2)),
			DUMPREGOP(F32, REG(3)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab);
		JIR_exec(&image);
		JIRIMAGE_destroy(&image);
	}
	
	printf("\n###### testing io ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(U64, REG(0), IMMU64(1)),
			MOVEOPIMM(PTR_GLOBAL, REG(1), IMMU64(0)),
			MOVEOPIMM(U64, REG(2), IMMU64(14)),
			IOWRITEOP(PTR_GLOBAL, REG(0), REG(1), REG(2)),
			DUMPMEMOP(PTR_GLOBAL, true, true, IMMU64(0), IMMU64(14)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab);
		strcpy((char*)image.global, "Hello, World!\n");
		JIR_exec(&image);
		JIRIMAGE_destroy(&image);
	}

	printf("\n###### testing float ops ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(F32, REG(1), IMMF32(1.333)),
			BINOPIMM(FMUL, F32, REG(2), REG(1), IMMF32(7.1)),
			MOVEOPIMM(PTR_GLOBAL, REG(1), IMMU64(0)),
			DUMPREGOP(F32, REG(1)),
			DUMPREGOP(F32, REG(2)),
			MOVEOPIMM(F64, REG(1), IMMF64(1.333)),
			BINOPIMM(FDIV, F64, REG(2), REG(1), IMMF64(7.1)),
			DUMPREGOP(F64, REG(1)),
			DUMPREGOP(F64, REG(2)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab);
		JIR_exec(&image);
		JIRIMAGE_destroy(&image);
	}

	printf("\n###### testing integer comparisons ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(S8, REG(1), IMMU64(-12)),
			MOVEOPIMM(S8, REG(2), IMMU64(5)),
			BINOP(LE, S8, REG(3), REG(1), REG(2)),
			DUMPREGOP(U64, REG(3)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab);
		JIR_exec(&image);
		JIRIMAGE_destroy(&image);
	}

	printf("\n###### testing procedure calls ######\n\n");
	{
		JIR proc0[] = {
			MOVEOPIMM(S64, REG(1), IMMU64(22)),
			SETARGOP(S64,PORT(1),REG(1)),
			DEBUGMSGOP("dumping port in proc0"),
			DUMPPORTOP(S64,PORT(1)),
			CALLOP(IMMU64(1)),
			HALTOP,
		};
		JIR proc1[] = {
			BFRAMEOP,
			GETARGOP(S64,REG(1),PORT(1)),
			BINOPIMM(MOD,U64,REG(2),REG(1),IMMU64(11)),
			DUMPREGOP(U64,REG(2)),
			ALLOCOP(S64,REG(3)),
			STOROP(PTR_LOCAL,S64,REG(3),REG(2)),
			BRANCHOPIMM(REG(2),IMMU64(3)),
			MOVEOPIMM(U64, REG(1), IMMU64(12)),
			SETARGOP(U64,PORT(1),REG(1)),
			DUMPREGOP(U64,REG(1)),
			DUMPPORTOP(U64,PORT(1)),
			CALLOP(IMMU64(2)),
			LOADOP(S64,PTR_LOCAL,REG(2),REG(3)),
			DUMPREGOP(U64,REG(2)),
			EFRAMEOP,
			RETOP,
			HALTOP,
		};
		JIR proc2[] = {
			GETARGOP(U64,REG(1),PORT(1)),
			DUMPPORTOP(U64,PORT(1)),
			BINOPIMM(MUL,U64,REG(2),REG(1),IMMU64(10)),
			DUMPREGOP(U64,REG(2)),
			BINOPIMM(ADD,U64,REG(2),REG(2),IMMU64(12)),
			DUMPREGOP(U64,REG(2)),
			BINOPIMM(ADD,U8,REG(2),REG(2),IMMU64(126)),
			DUMPREGOP(U64,REG(2)),
			RETOP,
			HALTOP,
		};
		JIR *proctab[8] = {proc0,proc1,proc2};
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab);
		JIR_exec(&image);
		JIRIMAGE_destroy(&image);
	}

	printf("\n###### testing heap ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(U16, REG(1), IMMU64(12)),
			MOVEOPIMM(PTR_HEAP, REG(2), PTR(0xfed)),
			STOROP(PTR_HEAP, U16, REG(2), REG(1)),
			LOADOP(U16, PTR_HEAP, REG(3), REG(2)),
			DUMPREGOP(U16, REG(3)),
			DUMPMEMOP(PTR_HEAP, false, true, REG(2), IMMU64(0xfed+18)),
			GROWHEAPOP(IMMU64(2)),
			DUMPMEMOP(PTR_HEAP, false, true, REG(2), IMMU64(0xfed+100)),
			HALTOP,
		};
		JIR *proctab[8] = {instarr};
		JIRIMAGE image;
		JIRIMAGE_init(&image, proctab);
		JIR_exec(&image);
		JIRIMAGE_destroy(&image);
	}

	return 0;
}
