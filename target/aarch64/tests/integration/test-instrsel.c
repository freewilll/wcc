#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"
#include "../../aarch64.h"

Function *function;

// Run with a single instruction
Tac *si(Function *function, int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    start_ir();
    i(label, operation, dst, src1, src2);
    finish_ir(function);

    return tac;
}

void test_instrsel_constant_bitshift() {
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

    if (failures) {
        printf("%d tests failed\n", failures);
        exit(1);
    }
    printf("All tests succeeded\n");
}
