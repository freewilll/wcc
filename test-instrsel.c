#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

void assert(long expected, long actual) {
    if (expected != actual) {
        printf("Expected %ld, got %ld\n", expected, actual);
        exit(1);
    }
}

void assert_tac(Tac *tac, int operation, Value *dst, Value *src1, Value *src2) {
    assert(operation, tac->operation);

    if (dst-> is_constant) assert(dst-> value, tac->dst-> value); else assert(dst-> vreg, tac->dst-> vreg);
    if (src1->is_constant) assert(src1->value, tac->src1->value); else assert(src1->vreg, tac->src1->vreg);
    if (src2->is_constant) assert(src2->value, tac->src2->value); else assert(src2->vreg, tac->src2->vreg);
}

void remove_reserved_physical_register_count_from_tac(Tac *ir) {
    Tac *tac;

    tac = ir;
    while (tac) {
        if (tac->dst  && tac->dst ->vreg && tac->dst ->vreg >= RESERVED_PHYSICAL_REGISTER_COUNT) tac->dst ->vreg -= RESERVED_PHYSICAL_REGISTER_COUNT;
        if (tac->src1 && tac->src1->vreg && tac->src1->vreg >= RESERVED_PHYSICAL_REGISTER_COUNT) tac->src1->vreg -= RESERVED_PHYSICAL_REGISTER_COUNT;
        if (tac->src2 && tac->src2->vreg && tac->src2->vreg >= RESERVED_PHYSICAL_REGISTER_COUNT) tac->src2->vreg -= RESERVED_PHYSICAL_REGISTER_COUNT;

        tac = tac->next;
    }
}

void start_ir() {
    ir_start = 0;
    init_instruction_selection_rules();
}

void finish_ir(Function *function) {
    function->ir = ir_start;
    eis1(function);
    remove_reserved_physical_register_count_from_tac(function->ir);
}

void nuke_rule(int operation, int src1, int src2) {
    int i;
    Rule *r;

    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);
        if (r->operation == operation && r->src1 == src1 && r->src2 == src2)
            r->operation = -1;
    }
}

void test_instrsel() {
    Symbol *symbol;
    Function *function;
    Tac *tac;

    symbol = malloc(sizeof(Symbol));
    memset(symbol, 0, sizeof(Symbol));

    function = new_function();
    symbol->function = function;

    // c1 + c2, with both cst/reg & reg/cst rules missing, forcing two register loads.
    // c1 goes into v2 and c2 goes into v3
    start_ir();
    nuke_rule(IR_ADD, CST, REG); nuke_rule(IR_ADD, REG, CST);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,                   X_MOV, v(2), c(1), v(2));
    assert_tac(ir_start->next,             X_MOV, v(3), c(2), v(3));
    assert_tac(ir_start->next->next,       X_MOV, v(1), v(3), v(1));
    assert_tac(ir_start->next->next->next, X_ADD, v(1), v(2), v(1));

    // c1 + c2, with only the cst/reg rule, forcing a register load for c2 into v2.
    start_ir();
    nuke_rule(IR_ADD, REG, CST);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,             X_MOV, v(2), c(2), v(2));
    assert_tac(ir_start->next,       X_MOV, v(1), v(2), v(1));
    assert_tac(ir_start->next->next, X_ADD, v(1), c(1), v(1));

    // c1 + c2, with only the reg/cst rule, forcing a register load for c1 into v2.
    start_ir();
    nuke_rule(IR_ADD, CST, REG);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,             X_MOV, v(2), c(1), v(2));
    assert_tac(ir_start->next,       X_MOV, v(1), v(2), v(1));
    assert_tac(ir_start->next->next, X_ADD, v(1), c(2), v(1));

    // r1 + r2. No loads are necessary, this is the simplest add operation.
    start_ir();
    i(0, IR_ADD, v(3), v(1), v(2));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(3), v(2), v(3));
    assert_tac(ir_start->next, X_ADD, v(3), v(1), v(3));
}

int main() {
    ssa_physical_register_count = 12;
    ssa_physical_register_count = 0;
    opt_optimize_arithmetic_operations = 1;

    init_allocate_registers();
    init_instruction_selection_rules();
    test_instrsel();
}
