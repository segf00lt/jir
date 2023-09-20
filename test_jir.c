#define JIR_IMPL

#include <stdio.h>
#include "basic.h"
#include "jir.h"

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
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
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
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
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
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		strcpy((char*)image.global, "Hello, World!\n");
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
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
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
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
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
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
			BRANCHOP(REG(2), LABEL("is_multiple_of_2")),
			MOVEOPIMM(U64, REG(1), IMMU64(12)),
			SETARGOP(U64,PORT(1),REG(1)),
			LABELOP(LABEL("is_multiple_of_2")),
			DUMPREGOP(U64,REG(1)),
			DUMPPORTOP(U64,PORT(1)),
			CALLOP(IMMU64(2)),
			LOADOP(S64,PTR_LOCAL,REG(2),REG(3)),
			DUMPREGOP(U64,REG(2)),
			EFRAMEOP,
			LABELOP(LABEL("proc1_return")),
			LABELOP(LABEL("unreachable_instruction")),
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
		u64 nprocs = 3;
		JIRIMAGE_init(&image, proctab, nprocs);
		if(JIR_preprocess(&image))
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
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
			JIR_exec(&image);
		JIRIMAGE_destroy(&image);
	}

	return 0;
}
