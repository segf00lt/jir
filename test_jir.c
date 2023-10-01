#include <stdio.h>
#include <string.h>
#include "basic.h"
#define JIR_IMPL
#include "jir.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

typedef struct {
	bool status;
	const char *name;
} Test_result;
typedef Test_result (*Test)(void);

#define TEST_RESULT(s) (Test_result){ .status = (s), .name = __func__ }

void test_run(Test t) {
	Test_result res = t();
	printf("%s: %s\n", res.name, res.status ? "PASSED" : "FAILED");
}

Test_result test_casts() {
	bool status = false;
	JIR instarr[] = {
		MOVEOPIMM(F32, REG(2), IMMF32(112.4367)),
		CASTOP(BITCAST,U64,F32,REG(1),REG(2)),
		MOVEOPIMM(U64,REG(0),IMMU64(0xffffffffff000000)),
		CASTOP(BITCAST,F32,U64,REG(1),REG(0)),
		MOVEOPIMM(F64,REG(0),IMMF64(0.55512316)),
		CASTOP(BITCAST,F32,F64,REG(3),REG(0)),
		CASTOP(BITCAST,U64,F64,REG(2),REG(0)),
		MOVEOPIMM(F32,REG(4),IMMF32(200.613)),
		CASTOP(TYPECAST,U32,F32,REG(4),REG(4)),
		MOVEOPIMM(F64,REG(5),IMMF64(1992931.234)),
		CASTOP(TYPECAST,F32,F64,REG(5),REG(5)),
		HALTOP,
	};

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);

	u64 *reg = image.reg;
	float *reg_f32 = image.reg_f32;
	double *reg_f64 = image.reg_f64;

	bool test_reg2_f32_bitcast_to_reg1_u64  = (reg_f32[2] == *(float*)(reg+1));
	bool test_reg0_u64_bitcast_to_reg1_f32  = (reg_f32[1] == *(float*)(reg+0));
	bool test_reg0_f64_bitcast_to_reg3_f32  = (reg_f32[3] == *(float*)(reg_f64+0));
	bool test_reg0_f64_bitcast_to_reg2_u64  = (reg[2] == *(u64*)(reg_f64+0));
	bool test_reg4_f32_typecast_to_reg4_u32 = ((reg[4]&0xffffffff) == (u32)reg_f32[4]);
	bool test_reg5_f64_typecast_to_reg5_f32 = (reg_f32[5] == (float)reg_f64[5]);

	status =
		test_reg2_f32_bitcast_to_reg1_u64  &&
		test_reg0_u64_bitcast_to_reg1_f32  &&
		test_reg0_f64_bitcast_to_reg3_f32  &&
		test_reg0_f64_bitcast_to_reg2_u64  &&
		test_reg4_f32_typecast_to_reg4_u32 &&
		test_reg5_f64_typecast_to_reg5_f32;

	JIRIMAGE_destroy(&image);

	return TEST_RESULT(status);
}

Test_result test_binary_and_unaryops() {
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

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);

	u64 *reg = image.reg;
	float *reg_f32 = image.reg_f32;

	bool test_neg_s8_not_u16    = (reg[1] == (~(-reg[0]&0xff)&0xffff));
	bool test_fneg_f32_fsub_f32 = (reg_f32[2] == (-reg_f32[1] - reg_f32[1]));
	bool test_fmul_f32_fadd_f32 = (reg_f32[3] == (reg_f32[1]*2.0 + reg_f32[2]));

	bool status = test_fmul_f32_fadd_f32 && test_fneg_f32_fsub_f32 && test_neg_s8_not_u16;

	JIRIMAGE_destroy(&image);

	return TEST_RESULT(status);
}

Test_result test_io() {
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

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	strcpy((char*)image.global, "Hello, World!\n");
	strcpy((char*)image.global+128, "iotest");
	if(JIR_preprocess(&image))
		JIR_exec(&image);
	JIRIMAGE_destroy(&image);
	FILE *fp = fopen("iotest", "r");
	fseek(fp, 0L, SEEK_END);
	size_t sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	char *message = malloc(sz+1);
	message[sz] = 0;
	fread(message, 1, sz, fp);
	assert(!remove("iotest"));
	bool status = !strcmp(message, "Hello, World!\n");
	free(message);
	fclose(fp);
	return TEST_RESULT(status);
}

Test_result test_float_ops() {
	JIR instarr[] = {
		MOVEOPIMM(F32, REG(1), IMMF32(1.333)),
		BINOPIMM(FMUL, F32, REG(2), REG(1), IMMF32(7.1)),
		//DUMPREGOP(F32, REG(1)),
		//DUMPREGOP(F32, REG(2)),
		MOVEOPIMM(F64, REG(1), IMMF64(1.333)),
		BINOPIMM(FDIV, F64, REG(2), REG(1), IMMF64(7.1)),
		//DUMPREGOP(F64, REG(1)),
		//DUMPREGOP(F64, REG(2)),
		HALTOP,
	};

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);

	float *reg_f32 = image.reg_f32;
	double *reg_f64 = image.reg_f64;
	float f = 7.1; // NOTE for some reason this prevents a bit of float inaccuracy
	bool test_moveimm_f32_fmul_f32 = ((reg_f32[1] == (float)1.333000) && (reg_f32[2] == reg_f32[1]*f));
	bool test_moveimm_f64_fmul_f64 = ((reg_f64[1] == (double)1.333000) && (reg_f64[2] == reg_f64[1]/7.1));
	bool status = test_moveimm_f64_fmul_f64 && test_moveimm_f32_fmul_f32;

	JIRIMAGE_destroy(&image);
	return TEST_RESULT(status);
}

