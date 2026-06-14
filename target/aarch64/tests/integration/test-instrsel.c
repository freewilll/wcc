#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"
#include "../../aarch64.h"

Function *function;

void assert_long(long expected, long actual);

// Run with a single instruction
Tac *si(Function *function, int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    start_ir();
    i(label, operation, dst, src1, src2);
    finish_ir(function);

    return tac;
}

void test_instrsel_constant_bitshift(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_BSHL v << c
    si(function, 0, IR_BSHL, vsz(3, TYPE_CHAR), vsz(1, TYPE_CHAR), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_INT), vsz(1, TYPE_INT), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(1));
    assert_target_op("lsl         r2x, r1x, 1");

    // IR_ASHR v << c
    si(function, 0, IR_ASHR, vsz(3, TYPE_CHAR), vsz(1, TYPE_CHAR), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_INT), vsz(1, TYPE_INT), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(1));
    assert_target_op("asr         r2x, r1x, 1");

    // IR_BSHR v << c
    si(function, 0, IR_BSHR, vusz(3, TYPE_CHAR), vusz(1, TYPE_CHAR), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_INT), vusz(1, TYPE_INT), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_LONG), vusz(1, TYPE_LONG), c(1));
    assert_target_op("lsr         r2x, r1x, 1");
}

// Immediates in add/sub operations can be encoded in the instruction if.
// the constant is 12-bit immediate, optionally shifted left by 12 bits.
void test_instrsel_add_sub_constant(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_ADD v1 + v1 + c
    si(function, 0, IR_ADD, vsz(3, TYPE_CHAR),   vsz(1, TYPE_CHAR),   ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_SHORT),  vsz(1, TYPE_SHORT),  ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_INT),    vsz(1, TYPE_INT),    ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_LONG),   vsz(1, TYPE_LONG),   ci(1)); assert_target_op("add         r2x, r1x, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(1)); assert_target_op("add         r2x, r1x, 1");

    // IR_SUB v1 + v1 + c
    si(function, 0, IR_SUB, vsz(3, TYPE_CHAR),   vsz(1, TYPE_CHAR),   ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_SHORT),  vsz(1, TYPE_SHORT),  ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_INT),    vsz(1, TYPE_INT),    ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_LONG),   vsz(1, TYPE_LONG),   ci(1)); assert_target_op("sub         r2x, r1x, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(1)); assert_target_op("sub         r2x, r1x, 1");

    // Check encodable constants
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(4095));        assert_target_op("add         r2x, r1x, 4095");     // 0xfff
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(4096));        assert_target_op("add         r2x, r1x, 4096");     // 0x001 << 12
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(0x00800000));  assert_target_op("add         r2x, r1x, 8388608");  // 0x800 << 12
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(0x00fff000));  assert_target_op("add         r2x, r1x, 16773120"); // 0xfff << 12

    // Not encodable constant, it has to get loaded in a register
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(0x00ffffff));
    assert_long(AARCH64_OP_MOV_INT_CST, ir_start->operation.id);
    ir_start = ir_start->next;
    assert_target_op("add         r2x, r1x, r3x");
}

int main() {
    int verbose;

    verbose = 0;
    failures = 0;

    init_memory_management_for_translation_unit();

    function = new_function("test");
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    opt_optimize_arithmetic_operations = 1;
    string_literals = malloc(MAX_STRING_LITERALS);

    init_allocate_registers();
    init_instruction_selection_rules();

    if (verbose) printf("Running instrsel test_instrsel_constant_bitshift\n"); test_instrsel_constant_bitshift();
    if (verbose) printf("Running instrsel test_instrsel_add_sub_constant\n"); test_instrsel_add_sub_constant();

    if (failures) {
        printf("%d tests failed\n", failures);
        exit(1);
    }
    printf("All tests succeeded\n");
}
