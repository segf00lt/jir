#define JLIB_FMAP_IMPL

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

#define INSTRUCTRIONS \
	X(EQ)         \
	X(NE)         \
	X(LE)         \
	X(GT)         \
	X(EQ8)        \
	X(NE8)        \
	X(LE8)        \
	X(GT8)        \
	X(EQ16)       \
	X(NE16)       \
	X(LE16)       \
	X(GT16)       \
	X(EQ32)       \
	X(NE32)       \
	X(LE32)       \
	X(GT32)       \
	X(EQ64)       \
	X(NE64)       \
	X(LE64)       \
	X(GT64)       \
	X(EQU8)       \
	X(NEU8)       \
	X(LEU8)       \
	X(GTU8)       \
	X(EQU16)      \
	X(NEU16)      \
	X(LEU16)      \
	X(GTU16)      \
	X(EQU32)      \
	X(NEU32)      \
	X(LEU32)      \
	X(GTU32)      \
	X(FEQ)        \
	X(FNE)        \
	X(FLE)        \
	X(FGT)        \
	X(FEQ32)      \
	X(FNE32)      \
	X(FLE32)      \
	X(FGT32)      \
	X(FEQ64)      \
	X(FNE64)      \
	X(FLE64)      \
	X(FGT64)      \
	X(BRANCH)     \
	X(JMP)        \
	X(BRANCHIMM)  \
	X(JMPIMM)     \
	X(FLOAD)      \
	X(FLOAD32)    \
	X(FLOAD64)    \
	X(FSTOR)      \
	X(FSTOR32)    \
	X(FSTOR64)    \
	X(FSET)       \
	X(FSET32)     \
	X(FSET64)     \
	X(FSETREG)    \
	X(FSETREG32)  \
	X(FSETREG64)  \
	X(LOAD)       \
	X(LOAD8)      \
	X(LOADU8)     \
	X(LOAD16)     \
	X(LOADU16)    \
	X(LOAD32)     \
	X(LOADU32)    \
	X(LOAD64)     \
	X(STOR)       \
	X(STOR8)      \
	X(STOR16)     \
	X(STOR32)     \
	X(STOR64)     \
	X(STORU8)     \
	X(STORU16)    \
	X(STORU32)    \
	X(POP)        \
	X(POP8)       \
	X(POPU8)      \
	X(POP16)      \
	X(POPU16)     \
	X(POP32)      \
	X(POPU32)     \
	X(POP64)      \
	X(PUSH)       \
	X(PUSH8)      \
	X(PUSH16)     \
	X(PUSH32)     \
	X(PUSH64)     \
	X(PUSHU8)     \
	X(PUSHU16)    \
	X(PUSHU32)    \
	X(SET)        \
	X(SET8)       \
	X(SETU8)      \
	X(SET16)      \
	X(SETU16)     \
	X(SET32)      \
	X(SETU32)     \
	X(SET64)      \
	X(SETREG)     \
	X(SETREG8)    \
	X(SETREGU8)   \
	X(SETREG16)   \
	X(SETREGU16)  \
	X(SETREG32)   \
	X(SETREGU32)  \
	X(SETREG64)   \
	X(AND)        \
	X(AND8)       \
	X(AND16)      \
	X(AND32)      \
	X(AND64)      \
	X(OR)         \
	X(OR8)        \
	X(OR16)       \
	X(OR32)       \
	X(OR64)       \
	X(NOT)        \
	X(NOT8)       \
	X(NOT16)      \
	X(NOT32)      \
	X(NOT64)      \
	X(XOR)        \
	X(XOR8)       \
	X(XOR16)      \
	X(XOR32)      \
	X(XOR64)      \
	X(LSHIFT)     \
	X(LSHIFT8)    \
	X(LSHIFT16)   \
	X(LSHIFT32)   \
	X(LSHIFT64)   \
	X(RSHIFT)     \
	X(RSHIFT8)    \
	X(RSHIFT16)   \
	X(RSHIFT32)   \
	X(RSHIFT64)   \
	X(RSHIFTU8)   \
	X(RSHIFTU16)  \
	X(RSHIFTU32)  \
	X(ADD)        \
	X(ADD8)       \
	X(ADD16)      \
	X(ADD32)      \
	X(ADD64)      \
	X(FADD)       \
	X(FADD32)     \
	X(FADD64)     \
	X(SUB)        \
	X(SUB8)       \
	X(SUB16)      \
	X(SUB32)      \
	X(SUB64)      \
	X(FSUB)       \
	X(FSUB32)     \
	X(FSUB64)     \
	X(MUL)        \
	X(MUL8)       \
	X(MUL16)      \
	X(MUL32)      \
	X(MUL64)      \
	X(FMUL)       \
	X(FMUL32)     \
	X(FMUL64)     \
	X(DIV)        \
	X(DIV8)       \
	X(DIV16)      \
	X(DIV32)      \
	X(DIV64)      \
	X(FDIV)       \
	X(FDIV32)     \
	X(FDIV64)     \
	X(CALL)       \
	X(RET)        \
	X(INT)        \
	X(HALT)

enum JIROP {
#define X(val) val,
	INSTRUCTRIONS
#undef X
};
typedef enum JIROP JIROP;

struct JIR {
	JIROP opcode;
	int r1, r2, r3;
	uint64_t imm;
};
typedef struct JIR JIR;

#include "lex.c"



int main(int argc, char **argv) {
	Fmap fm;
	fmapopen("lexer_test", O_RDONLY, &fm);
	Lexer lexer = (Lexer) {
		.keywords = keywords,
		.keywordsCount = ARRLEN(keywords),
		.keywordLengths = keywordLengths,
		.src = fm.buf,
		.cur = fm.buf,
		.srcEnd = fm.buf + fm.size,
	};
	for(char *s = fm.buf; *s; ++s)
		*s = toupper(*s);
	while(!lexer.eof) {
		lex(&lexer);
		printf("%s\n", tokenDebug[lexer.token-TOKEN_INVALID]);
	}
	fmapclose(&fm);
	return 0;
}
