#define JIR_IMPL

#include <stdio.h>
#include "basic.h"
#include "jir.h"

char *TEST_STATUS[] = { "FAILED","PASSED" };

typedef struct {
	char *name;
	bool status;
} Test;

int main(int argc, char **argv) {
	printf("\n###### testing casts ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(F32, REG(2), IMMF32(112.4367)),
			CASTOP(BITCAST,U64,F32,REG(1),REG(2)),
			//DUMPREGOP(F32, REG(2)),
			//DUMPREGOP(U64, REG(1)),
			MOVEOPIMM(U64,REG(0),IMMU64(0xffffffffff000000)),
			CASTOP(BITCAST,F32,U64,REG(1),REG(0)),
			//DUMPREGOP(U64, REG(0)),
			//DUMPREGOP(F32, REG(1)),
			MOVEOPIMM(F64,REG(0),IMMF64(0.55512316)),
			CASTOP(BITCAST,F32,F64,REG(3),REG(0)),
			//DUMPREGOP(F64, REG(0)),
			//DUMPREGOP(F32, REG(3)),
			CASTOP(BITCAST,U64,F64,REG(2),REG(0)),
			//DUMPREGOP(U64, REG(2)),
			MOVEOPIMM(F32,REG(4),IMMF32(200.613)),
			CASTOP(TYPECAST,U32,F32,REG(4),REG(4)),
			MOVEOPIMM(F64,REG(5),IMMF64(1992931.234)),
			CASTOP(TYPECAST,F32,F64,REG(5),REG(5)),
			//DUMPREGOP(F64,REG(5)),
			//DUMPREGOP(F32,REG(5)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
			JIR_exec(&image);

		u64 *reg = image.reg;
		float *reg_f32 = image.reg_f32;
		double *reg_f64 = image.reg_f64;

		Test cast_tests[] = {
			(Test){ .name = "test_reg2_f32_bitcast_to_reg1_u64", .status = (reg_f32[2] == *(float*)(reg+1)), },
			(Test){ .name = "test_reg0_u64_bitcast_to_reg1_f32", .status = (reg_f32[1] == *(float*)(reg+0)), },
			(Test){ .name = "test_reg0_f64_bitcast_to_reg3_f32", .status = (reg_f32[3] == *(float*)(reg_f64+0)), },
			(Test){ .name = "test_reg0_f64_bitcast_to_reg2_u64", .status = (reg[2] == *(u64*)(reg_f64+0)), },
			(Test){ .name = "test_reg4_f32_typecast_to_reg4_u32", .status = ((reg[4]&0xffffffff) == (u32)reg_f32[4]), },
			(Test){ .name = "test_reg5_f64_typecast_to_reg5_f32", .status = (reg_f32[5] == (float)reg_f64[5]), },
		};
		for(int i = 0; i < STATICARRLEN(cast_tests); ++i)
			printf("%s: %s\n", cast_tests[i].name, TEST_STATUS[cast_tests[i].status]);
		
		JIRIMAGE_destroy(&image);
	}

	printf("\n###### testing unaryops ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(S8, REG(0), IMMU64(1)),
			UNOP(NEG,S8,REG(1),REG(0)),
			UNOP(NOT,U16,REG(1),REG(1)),
			//DUMPREGOP(S8, REG(1)),
			//DUMPREGOP(U16, REG(1)),
			MOVEOPIMM(F32,REG(1),IMMF32(35.412)),
			UNOP(FNEG,F32,REG(2),REG(1)),
			BINOP(FSUB,F32,REG(2),REG(2),REG(1)),
			//DUMPREGOP(F32, REG(2)),
			BINOPIMM(FMUL,F32,REG(3),REG(1),IMMF32(2.0)),
			BINOP(FADD,F32,REG(3),REG(3),REG(2)),
			//DUMPREGOP(F32, REG(3)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
			JIR_exec(&image);

		u64 *reg = image.reg;
		float *reg_f32 = image.reg_f32;

		Test cast_tests[] = {
			(Test){ .name = "test_neg_s8_not_u16", .status = (reg[1] == (~(-reg[0]&0xff)&0xffff)), },
			(Test){ .name = "test_fneg_f32_fsub_f32", .status = (reg_f32[2] == (-reg_f32[1] - reg_f32[1])), },
			(Test){ .name = "test_fmul_f32_fadd_f32", .status = (reg_f32[3] == (reg_f32[1]*2.0 + reg_f32[2])), },
		};
		for(int i = 0; i < STATICARRLEN(cast_tests); ++i)
			printf("%s: %s\n", cast_tests[i].name, TEST_STATUS[cast_tests[i].status]);

		JIRIMAGE_destroy(&image);
	}
	
	printf("\n###### testing io ######\n\n");
	{
		JIR instarr[] = {
			SETARGOPIMM(PTR_GLOBAL, PORT(0), PTR(128)),
			SETARGOPIMM(S32, PORT(1), IMMS32(O_WRONLY | O_CREAT)),
			SETARGOPIMM(U64, PORT(2), IMMU64(00666)),
			SYSCALLOP(IMMU64(2)),
			GETRETOP(S32, REG(0), PORT(0)),
			SETARGOP(S32, PORT(0), REG(0)),
			SETARGOPIMM(PTR_GLOBAL, PORT(1), PTR(0)),
			SETARGOPIMM(U64, PORT(2), IMMU64(14)),
			SYSCALLOP(IMMU64(1)),
			SETARGOP(S32, PORT(0), REG(0)),
			SYSCALLOP(IMMU64(3)),
			//DUMPMEMOP(PTR_GLOBAL, true, true, IMMU64(0), IMMU64(14)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		strcpy((char*)image.global, "Hello, World!\n");
		strcpy((char*)image.global+128, "iotest");
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
			JIR_exec(&image);
		JIRIMAGE_destroy(&image);
		FILE *fp = fopen("iotest", "r");
		fseek(fp, 0L, SEEK_END);
		size_t sz = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		char *message = malloc(sz);
		fread(message, 1, sz, fp);
		printf("test_io_syscalls: %s\n", TEST_STATUS[!strcmp(message, "Hello, World!\n")]);
		assert(!remove("iotest"));
	}

	printf("\n###### testing float ops ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(F32, REG(1), IMMF32(1.333)),
			BINOPIMM(FMUL, F32, REG(2), REG(1), IMMF32(7.1)),
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

		float *reg_f32 = image.reg_f32;
		double *reg_f64 = image.reg_f64;
		float f = 7.1; // NOTE for some reason this prevents a bit of float inaccuracy
		Test cast_tests[] = {
			(Test){ .name = "test_moveimm_f32_fmul_f32", .status = ((reg_f32[1] == (float)1.333000) && (reg_f32[2] == reg_f32[1]*f)), },
			(Test){ .name = "test_moveimm_f64_fmul_f64", .status = ((reg_f64[1] == (double)1.333000) && (reg_f64[2] == reg_f64[1]/7.1)), },
		};
		for(int i = 0; i < STATICARRLEN(cast_tests); ++i)
			printf("%s: %s\n", cast_tests[i].name, TEST_STATUS[cast_tests[i].status]);

		JIRIMAGE_destroy(&image);
	}

	printf("\n###### testing integer comparisons ######\n\n");
	{
		JIR instarr[] = {
			MOVEOPIMM(S8, REG(1), IMMU64(-12)),
			MOVEOPIMM(S8, REG(2), IMMU64(5)),
			BINOP(LE, S8, REG(3), REG(1), REG(2)),
			//DUMPREGOP(U64, REG(3)),
			HALTOP,
		};
		JIR *proctab[8] = { instarr };
		JIRIMAGE image;
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
			JIR_exec(&image);
		u64 *reg = image.reg;
		printf("test_reg1_reg2_le_s8: %s\n", TEST_STATUS[reg[3] == 0x1]);
		JIRIMAGE_destroy(&image);
	}

	printf("\n###### testing procedure calls ######\n\n");
	{
		// TODO write a test that actually involves getting return values from a procedure
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
			DEFLABEL(LABEL("is_multiple_of_2")),
			DUMPREGOP(U64,REG(1)),
			DUMPPORTOP(U64,PORT(1)),
			CALLOP(IMMU64(2)),
			LOADOP(S64,PTR_LOCAL,REG(2),REG(3)),
			DUMPREGOP(U64,REG(2)),
			EFRAMEOP,
			DEFLABEL(LABEL("proc1_return")),
			DEFLABEL(LABEL("unreachable_instruction")),
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
			MOVEOPIMM(U16, REG(1), IMMU64(0xc)),
			MOVEOPIMM(PTR_HEAP, REG(2), PTR(0xfed)),
			STOROP(PTR_HEAP, U16, REG(2), REG(1)),
			LOADOP(U16, PTR_HEAP, REG(3), REG(2)),
			//DUMPREGOP(U16, REG(3)),
			//DUMPMEMOP(PTR_HEAP, false, true, REG(2), IMMU64(0xfed+18)),
			GROWHEAPOP(IMMU64(2)),
			//DUMPMEMOP(PTR_HEAP, false, true, REG(2), IMMU64(0xfed+100)),
			HALTOP,
		};
		JIR *proctab[8] = {instarr};
		JIRIMAGE image;
		u64 nprocs = 1;
		JIRIMAGE_init(&image, proctab, nprocs);
		JIR_maplabels(proctab, nprocs);
		if(JIR_preprocess(&image))
			JIR_exec(&image);
		printf("test_growheap: %s\n", TEST_STATUS[*(u16*)(image.heap+0xfed) == (u16)0xc]);
		JIRIMAGE_destroy(&image);
	}

	return 0;
}
