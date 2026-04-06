#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../wcc.h"

static void assert_int(int expected, int actual) {
    if (expected != actual) {
        failures++;
        printf("Expected %d, got %d\n", expected, actual);
    }
}

static Tac *run_function_params_compiler(const char *code) {
    char *filename =  make_temp_filename("/tmp/XXXXXX.c");

    FILE *f = fopen(filename, "w");
    fprintf(f, "%s\n", code);
    fclose(f);

    init_lexer_from_filename(filename);
    init_parser();
    init_scopes();
    parse();

    for (int i = 0; i < global_scope->symbol_list->length; i++) {
        Symbol *symbol = global_scope->symbol_list->elements[i];
        if (symbol->type->type == TYPE_FUNCTION && symbol->function->is_defined) {
            Function *function = symbol->function;
            if (!strcmp(symbol->identifier, "test")) {
                run_compiler_phases(function, symbol->identifier, COMPILE_START_AT_BEGINNING, COMPILE_STOP_AFTER_FUNCTION_PARAM_MOVES);

                // Move ir_start to first non-labelled non-noop for convenience
                ir_start = function->ir;
                while (ir_start && ir_start->operation == IR_NOP && !ir_start->label) ir_start = ir_start->next;
                function->ir = ir_start;
                return ir_start;
            }
        }
    }

    panic("Unexpectedly didn't compile the test function");
}

