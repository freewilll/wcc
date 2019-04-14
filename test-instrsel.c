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

void assert_value(Value *v1, Value *v2) {
    if (v1->is_constant)
        assert(v1->value, v2->value);
    else if (v1->is_string_literal)
        assert(v1->vreg, v2->vreg);
    else if (v1->vreg)
        assert(v1->vreg, v2->vreg);
    else if (v1->global_symbol)
        assert(v1->global_symbol->identifier[1], v2->global_symbol->identifier[1]);
    else if (v1->label)
        assert(v1->label, v2->label);
    else
        panic("Don't know how to assert_value");
}

void assert_tac(Tac *tac, int operation, Value *dst, Value *src1, Value *src2) {
    assert(operation, tac->operation);

    if (dst) assert_value(tac->dst, dst);
    if (src1) assert_value(tac->src1, src1);
    if (src2) assert_value(tac->src2, src2);

    if (dst  && dst-> is_constant)
        assert(dst-> value, tac->dst-> value);
    else if (dst )
        assert(dst-> vreg, tac->dst-> vreg);

    if (src1 && src1->is_constant)
        assert(src1->value, tac->src1->value);
    else if (src1)
        assert(src1->vreg, tac->src1->vreg);

    if (src2 && src2->is_constant)
        assert(src2->value, tac->src2->value);
    else if (src2)
        assert(src2->vreg, tac->src2->vreg);

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
    Tac *tac;

    function->ir = ir_start;
    eis1(function);
    remove_reserved_physical_register_count_from_tac(function->ir);

    // Move ir_start to first non-noop for convenience
    ir_start = function->ir;
    while (ir_start->operation == IR_NOP) ir_start = ir_start->next;
    function->ir = ir_start;
}

void nuke_rule(int non_terminal, int operation, int src1, int src2) {
    int i;
    Rule *r;

    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);
        if (r->operation == operation && r->non_terminal == non_terminal && r->src1 == src1 && r->src2 == src2)
            r->operation = -1;
    }
}

void test_instrsel() {
    Function *function;
    Tac *tac;

    function = new_function();

    // c1 + c2, with both cst/reg & reg/cst rules missing, forcing two register loads.
    // c1 goes into v2 and c2 goes into v3
    start_ir();
    nuke_rule(REG, IR_ADD, CST, REG); nuke_rule(REG, IR_ADD, REG, CST);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,                   X_MOV, v(2), c(1), v(2));
    assert_tac(ir_start->next,             X_MOV, v(3), c(2), v(3));
    assert_tac(ir_start->next->next,       X_MOV, v(1), v(3), v(1));
    assert_tac(ir_start->next->next->next, X_ADD, v(1), v(2), v(1));

    // c1 + c2, with only the cst/reg rule, forcing a register load for c2 into v2.
    start_ir();
    nuke_rule(REG, IR_ADD, REG, CST);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,             X_MOV, v(2), c(2), v(2));
    assert_tac(ir_start->next,       X_MOV, v(1), v(2), v(1));
    assert_tac(ir_start->next->next, X_ADD, v(1), c(1), v(1));

    // c1 + c2, with only the reg/cst rule, forcing a register load for c1 into v2.
    start_ir();
    nuke_rule(REG, IR_ADD, CST, REG);
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

    // arg c with only the reg rule. Forces a load of c into r1
    start_ir();
    nuke_rule(0, IR_ARG, CST, CST);
    i(0, IR_ARG, 0, c(0), c(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(1), v(1));
    assert_tac(ir_start->next, X_ARG, 0,    c(0), v(1));

    // arg c
    start_ir();
    i(0, IR_ARG, 0, c(0), c(1));
    finish_ir(function);
    assert_tac(ir_start, X_ARG, 0, c(0), c(1));

    // arg r
    start_ir();
    i(0, IR_ARG, 0, c(0), v(1));
    finish_ir(function);
    assert_tac(ir_start, X_ARG, 0, c(0), v(1));

    // arg s
    start_ir();
    i(0, IR_ARG, 0, c(0), s(1));
    finish_ir(function);
    assert_tac(ir_start,       X_LEA, 0, s(1), v(1));
    assert_tac(ir_start->next, X_ARG, 0, c(0), v(1));

    // arg g
    start_ir();
    i(0, IR_ARG, 0, c(0), g(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, 0, g(1), v(1));
    assert_tac(ir_start->next, X_ARG, 0, c(0), v(1));

    // Store c in g
    start_ir();
    i(0, IR_ASSIGN, g(1), c(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, 0, c(1), g(1));

    // Store c in g with only the reg fule, forcing c into r1
    start_ir();
    nuke_rule(GLB, IR_ASSIGN, CST, 0);
    i(0, IR_ASSIGN, g(1), c(1), 0);
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, 0, c(1), v(1));
    assert_tac(ir_start->next, X_MOV, 0, v(1), g(1));

    // Store v1 in g
    start_ir();
    i(0, IR_ASSIGN, g(1), v(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, 0, v(1), g(1));

    // Load g into r1
    start_ir();
    i(0, IR_ASSIGN, v(1), g(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, 0, g(1), v(1));

    // jz with r1
    start_ir();
    i(0, IR_JZ,  0,    v(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_CMPZ, 0, v(1), 0);
    assert_tac(ir_start->next, X_JZ,   0, l(1), 0);

    // jz with global
    start_ir();
    i(0, IR_JZ,  0,    g(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_CMPZ, 0, g(1), 0);
    assert_tac(ir_start->next, X_JZ,   0, l(1), 0);

    // jnz with r1
    start_ir();
    i(0, IR_JNZ, 0,    v(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_CMPZ, 0, v(1), 0);
    assert_tac(ir_start->next, X_JNZ,  0, l(1), 0);

    // jnz with global
    start_ir();
    i(0, IR_JNZ, 0,    g(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_CMPZ, 0, g(1), 0);
    assert_tac(ir_start->next, X_JNZ,  0, l(1), 0);
}

int main() {
    ssa_physical_register_count = 12;
    ssa_physical_register_count = 0;
    opt_optimize_arithmetic_operations = 1;
    string_literals = malloc(MAX_STRING_LITERALS);

    init_allocate_registers();
    init_instruction_selection_rules();
    test_instrsel();
}