Test_result test_integer_comparisons() {
	JIR instarr[] = {
		MOVEOPIMM(S8, REG(1), IMMU64(-12)),
		MOVEOPIMM(S8, REG(2), IMMU64(5)),
		BINOP(LE, S8, REG(3), REG(1), REG(2)),
		//DUMPREGOP(U64, REG(3)),
		HALTOP,
	};

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);
	u64 *reg = image.reg;
	bool status = (reg[3] == 0x1);
	JIRIMAGE_destroy(&image);
	return TEST_RESULT(status);
}

// TODO
// add a way to set global data
Test_result test_procedure_calls() {
	JIR instarr[] = {
		DEFPROCLABEL(LABEL("proc0")),
		MOVEOPIMM(S64, REG(1), IMMU64(22)),
		SETARGOP(S64,PORT(0),REG(1)),
		//DEBUGMSGOP("dumping port in proc0"),
		//DUMPPORTOP(S64,PORT(1)),
		CALLOP(LABEL("proc1")),
		GETRETOP(U64,REG(1),PORT(0)),
		SETRETOP(U64,PORT(0),REG(1)),
		RETOP,
		DEFPROCLABEL(LABEL("proc1")),
		GETARGOP(S64,REG(1),PORT(0)),
		BINOPIMM(MOD,U64,REG(2),REG(1),IMMU64(11)),
		//DUMPREGOP(U64,REG(2)),
		LOCALOP(S64,REG(3)),
		STOROP(PTR_LOCAL,S64,REG(3),REG(2)),
		BRANCHOP(REG(2), LABEL("is_multiple_of_2")),
		MOVEOPIMM(U64, REG(1), IMMU64(12)),
		SETARGOP(U64,PORT(0),REG(1)),
		DEFLABEL(LABEL("is_multiple_of_2")),
		//DUMPREGOP(U64,REG(1)),
		//DUMPPORTOP(U64,PORT(0)),
		CALLOP(LABEL("proc2")),
		GETRETOP(U64,REG(4),PORT(0)),
		LOADOP(S64,PTR_LOCAL,REG(2),REG(3)),
		//DUMPREGOP(U64,REG(2)),
		SETRETOP(U64,PORT(0),REG(4)),
		DEFLABEL(LABEL("proc1_return")),
		DEFLABEL(LABEL("unreachable_instruction")),
		RETOP,
		DEFPROCLABEL(LABEL("proc2")),
		GETARGOP(U64,REG(1),PORT(0)),
		//DUMPPORTOP(U64,PORT(1)),
		BINOPIMM(MUL,U64,REG(2),REG(1),IMMU64(10)),
		//DUMPREGOP(U64,REG(2)),
		BINOPIMM(ADD,U64,REG(2),REG(2),IMMU64(12)),
		//DUMPREGOP(U64,REG(2)),
		BINOPIMM(ADD,U8,REG(2),REG(2),IMMU64(126)),
		//DUMPREGOP(U64,REG(2)),
		SETRETOP(U64,PORT(0),REG(2)),
		RETOP,
		HALTOP,
	};

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);
	bool status = (image.port[0] == 0x2);
	JIRIMAGE_destroy(&image);
	return TEST_RESULT(status);
}

Test_result test_heap() {
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

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);
	bool status = (*(u16*)(image.heap+0xfed) == (u16)0xc);
	JIRIMAGE_destroy(&image);
	return TEST_RESULT(status);
}

Test_result test_globals() {
	JIR instarr[] = {
		DEFGLOBAL(LABEL("test_global_var_1"), F32, IMMF32(42.1309)),
		DEFGLOBAL(LABEL("test_global_var_2"), F32, IMMF32(0.0)),
		LOADOP(F32, PTR_GLOBAL, REG(0), LABEL("test_global_var_1")),
		BINOPIMM(FDIV, F32, REG(1), REG(0), IMMF32(2.0)),
		STOROP(PTR_GLOBAL, F32, LABEL("test_global_var_2"), REG(1)),
		HALTOP,
	};
	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);
	float test_global_var_1 = 42.1309;
	float test_global_var_2 = 42.1309 / 2.0;
	bool status = (image.reg_f32[0] == test_global_var_1 && ((float*)image.global)[1] == test_global_var_2);
	return TEST_RESULT(status);
}

int main(int argc, char **argv) {
	Test tests[] = {
		test_casts,
		test_binary_and_unaryops,
		test_io,
		test_float_ops,
		test_integer_comparisons,
		test_procedure_calls,
		test_heap,
	};

	STATICARRFOR(tests)
		test_run(tests[tests_index]);

	return 0;
}
