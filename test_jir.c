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
		DEFPROCLABEL(LABEL("main")),
		MOVEOPIMM(F32, FREG(2), IMMF32(112.4367)),
		CASTOP(BITCAST,U64,F32,REG(1),FREG(2)),
		MOVEOPIMM(U64,REG(0),IMMU64(0xffffffffff000000)),
		CASTOP(BITCAST,F32,U64,FREG(1),REG(0)),
		MOVEOPIMM(F64,FREG(0),IMMF64(0.55512316)),
		CASTOP(BITCAST,F32,F64,FREG(3),FREG(0)),
		CASTOP(BITCAST,U64,F64,REG(2),FREG(0)),
		MOVEOPIMM(F32,FREG(4),IMMF32(200.613)),
		CASTOP(TYPECAST,U32,F32,REG(4),FREG(4)),
		MOVEOPIMM(F64,FREG(5),IMMF64(1992931.234)),
		CASTOP(TYPECAST,F32,F64,FREG(5),FREG(5)),
		RETOP,
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
		DEFPROCLABEL(LABEL("main")),
		MOVEOPIMM(S8, REG(0), IMMU64(1)),
		UNOP(NEG,S8,REG(1),REG(0)),
		UNOP(NOT,U16,REG(1),REG(1)),
		MOVEOPIMM(F32,FREG(1),IMMF32(35.412)),
		UNOP(FNEG,F32,FREG(2),FREG(1)),
		BINOP(FSUB,F32,FREG(2),FREG(2),FREG(1)),
		BINOPIMM(FMUL,F32,FREG(3),FREG(1),IMMF32(2.0)),
		BINOP(FADD,F32,FREG(3),FREG(3),FREG(2)),
		SETRETOP(U64,PORT(0),REG(0)),
		SETRETOP(U64,PORT(1),REG(1)),
		SETRETOP(F32,PORT(2),FREG(1)),
		SETRETOP(F32,PORT(3),FREG(2)),
		SETRETOP(F32,PORT(4),FREG(3)),
		RETOP,
	};

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);

	bool status = false;
	{
		u64 *reg = image.reg;
		float *reg_f32 = image.reg_f32;

		bool test_neg_s8_not_u16    = (reg[1] == (~(-reg[0]&0xff)&0xffff));
		bool test_fneg_f32_fsub_f32 = (reg_f32[2] == (-reg_f32[1] - reg_f32[1]));
		bool test_fmul_f32_fadd_f32 = (reg_f32[3] == (reg_f32[1]*2.0 + reg_f32[2]));

		status = test_fmul_f32_fadd_f32 && test_fneg_f32_fsub_f32 && test_neg_s8_not_u16;
	}

	JIR_optimize_regs(instarr, STATICARRLEN(instarr));
	JIR_exec(&image);

	bool status2 = false;
	{
		u64 *reg = image.reg;
		float *reg_f32 = image.reg_f32;

		bool test_neg_s8_not_u16    = (reg[1] == (~(-reg[0]&0xff)&0xffff));
		bool test_fneg_f32_fsub_f32 = (reg_f32[2] == (-reg_f32[1] - reg_f32[1]));
		bool test_fmul_f32_fadd_f32 = (reg_f32[3] == (reg_f32[1]*2.0 + reg_f32[2]));

		status2 = test_fmul_f32_fadd_f32 && test_fneg_f32_fsub_f32 && test_neg_s8_not_u16;
	}

	JIRIMAGE_destroy(&image);

	return TEST_RESULT(status && status2);
}

Test_result test_io() {
	char *message = "Hello, World!\n";
	u64 message_len = strlen(message) + 1;
	char *outfile = "iotest";
	u64 outfile_len = strlen(outfile) + 1;
	JIR instarr[] = {
		DEFPROCLABEL(LABEL("main")),
		DEFGLOBALBYTES(LABEL("message"), IMMU64(message_len), EXTERNALDATA(message)),
		DEFGLOBALBYTES(LABEL("outfile"), IMMU64(outfile_len), EXTERNALDATA(outfile)),
		SETARGOPIMM(PTR_GLOBAL, PORT(0), LABEL("outfile")),
		SETARGOPIMM(S32, PORT(1), IMMS32(O_WRONLY | O_CREAT)),
		SETARGOPIMM(U64, PORT(2), IMMU64(00666)),
		MOVEOPIMM(U64, REG(0), IMMU64(2)),
		SYSCALLOPIND(REG(0)),
		GETRETOP(S32, REG(0), PORT(0)),
		SETARGOP(S32, PORT(0), REG(0)),
		SETARGOPIMM(PTR_GLOBAL, PORT(1), LABEL("message")),
		SETARGOPIMM(U64, PORT(2), IMMU64(14)),
		SYSCALLOP(IMMU64(1)),
		SETARGOP(S32, PORT(0), REG(0)),
		SYSCALLOP(IMMU64(3)),
		//DUMPMEMOP(PTR_GLOBAL, true, true, IMMU64(0), IMMU64(14)),
		RETOP,
	};

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);
	JIRIMAGE_destroy(&image);
	FILE *fp = fopen("iotest", "r");
	fseek(fp, 0L, SEEK_END);
	size_t sz = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	char *text = malloc(sz+1);
	text[sz] = 0;
	fread(text, 1, sz, fp);
	assert(!remove("iotest"));
	bool status = !strcmp(text, "Hello, World!\n");
	free(text);
	fclose(fp);
	return TEST_RESULT(status);
}

