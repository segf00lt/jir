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
	X(PUSH)         \
	X(POP)          \
	X(MEMALLOC)     \
	X(MEMFREE)      \
	X(MEMSET)       \
	X(SET)          \
	X(SETREG)       \
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
	X(DUMPREG)      \
	X(DUMPREGRANGE) \
	X(DUMPMEM)      \
	X(HALT)

#define TYPES  \
	X(S64) \
	X(U64) \
	X(S32) \
	X(U32) \
	X(S8)  \
	X(U8)  \
	X(S16) \
	X(U16) \
	X(F32) \
	X(F64)

#define SEGMENTS   \
	X(LOCAL)   \
	X(GLOBAL)  \
	X(ARGUMENT)\
	X(HEAP)

#define TOKEN_TO_OP(t) (JIROP)(t - TOKEN_EQ)
#define TOKEN_TO_TYPE(t) (JIRTYPE)(t - TOKEN_S64)
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
	int ra, rb, rc;
	int fa, fb, fc;
	int f64a, f64b, f64c;
	bool immediate;
	uint64_t immui;
	int64_t immsi;
	float immf;
	double immf64;
};
typedef struct JIR JIR;

#include "lex.c"

void parse(Lexer *l, JIR *instarr) {
	char debugBuf[64];
	JIR inst;
	Token token;
	char *ident = NULL;

	assert(instarr);

	/*
	 * grammar
	 *
	 * binop type dest_reg a_reg b_reg
	 * binop "immediate" type dest_reg reg imm
	 * unop type dest_reg reg
	 * castop type cast_type dest_reg reg
	 * branch cond_reg offset_reg
	 * branch "immediate" cond_reg offset_imm
	 * jmp offset_reg
	 * jmp "immediate" offset_imm
	 * load type segment dest_reg addr_reg
	 * load "immediate" type segment dest_reg addr_imm
	 * stor type segment src_reg addr_reg
	 * stor "immediate" type segment src_reg addr_imm
	 * set type dest_reg src_reg
	 * set "immediate" type dest_reg imm
	 * push type src_reg
	 * push "immediate" type imm
	 * pop type dest_reg
	 * memalloc ptr_dest_reg size_reg
	 * memalloc "immediate" ptr_dest_reg size_imm
	 * memfree ptr_reg
	 * memset ptr_reg size_reg val_reg
	 * memset "immediate" ptr_reg size_reg val_imm
	 * call funcid_reg
	 * call "immediate" funcid_imm
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
			arrpush(instarr, inst);
			break;
		case TOKEN_DUMPREG:
			inst.opcode = JIROP_DUMPREG;
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			ident = l->start;
			if(strstr(ident, "REG") == ident) {
				ident += STRLEN("REG");
				inst.ra = atoi(ident);
			} else if(strstr(ident, "FREG") == ident) {
				ident += STRLEN("FREG");
				inst.fa = atoi(ident);
			} else if(strstr(ident, "F64REG") == ident) {
				ident += STRLEN("F64REG");
				inst.f64a = atoi(ident);
			}
			arrpush(instarr, inst);
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
			if(l->token == TOKEN_IMMEDIATE) {
				inst.immediate = true;
				lex(l);
			}
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(l->token);
				lex(l);
			}

			assert(l->token == TOKEN_IDENTIFIER);
			ident = l->start;
			if(strstr(ident, "REG") == ident) {
				ident += STRLEN("REG");
				inst.ra = atoi(ident);
			} else if(strstr(ident, "FREG") == ident) {
				ident += STRLEN("FREG");
				inst.fa = atoi(ident);
			} else if(strstr(ident, "F64REG") == ident) {
				ident += STRLEN("F64REG");
				inst.f64a = atoi(ident);
			}

			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			ident = l->start;
			if(strstr(ident, "REG") == ident) {
				ident += STRLEN("REG");
				inst.rb = atoi(ident);
			} else if(strstr(ident, "FREG") == ident) {
				ident += STRLEN("FREG");
				inst.fb = atoi(ident);
			} else if(strstr(ident, "F64REG") == ident) {
				ident += STRLEN("F64REG");
				inst.f64b = atoi(ident);
			}
			
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
				ident = l->start;
				if(strstr(ident, "REG") == ident) {
					ident += STRLEN("REG");
					inst.rc = atoi(ident);
				} else if(strstr(ident, "FREG") == ident) {
					ident += STRLEN("FREG");
					inst.fc = atoi(ident);
				} else if(strstr(ident, "F64REG") == ident) {
					ident += STRLEN("F64REG");
					inst.f64c = atoi(ident);
				}
			}

			arrpush(instarr, inst);
			break;
		case TOKEN_NOT: case TOKEN_NEG: case TOKEN_FNEG:


			// TODO
			break;


			lex(l);
			inst.opcode = TOKEN_TO_OP(token);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64)
				inst.type = TOKEN_TO_TYPE(token);
			arrpush(instarr, inst);
			break;
		case TOKEN_BRANCH:
			inst.opcode = JIROP_BRANCH;
			lex(l);
			if(l->token == TOKEN_IMMEDIATE) {
				inst.immediate = true;
				lex(l);
				assert(l->token == TOKEN_IDENTIFIER);
				ident = l->start;
				if(strstr(ident, "REG") == ident) {
					ident += STRLEN("REG");
					inst.ra = atoi(ident);
				} else {
					assert("reg must be integer reg" && 0);
				}
				lex(l);
				assert(l->token == TOKEN_INTLIT);
				sscanf(l->start, "%li", &inst.immsi);
			} else {
				assert(l->token == TOKEN_IDENTIFIER);
				ident = l->start;
				if(strstr(ident, "REG") == ident) {
					ident += STRLEN("REG");
					inst.ra = atoi(ident);
				} else {
					assert("reg must be integer reg" && 0);
				}
				lex(l);
				assert(l->token == TOKEN_IDENTIFIER);
				ident = l->start;
				if(strstr(ident, "REG") == ident) {
					ident += STRLEN("REG");
					inst.rb = atoi(ident);
				} else {
					assert("reg must be integer reg" && 0);
				}
			}
			arrpush(instarr, inst);
			break;     
		case TOKEN_JMP:
			inst.opcode = JIROP_JMP;
			lex(l);
			if(l->token == TOKEN_IMMEDIATE) {
				inst.immediate = true;
				lex(l);
				assert(l->token == TOKEN_INTLIT);
				sscanf(l->start, "%li", &inst.immsi);
			} else {
				assert(l->token == TOKEN_IDENTIFIER);
				ident = l->start;
				if(strstr(ident, "REG") == ident) {
					ident += STRLEN("REG");
					inst.ra = atoi(ident);
				} else {
					assert("reg must be integer reg" && 0);
				}
			}
			arrpush(instarr, inst);
			break;        
		case TOKEN_LOAD:
			break;       
		case TOKEN_STOR:
			break;       
		case TOKEN_PUSH:
			break;
		case TOKEN_POP:
			break;
		case TOKEN_MEMALLOC:
			break;
		case TOKEN_MEMFREE:
			break;
		case TOKEN_MEMSET:
			break;
		case TOKEN_SET:
			inst.opcode = JIROP_SET;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			ident = l->start;
			if(strstr(ident, "REG") == ident) {
				ident += STRLEN("REG");
				inst.ra = atoi(ident);
			} else if(strstr(ident, "FREG") == ident) {
				ident += STRLEN("FREG");
				inst.fa = atoi(ident);
			} else if(strstr(ident, "F64REG") == ident) {
				ident += STRLEN("F64REG");
				inst.f64a = atoi(ident);
			}
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
			arrpush(instarr, inst);
			break;        
		case TOKEN_SETREG:
			inst.opcode = JIROP_SETREG;
			lex(l);
			if(l->token >= TOKEN_S64 && l->token <= TOKEN_F64) {
				inst.type = TOKEN_TO_TYPE(token);
				lex(l);
			}
			assert(l->token == TOKEN_IDENTIFIER);
			ident = l->start;
			if(strstr(ident, "REG") == ident) {
				ident += STRLEN("REG");
				inst.ra = atoi(ident);
			} else if(strstr(ident, "FREG") == ident) {
				ident += STRLEN("FREG");
				inst.fa = atoi(ident);
			} else if(strstr(ident, "F64REG") == ident) {
				ident += STRLEN("F64REG");
				inst.f64a = atoi(ident);
			}
			lex(l);
			assert(l->token == TOKEN_IDENTIFIER);
			ident = l->start;
			if(strstr(ident, "REG") == ident) {
				ident += STRLEN("REG");
				inst.ra = atoi(ident);
			} else if(strstr(ident, "FREG") == ident) {
				ident += STRLEN("FREG");
				inst.fa = atoi(ident);
			} else if(strstr(ident, "F64REG") == ident) {
				ident += STRLEN("F64REG");
				inst.f64a = atoi(ident);
			}
			arrpush(instarr, inst);
			break;        
		case TOKEN_INVALID:
			break;
		}
	}
}

void JIR_print(JIR inst) {
	printf("opcode: %s\ntype: %s\ntype2: %s\nra: %i\nrb: %i\nrc: %i\nfa: %i\nfb: %i\nfc: %i\nf64a: %i\nf64b: %i\nf64c: %i\nimmediate: %i\nimmui: %lu\nimmsi: %li\nimmf: %f\nimmf64: %lf\n",
	opcodesDebug[inst.opcode],
	typesDebug[inst.type],
	typesDebug[inst.type2],
	inst.ra, inst.rb, inst.rc,
	inst.fa, inst.fb, inst.fc,
	inst.f64a, inst.f64b, inst.f64c,
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

	bool run = true;
	bool dojump = false;
	int pc = 0;

	while(pc < arrlen(prog)) {
		JIR inst = prog[pc];
		int newpc = pc + 1;

		switch(inst.opcode) {
		default:
			printf("opcode %s unimplemeted\n", opcodesDebug[inst.opcode]);
			assert(0);
			break;
		case JIROP_SET:
			regs[inst.ra] = inst.immsi;
			break;
		case JIROP_DUMPREG:
			if(inst.type == JIRTYPE_F32)
				printf("freg %i: %f\n", inst.fa, fregs[inst.fa]);
			else if(inst.type == JIRTYPE_F64)
				printf("f64reg %i: %g\n", inst.f64a, f64regs[inst.fa]);
			else
				printf("reg %i: %li\n", inst.ra, regs[inst.ra]);
			break;
		case JIROP_ADD:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] + inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] + regs[inst.rc];
			}
			break;
		case JIROP_SUB:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] - inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] - regs[inst.rc];
			}
			break;
		case JIROP_MUL:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] * inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] * regs[inst.rc];
			}
			break;
		case JIROP_DIV:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] / inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] / regs[inst.rc];
			}
			break;
		case JIROP_MOD:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] % inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] % regs[inst.rc];
			}
			break;
		case JIROP_EQ:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] == inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] == regs[inst.rc];
			}
			break;
		case JIROP_NE:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] != inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] != regs[inst.rc];
			}
			break;
		case JIROP_LE:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] <= inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] <= regs[inst.rc];
			}
			break;
		case JIROP_GT:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] > inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] > regs[inst.rc];
			}
			break;
		case JIROP_LT:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] < inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] < regs[inst.rc];
			}
			break;
		case JIROP_GE:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] >= inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] >= regs[inst.rc];
			}
			break;
		case JIROP_AND:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] & inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] & regs[inst.rc];
			}
			break;
		case JIROP_OR:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] | inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] | regs[inst.rc];
			}
			break;
		case JIROP_XOR:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] ^ inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] ^ regs[inst.rc];
			}
			break;
		case JIROP_LSHIFT:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] << inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] << regs[inst.rc];
			}
			break;
		case JIROP_RSHIFT:
			if(inst.immediate) {
				regs[inst.ra] = regs[inst.rb] >> inst.immsi;
			} else {
				regs[inst.ra] = regs[inst.rb] >> regs[inst.rc];
			}
			break;

		case JIROP_JMP:
			if(inst.immediate) {
				newpc = pc + inst.immsi;
			} else {
				newpc = pc + (int64_t)regs[inst.ra];
			}
			break;
		case JIROP_BRANCH:
			dojump = (bool)regs[inst.ra];
			if(dojump) {
				if(inst.immediate) {
					newpc = pc + inst.immsi;
				} else {
					newpc = pc + (int64_t)regs[inst.rb];
				}
			}
			break;

		//TODO floating ops
		case JIROP_FADD:
			break;
		case JIROP_FSUB:
			break;
		case JIROP_FMUL:
			break;
		case JIROP_FDIV:
			break;
		case JIROP_FMOD:
			break;
		case JIROP_FEQ:
			break;
		case JIROP_FNE:
			break;
		case JIROP_FLE:
			break;
		case JIROP_FGT:
			break;
		case JIROP_FLT:
			break;
		case JIROP_FGE:
			break;

		case JIROP_HALT:
			run = false;
			break;
		}

		if(!run) break;

		pc = newpc;
	}
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
	arrsetcap(instarr, 10);
	lexer = (Lexer) {
		.keywords = keywords,
		.keywordsCount = ARRLEN(keywords),
		.keywordLengths = keywordLengths,
		.src = fm.buf,
		.cur = fm.buf,
		.srcEnd = fm.buf + fm.size,
		.debugInfo = (DebugInfo){ .line = 1, .col = 1 },
	};

	parse(&lexer, instarr);
	for(int i = 0; i < arrlen(instarr); ++i) {
		printf("INST %i\n", i);
		JIR_print(instarr[i]);
		printf("\n");
	}

	JIR_exec(instarr);

	return 0;
}