static void test_int(void) {
    Tac *ir = run_function_params_compiler("int test(int i) { label: int j = i; goto label; }");

    // r3:int = r4_LRpreg6:int
    assert_tac(ir, IR_MOVE, vsz(3, TYPE_INT), vsz(4, TYPE_INT), 0);
    assert_int(LIVE_RANGE_PREG_RDI_INDEX, ir->src1->live_range_preg);
    assert_int(0, !!ir->label);

    // label: nop
    ir = ir->next;
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

static void test_int_on_stack(void) {
    Tac *ir = run_function_params_compiler("int test(int i) { label: int &j = &i; goto label; }");

    // S[-2]:int = r4_LRpreg6:int
    assert_tac(ir, IR_MOVE, S(-2), vsz(4, TYPE_INT), 0);
    assert_int(LIVE_RANGE_PREG_RDI_INDEX, ir->src1->live_range_preg);
    assert_int(0, !!ir->label);

    // label: nop
    ir = ir->next;
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

static void test_struct_return_value(void) {
    Tac *ir = run_function_params_compiler(
        "struct s{long i, j, k;}; "
        "struct s test() { "
            "label: struct s s = {0}; "
            "return s; "
            "goto label; "
        "}");

    // r1:*void = r2_LRpreg6:*void
    assert_tac(ir, IR_MOVE, p(vsz(1, TYPE_VOID)), vsz(2, TYPE_INT), 0);
    assert_int(LIVE_RANGE_PREG_RDI_INDEX, ir->src1->live_range_preg);
    assert_int(0, !!ir->label);

    // label: nop
    ir = ir->next;
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

#define ASSERT_REG_PARAM(r1, r2, preg) \
    assert_tac(ir, IR_MOVE, vsz(r1, TYPE_INT), vsz(r2, TYPE_INT), 0); \
    assert_int(preg, ir->src1->live_range_preg); \
    assert_int(0, !!ir->label); \
    ir = ir->next;

static void test_param_on_stack(void) {
    Tac *ir = run_function_params_compiler(
        "void test(int i1, int i2, int i3, int i4, int i5, int i6, int i7) { "
            "label: goto label;"
    "}");

    // Params in registers
    ASSERT_REG_PARAM(1,  2, LIVE_RANGE_PREG_RDI_INDEX);
    ASSERT_REG_PARAM(3,  4, LIVE_RANGE_PREG_RSI_INDEX);
    ASSERT_REG_PARAM(5,  6, LIVE_RANGE_PREG_RDX_INDEX);
    ASSERT_REG_PARAM(7,  8, LIVE_RANGE_PREG_RCX_INDEX);
    ASSERT_REG_PARAM(9,  10, LIVE_RANGE_PREG_R08_INDEX);
    ASSERT_REG_PARAM(11, 12, LIVE_RANGE_PREG_R09_INDEX);

    // r13:int = &S[2]:int
    assert_tac(ir, IR_MOVE, vsz(13, TYPE_INT), S(2), 0);
    assert_int(0, !!ir->label);

    // label: nop
    ir = ir->next;
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

// Test recreating a small struct from registers
static void test_small_struct_in_registers_in_func_call(void) {
    Tac *ir = run_function_params_compiler(
        "struct s{int i;}; "
        "test(struct s s) { "
            "label: goto label; "
        "}");

    // declare {l}S[-1]:struct s
    assert_int(IR_DECL_LOCAL_COMP_OBJ, ir->operation);
    ir = ir->next;

    // {l}S[-1]:unsigned int = r1_LRpreg6:unsigned int
    assert_tac(ir, IR_MOVE, S(-1), vsz(1, TYPE_INT), 0);
    assert_int(LIVE_RANGE_PREG_RDI_INDEX, ir->src1->live_range_preg);
    assert_int(0, !!ir->label);

    // label: nop
    ir = ir->next;
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

// Test recreating a larger struct from registers, using a shift register
static void test_big_struct_in_registers_in_func_call(void) {
    Tac *ir = run_function_params_compiler(
        "struct s{char c[5];}; "
        "test(struct s s) { "
            "label: goto label; "
        "}");

    // declare {l}S[-1]:struct s
    assert_int(IR_DECL_LOCAL_COMP_OBJ, ir->operation);
    ir = ir->next;

    // {l}S[-1]:unsigned int = r1_LRpreg6:unsigned int
    assert_tac(ir, IR_MOVE, S(-1), vsz(1, TYPE_INT), 0);
    assert_int(LIVE_RANGE_PREG_RDI_INDEX, ir->src1->live_range_preg);
    assert_int(0, !!ir->label);
    ir = ir->next;

    // 1:long = r1_LRpreg6:long >> 32:long
    assert_tac(ir, IR_BSHR, vsz(1, TYPE_INT), vsz(1, TYPE_INT), c(32));
    ir = ir->next;

    // {l}S[-1][4]:unsigned char = r1:unsigned char
    assert_tac(ir, IR_MOVE, S(-1), vsz(1, TYPE_CHAR), 0);
    assert_int(0, !!ir->label);
    ir = ir->next;

    // label: nop
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

static void test_one_float(void) {
    Tac *ir = run_function_params_compiler(
        "struct s{float f;}; "
        "test(struct s s) { "
            "label: goto label; "
        "}");

    // declare {l}S[-1]:struct s
    assert_int(IR_DECL_LOCAL_COMP_OBJ, ir->operation);
    ir = ir->next;

    // {l}S[-1]:float = r1_LRpreg13:float
    assert_tac(ir, IR_MOVE, S(-1), vsz(1, TYPE_FLOAT), 0);
    assert_int(LIVE_RANGE_PREG_XMM00_INDEX, ir->src1->live_range_preg);
    assert_int(0, !!ir->label);

    // label: nop
    ir = ir->next;
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

static void test_one_double(void) {
    Tac *ir = run_function_params_compiler(
        "struct s{double d;}; "
        "test(struct s s) { "
            "label: goto label; "
        "}");

    // declare {l}S[-1]:struct s
    assert_int(IR_DECL_LOCAL_COMP_OBJ, ir->operation);
    ir = ir->next;

    // {l}S[-1]:double = r1_LRpreg13:double
    assert_tac(ir, IR_MOVE, S(-1), vsz(1, TYPE_DOUBLE), 0);
    assert_int(LIVE_RANGE_PREG_XMM00_INDEX, ir->src1->live_range_preg);
    assert_int(0, !!ir->label);

    // label: nop
    ir = ir->next;
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

static void test_two_floats(void) {
    Tac *ir = run_function_params_compiler(
        "struct s{float f1, f2;}; "
        "test(struct s s) { "
            "label: goto label; "
        "}");

    // declare {l}S[-1]:struct s
    assert_int(IR_DECL_LOCAL_COMP_OBJ, ir->operation);
    ir = ir->next;

    // r2:long = r1_LRpreg13:double
    assert_tac(ir, IR_MOVE_PREG_CLASS, vsz(2, TYPE_LONG), vsz(1, TYPE_DOUBLE), 0);
    assert_int(LIVE_RANGE_PREG_XMM00_INDEX, ir->src1->live_range_preg);
    assert_int(0, !!ir->label);
    ir = ir->next;

    // {l}S[-1]:long = r2:long
    assert_tac(ir, IR_MOVE, S(-1), vsz(2, TYPE_LONG), 0);
    assert_int(0, !!ir->label);
    ir = ir->next;

    // label: nop
    assert_tac(ir, IR_NOP, 0, 0, 0);
    assert_int(1, !!ir->label);
}

int main() {
    int verbose;

    verbose = 0;
    failures = 0;

    init_memory_management_for_translation_unit();
    init_allocate_registers();
    init_instruction_selection_rules();

    // Test a regression where function parameter assignment hijacks a label at the
    // start of a function instead of adding instructions before the label.
    if (verbose) printf("Running functions int\n");                                        test_int();
    if (verbose) printf("Running functions int_on_stack\n");                               test_int_on_stack();
    if (verbose) printf("Running functions struct_return_value\n");                        test_struct_return_value();
    if (verbose) printf("Running functions param_on_stack\n");                             test_param_on_stack();
    if (verbose) printf("Running functions small_struct_in_registers_in_func_call\n");     test_small_struct_in_registers_in_func_call();
    if (verbose) printf("Running functions big_struct_in_registers\n");                    test_big_struct_in_registers_in_func_call();
    if (verbose) printf("Running fucntions one_float\n");                                  test_one_float();
    if (verbose) printf("Running fucntions one_double\n");                                 test_one_double();
    if (verbose) printf("Running fucntions two_floats\n");                                 test_two_floats();

    if (failures) {
        printf("%d tests failed\n", failures);
        exit(1);
    }
    printf("All tests succeeded\n");
}