Test_result test_float_ops() {
	JIR instarr[] = {
		DEFPROCLABEL(LABEL("main")),
		MOVEOPIMM(F32, FREG(1), IMMF32(1.333)),
		BINOPIMM(FMUL, F32, FREG(2), FREG(1), IMMF32(7.1)),
		MOVEOPIMM(F64, FREG(1), IMMF64(1.333)),
		BINOPIMM(FDIV, F64, FREG(2), FREG(1), IMMF64(7.1)),
		RETOP,
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
		DEFPROCLABEL(LABEL("main")),
		MOVEOPIMM(S8, REG(1), IMMU64(-12)),
		MOVEOPIMM(S8, REG(2), IMMU64(5)),
		BINOP(LE, S8, REG(3), REG(1), REG(2)),
		//DUMPREGOP(U64, REG(3)),
		RETOP,
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

Test_result test_procedure_calls() {
	JIR instarr[] = {
		DEFPROCLABEL(LABEL("main")),
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
		ALLOCOP(S64,PTR(3)),
		ALLOCOP(F32,PTR(8)),
		STOROPIND(PTR_LOCAL,S64,PTR(3),REG(2)),
		BRANCHOP(REG(2), LABEL("is_multiple_of_2")),
		MOVEOPIMM(U64, REG(1), IMMU64(12)),
		SETARGOP(U64,PORT(0),REG(1)),
		DEFJMPLABEL(LABEL("is_multiple_of_2")),
		//DUMPREGOP(U64,REG(1)),
		//DUMPPORTOP(U64,PORT(0)),
		CALLOP(LABEL("proc2")),
		GETRETOP(U64,REG(4),PORT(0)),
		LOADOPIND(S64,PTR_LOCAL,REG(2),PTR(3)),
		//DUMPREGOP(U64,REG(2)),
		SETRETOP(U64,PORT(0),REG(4)),
		DEFJMPLABEL(LABEL("proc1_return")),
		DEFJMPLABEL(LABEL("unreachable_instruction")),
		RETOP,
		DEFPROCLABEL(LABEL("proc2")),
		ALLOCOP(S8,PTR(9)),
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
	};

	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);
	bool status = (image.port[0] == 0x2);
	FILE *outfile = fopen("out.asm", "w");
	JIR_translate_nasm(outfile, &image);
	fclose(outfile);
	JIRIMAGE_destroy(&image);
	return TEST_RESULT(status);
}

Test_result test_heap() {
	JIR instarr[] = {
		DEFPROCLABEL(LABEL("main")),
		MOVEOPIMM(U16, REG(1), IMMU64(0xc)),
		MOVEOPIMM(PTR_HEAP, PTR(2), OFFSET(0xfed)),
		STOROPIND(PTR_HEAP, U16, PTR(2), REG(1)),
		LOADOPIND(U16, PTR_HEAP, REG(3), PTR(2)),
		//DUMPREGOP(U16, REG(3)),
		//DUMPMEMOP(PTR_HEAP, false, true, REG(2), IMMU64(0xfed+18)),
		GROWHEAPOP(IMMU64(2)),
		//DUMPMEMOP(PTR_HEAP, false, true, REG(2), IMMU64(0xfed+100)),
		RETOP,
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
		DEFPROCLABEL(LABEL("main")),
		DEFGLOBAL(LABEL("test_global_var_2"), F32, IMMF32(0.0)),
		DEFGLOBAL(LABEL("test_global_var_1"), F32, IMMF32(42.1309)),
		DEFGLOBAL(LABEL("biginteger"), U64, IMMU64(0x34234)),
		MOVEOPIMM(PTR_GLOBAL, PTR(2), LABEL("test_global_var_1")),
		LOADOPIND(F32, PTR_GLOBAL, FREG(0), PTR(2)),
		BINOPIMM(FDIV, F32, FREG(1), FREG(0), IMMF32(2.0)),
		STOROP(PTR_GLOBAL, F32, LABEL("test_global_var_2"), FREG(1)),
		RETOP,
	};
	JIRIMAGE image;
	JIRIMAGE_init(&image, instarr, STATICARRLEN(instarr));
	if(JIR_preprocess(&image))
		JIR_exec(&image);
	float test_global_var_1 = 42.1309;
	float test_global_var_2 = 42.1309 / 2.0;
	bool status = (image.reg_f32[0] == test_global_var_1 && *(float*)(image.global) == test_global_var_2);
	JIRIMAGE_destroy(&image);
	return TEST_RESULT(status);
}

Test_result test_register_allocation() {
	JIR prog[] = {
		DEFPROCLABEL(LABEL("main")),

		MOVEOPIMM(S64, REG(1), IMMU64(3)),
		MOVEOPIMM(S64, REG(2), IMMU64(5)),
		MOVEOPIMM(S64, REG(5), IMMU64(7)),
		BINOP(ADD,S64,REG(0),REG(1),REG(2)),
		UNOP(NEG,S64,REG(3),REG(0)),
		BINOP(ADD,S64,REG(4),REG(3),REG(5)),
		BINOPIMM(MUL,S64,REG(5),REG(4),IMMU64(2)),
		BINOP(ADD,S64,REG(1),REG(3),REG(4)),
		BINOPIMM(SUB,S64,REG(4),REG(4),IMMU64(1)),
		BINOP(ADD,S64,REG(1),REG(5),REG(2)),
		SETRETOP(S64,PORT(0),REG(1)),

		MOVEOPIMM(S64, REG(7), IMMU64(3)),
		MOVEOPIMM(S64, REG(8), IMMU64(5)),
		MOVEOPIMM(S64, REG(11), IMMU64(7)),
		BINOP(ADD,S64,REG(6),REG(7),REG(8)),
		UNOP(NEG,S64,REG(9),REG(6)),
		BINOP(ADD,S64,REG(10),REG(9),REG(11)),
		BINOPIMM(MUL,S64,REG(11),REG(10),IMMU64(2)),
		BINOP(ADD,S64,REG(7),REG(9),REG(10)),
		BINOPIMM(SUB,S64,REG(10),REG(10),IMMU64(1)),
		BINOP(ADD,S64,REG(7),REG(11),REG(8)),
		SETRETOP(S64,PORT(1),REG(7)),

		MOVEOPIMM(S64, REG(13), IMMU64(3)),
		MOVEOPIMM(S64, REG(14), IMMU64(5)),
		MOVEOPIMM(S64, REG(17), IMMU64(7)),
		BINOP(ADD,S64,REG(12),REG(13),REG(14)),
		UNOP(NEG,S64,REG(15),REG(12)),
		BINOP(ADD,S64,REG(16),REG(15),REG(17)),
		BINOPIMM(MUL,S64,REG(17),REG(16),IMMU64(2)),
		BINOP(ADD,S64,REG(13),REG(15),REG(16)),
		BINOPIMM(SUB,S64,REG(16),REG(16),IMMU64(1)),
		BINOP(ADD,S64,REG(13),REG(17),REG(14)),
		SETRETOP(S64,PORT(2),REG(13)),

		MOVEOPIMM(S64, REG(19), IMMU64(3)),
		MOVEOPIMM(S64, REG(20), IMMU64(5)),
		MOVEOPIMM(S64, REG(23), IMMU64(7)),
		BINOP(ADD,S64,REG(18),REG(19),REG(20)),
		UNOP(NEG,S64,REG(21),REG(18)),
		BINOP(ADD,S64,REG(22),REG(21),REG(23)),
		BINOPIMM(MUL,S64,REG(23),REG(22),IMMU64(2)),
		BINOP(ADD,S64,REG(19),REG(21),REG(22)),
		BINOPIMM(SUB,S64,REG(22),REG(22),IMMU64(1)),
		BINOP(ADD,S64,REG(19),REG(23),REG(20)),
		SETRETOP(S64,PORT(3),REG(19)),

		MOVEOPIMM(S64, REG(25), IMMU64(3)),
		MOVEOPIMM(S64, REG(26), IMMU64(5)),
		MOVEOPIMM(S64, REG(29), IMMU64(7)),
		BINOP(ADD,S64,REG(24),REG(25),REG(26)),
		UNOP(NEG,S64,REG(27),REG(24)),
		BINOP(ADD,S64,REG(28),REG(27),REG(29)),
		BINOPIMM(MUL,S64,REG(29),REG(28),IMMU64(2)),
		BINOP(ADD,S64,REG(25),REG(27),REG(28)),
		BINOPIMM(SUB,S64,REG(28),REG(28),IMMU64(1)),
		BINOP(ADD,S64,REG(25),REG(29),REG(26)),
		SETRETOP(S64,PORT(4),REG(25)),

		RETOP,
		HALTOP,
	};

	JIRIMAGE image;
	JIRIMAGE_init(&image, prog, STATICARRLEN(prog));
	if(JIR_preprocess(&image))
		JIR_exec(&image);

	u64 *ports = image.port;
	bool status = (ports[0] == ports[1] && ports[1] == ports[2] && ports[2] == ports[3] && ports[3] == ports[4]);

	JIRIMAGE_destroy(&image);

	JIRIMAGE_init(&image, prog, STATICARRLEN(prog));
	JIR_optimize_regs(prog, STATICARRLEN(prog));
	if(JIR_preprocess(&image))
		JIR_exec(&image);

	ports = image.port;
	bool status2 = (ports[0] == ports[1] && ports[1] == ports[2] && ports[2] == ports[3] && ports[3] == ports[4]);

	JIRIMAGE_destroy(&image);

	return TEST_RESULT(status && status2);
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
		test_globals,
		test_register_allocation,
	};

	STATICARRFOR(tests)
		test_run(tests[tests_index]);

	return 0;
}
