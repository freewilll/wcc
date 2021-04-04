#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

Function *function;

void assert(long expected, long actual) {
    if (expected != actual) {
        failures++;
        printf("Expected %ld, got %ld\n", expected, actual);
    }
}

void n() {
    ir_start = ir_start->next;
}

void nop() {
    assert(IR_NOP, ir_start->operation);
    ir_start = ir_start->next;
}

char *rx86op(Tac *tac) {
    return render_x86_operation(tac, 0, 0, 0);
}

void assert_x86_op(char *expected) {
    char *got;

    if (!ir_start) {
        printf("Expected %s, got nothing\n", expected);
        failures++;
        return;
    }

    got = render_x86_operation(ir_start, 0, 0, 0);;

    if (!got) {
        printf("Mismatch:\n  expected: %s\n  got:      null\n", expected);
        failures++;
    }
    else {

        if (strcmp(got, expected)) {
            printf("Mismatch:\n  expected: %s\n  got:      %s\n", expected, got);
            failures++;
        }
    }

    n();
}

char *assert_rx86_preg_op(char *expected) {
    char *got;

    got = render_x86_operation(ir_start, 0, 0, 1);
    if (strcmp(got, expected)) {
        printf("Mismatch:\n  expected: %s\n  got:      %s\n", expected, got);
        failures++;
    }

    n();
}

// Run with a single instruction
Tac *si(Function *function, int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    start_ir();
    i(label, operation, dst, src1, src2);
    finish_ir(function);

    return tac;
}

void nuke_rule(int dst, int operation, int src1, int src2) {
    int i;
    Rule *r;

    for (i = 0; i < instr_rule_count; i++) {
        r = &(instr_rules[i]);
        if (r->operation == operation && r->dst == dst && r->src1 == src1 && r->src2 == src2)
            r->operation = -1;
    }
}

void test_instrsel_tree_merging_type_merges() {
    // Ensure type conversions are merged correctly. This tests a void * being converted to a char *
    remove_reserved_physical_registers = 1;
    start_ir();
    i(0, IR_MOVE,        asz(2, TYPE_CHAR), asz(1, TYPE_VOID), 0);
    i(0, IR_MOVE_TO_PTR, vsz(2, TYPE_CHAR), vsz(2, TYPE_CHAR), c(1));
    finish_ir(function);
    assert_x86_op("movq    r1q, r4q");
    assert_x86_op("movb    $1, (r4q)");
}

void test_instrsel_tree_merging_register_constraint() {
    remove_reserved_physical_registers = 1;

    // Ensure the dst != src1 restriction is respected when merging trees
    start_ir();
    i(0, IR_NOP,    0,    0,    0   );   // required for the next label to nog segfault ssa
    i(1, IR_MOVE,   v(2), v(1), 0   );   // 1: v2 = v1
    i(0, IR_ADD ,   v(1), v(2), v(3));   //    v1 = v2 + v3
    i(0, IR_JMP, 0, l(1), 0         );   //    jmp 1. Required to ensure v(1) is in liveout
                                         //           otherwise, it'll get optimized into a new reg
    finish_ir(function);
    assert_x86_op("movq    r1q, r2q");
    assert_x86_op("movq    r3q, r1q");
    assert_x86_op("addq    r2q, r1q");
    assert_x86_op("jmp     .l1"     );
}

void test_instrsel_tree_merging() {
    int j;
    Tac *tac;

    remove_reserved_physical_registers = 0;

    // Test edge case of a split block with variables being live on the way
    // out. This should pervent tree merging of the assigns with the first equals.
    // The label splits the block and causes v(1) and v(2) to be in block[0] liveout
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0   );
    i(0, IR_MOVE, v(2), c(2), 0   );
    i(0, IR_EQ,   v(3), v(1), v(2));
    i(1, IR_EQ,   v(4), v(1), v(2));
    finish_ir(function);

    // Ensure both CMP instructions operate on registers
    tac = ir_start;
    for (j = 0; j < 2; j++) {
        while (tac && tac->operation != X_CMP) tac = tac->next;
        assert(1, !!tac);
        assert(1, tac->src1->vreg > 0);
        assert(1, tac->src2->vreg > 0);
    }

    // Ensure a liveout in another block doesn't lead to an attempted
    // merge.
    remove_reserved_physical_registers = 1;
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0   );
    i(0, IR_MOVE, v(2), c(2), 0   );
    i(1, IR_EQ,   v(4), v(1), v(2));
    finish_ir(function);
    assert_x86_op("movq    $1, r1q" );
    assert_x86_op("movq    $2, r2q" ); nop();
    assert_x86_op("cmpq    r2q, r1q");
    assert_x86_op("sete    r3b"     );
    assert_x86_op("movzbq  r3b, r3q");

    // Ensure a dst in assign to an lvalue keeps the value alive, so
    // that a merge is prevented later on.
    start_ir();
    i(0, IR_MOVE,        v(3),              c(1),              0   ); // r3 = 1   <- r3 is used twice, so no tree merge happens
    i(0, IR_MOVE,        v(2),              v(3),              0   ); // r2 = r3
    i(0, IR_MOVE_TO_PTR, vsz(3, TYPE_LONG), vsz(3, TYPE_LONG), v(4)); // (r3) = r4
    finish_ir(function);
    assert_x86_op("movq    $1, r2q"   );
    assert_x86_op("movq    r2q, r1q"  );
    assert_x86_op("movq    r4q, (r2q)");

    // Ensure the assign to pointer instruction copies src1 to dst first
    remove_reserved_physical_registers = 1;
    start_ir();
    i(0, IR_ADDRESS_OF,  a(1), g(1), 0);
    i(0, IR_MOVE_TO_PTR, v(1), v(1), c(1));
    finish_ir(function);
    assert_x86_op("leaq    g1(%rip), r3q");
    assert_x86_op("movq    $1, (r3q)"    );

    // Test tree merges only happening on adjacent trees.
    // This is realistic example of a value swap of two values in memory.
    start_ir();
    i(0, IR_MOVE, v(1), g(1), 0);
    i(0, IR_MOVE, v(2), g(2), 0);
    i(0, IR_MOVE, g(1), v(2), 0);
    i(0, IR_MOVE, g(2), v(1), 0);
    finish_ir(function);
    assert_x86_op("movq    g1(%rip), r1q");
    assert_x86_op("movq    g2(%rip), r3q");
    assert_x86_op("movq    r3q, g1(%rip)");
    assert_x86_op("movq    r1q, g2(%rip)");
}

void test_cmp_with_conditional_jmp(Function *function, int cmp_operation, int jmp_operation, int x86_jmp_operation) {
    start_ir();
    i(0, cmp_operation,  v(3), v(1), v(2));
    i(0, jmp_operation,  0,    v(3), l(1));
    i(1, IR_NOP,         0,    0,    0   );
    finish_ir(function);
    assert_tac(ir_start,       X_CMP,             0, v(1), v(2));  // v(4) is allocated but not used
    assert_tac(ir_start->next, x86_jmp_operation, 0, l(1), 0   );
}

void test_less_than_with_conditional_jmp(Function *function, Value *src1, Value *src2, int disable_adrq_load, char *template) {
    start_ir();
    if (disable_adrq_load) nuke_rule(REGQ, 0, ADRQ, 0); // Disable direct ADRQ into register passthrough
    i(0, IR_LT,  v(3), src1, src2);
    i(0, IR_JZ,  0,    v(3), l(1));
    i(1, IR_NOP, 0,    0,    0   );
    src1 = dup_value(src1);
    src2 = dup_value(src2);
    finish_ir(function);
    assert_x86_op(template);
    assert_x86_op("jge     .l1");
}

void test_cmp_with_assignment(Function *function, int cmp_operation, int x86_set_operation) {
    start_ir();
    i(0, cmp_operation, v(3), v(1), v(2));
    finish_ir(function);
    assert_tac(ir_start,             X_CMP,             0,    v(1), v(2));
    assert_tac(ir_start->next,       x86_set_operation, v(3), 0,    0   );
    assert_tac(ir_start->next->next, X_MOVZBQ,          v(3), v(3), 0   );
}

void test_less_than_with_cmp_assignment(Function *function, Value *src1, Value *src2, char *t1, char *t2, char *t3) {
    // dst is the renumbered live range that the output goes to. It's basically the first free register after src1 and src2.
    start_ir();
    nuke_rule(REGQ, 0, ADRV, 0); // Disable direct ADRQ into register passthrough
    i(0, IR_LT, v(3), src1, src2);
    src1 = dup_value(src1);
    src2 = dup_value(src2);
    finish_ir(function);
    assert_x86_op(t1);
    assert_x86_op(t2);
    assert_x86_op(t3);
}

void test_cst_load(int operation, Value *dst, Value *src, char *code) {

    si(function, 0, operation, dst, src, 0);
    assert_x86_op(code);
}

void test_instrsel_constant_loading() {
    Tac *tac;
    long l;

    remove_reserved_physical_registers = 1;
    l = 4294967296;

    // Note: the arg push tests cover the rules that load constant into temporary registers

    // IR_MOVE
    // with a 32 bit int
    test_cst_load(IR_MOVE, vsz(3, TYPE_CHAR),  c(1), "movl    $1, r1l");
    test_cst_load(IR_MOVE, vsz(3, TYPE_SHORT), c(1), "movl    $1, r1l");
    test_cst_load(IR_MOVE, vsz(3, TYPE_INT),   c(1), "movl    $1, r1l");
    test_cst_load(IR_MOVE, vsz(3, TYPE_LONG),  c(1), "movq    $1, r1q");

    // with a 64 bit long. The first 3 are overflows, so a programmer error.
    test_cst_load(IR_MOVE, vsz(3, TYPE_CHAR),  c(l), "movq    $4294967296, r1q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_SHORT), c(l), "movq    $4294967296, r1q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_INT),   c(l), "movq    $4294967296, r1q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_LONG),  c(l), "movq    $4294967296, r1q");

    // IR_MOVE
    // with a 32 bit int
    test_cst_load(IR_MOVE, vsz(3, TYPE_CHAR),  c(1), "movl    $1, r1l");
    test_cst_load(IR_MOVE, vsz(3, TYPE_SHORT), c(1), "movl    $1, r1l");
    test_cst_load(IR_MOVE, vsz(3, TYPE_INT),   c(1), "movl    $1, r1l");
    test_cst_load(IR_MOVE, vsz(3, TYPE_LONG),  c(1), "movq    $1, r1q");

    // with a 64 bit long. The first 3 are overflows, so a programmer error.
    test_cst_load(IR_MOVE, vsz(3, TYPE_CHAR),  c(l), "movq    $4294967296, r1q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_SHORT), c(l), "movq    $4294967296, r1q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_INT),   c(l), "movq    $4294967296, r1q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_LONG),  c(l), "movq    $4294967296, r1q");
}

void test_instrsel() {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // Load constant into register with IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0);
    finish_ir(function);
    assert_x86_op("movq    $1, r1q");

    // c1 + c2, with both cst/reg & reg/cst rules missing, forcing two register loads.
    // c1 goes into v2 and c2 goes into v3
    start_ir();
    disable_merge_constants = 1;
    nuke_rule(REGQ, IR_ADD, CSTL, REGQ); nuke_rule(REGQ, IR_ADD, REGQ, CSTL);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,                   X_MOV, v(2), c(1), 0   );
    assert_tac(ir_start->next,             X_MOV, v(3), c(2), 0   );
    assert_tac(ir_start->next->next,       X_MOV, v(1), v(3), 0   );
    assert_tac(ir_start->next->next->next, X_ADD, v(1), v(2), v(1));

    // c1 + c2, with only the cst/reg rule, forcing a register load for c2 into v2.
    start_ir();
    disable_merge_constants = 1;
    nuke_rule(REG, IR_ADD, REGQ, CSTL);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,             X_MOV, v(2), c(2), 0   );
    assert_tac(ir_start->next,       X_MOV, v(1), v(2), 0   );
    assert_tac(ir_start->next->next, X_ADD, v(1), c(1), v(1));

    // c1 + c2, with only the reg/cst rule, forcing a register load for c1 into v2.
    start_ir();
    disable_merge_constants = 1;
    nuke_rule(REGQ, IR_ADD, CSTL, REGQ);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,             X_MOV, v(2), c(1), 0   );
    assert_tac(ir_start->next,       X_MOV, v(1), v(2), 0   );
    assert_tac(ir_start->next->next, X_ADD, v(1), c(2), v(1));

    // r1 + r2. No loads are necessary, this is the simplest add operation.
    start_ir();
    i(0, IR_ADD, v(3), v(1), v(2));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(3), v(2), 0   );
    assert_tac(ir_start->next, X_ADD, v(3), v(1), v(3));

    // r1 + S1
    start_ir();
    i(0, IR_ADD, v(2), v(1), S(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(2), v(1), 0   );
    assert_tac(ir_start->next, X_ADD, v(2), S(1), v(2));

    // S1 + r1
    start_ir();
    i(0, IR_ADD, v(2), v(1), S(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(2), v(1), 0   );
    assert_tac(ir_start->next, X_ADD, v(2), S(1), v(2));

    // r1 + g1
    start_ir();
    i(0, IR_ADD, v(2), v(1), g(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(2), v(1), 0   );
    assert_tac(ir_start->next, X_ADD, v(2), g(1), v(2));

    // g1 + r1
    start_ir();
    i(0, IR_ADD, v(2), v(1), g(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(2), v(1), 0   );
    assert_tac(ir_start->next, X_ADD, v(2), g(1), v(2));

    // c1 + g1
    start_ir();
    i(0, IR_ADD, v(2), c(1), g(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(1), 0   );
    assert_tac(ir_start->next, X_ADD, v(1), g(1), v(1));

    // g1 + c1
    start_ir();
    i(0, IR_ADD, v(2), g(1), c(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(1), 0   );
    assert_tac(ir_start->next, X_ADD, v(1), g(1), v(1));

    // arg c with only the reg rule. Forces a load of c into r1
    start_ir();
    nuke_rule(0, IR_ARG, CSTL, CSTL);
    i(0, IR_ARG, 0, c(0), c(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(1), 0   );
    assert_tac(ir_start->next, X_ARG, 0,    c(0), v(1));

    // arg c
    start_ir();
    i(0, IR_ARG, 0, c(0), c(1));
    finish_ir(function);
    assert_tac(ir_start, X_ARG, 0, c(0), c(1));

    // arg big c, which should go via a register
    start_ir();
    i(0, IR_ARG, 0, c(0), c(4294967296));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(4294967296), 0);
    assert_tac(ir_start->next, X_ARG, 0, c(0), v(1));

    start_ir();
    i(0, IR_ARG, 0, c(0), c(2147483648));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(2147483648), 0);
    assert_tac(ir_start->next, X_ARG, 0, c(0), v(1));

    start_ir();
    i(0, IR_ARG, 0, c(0), c(-2147483649));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(-2147483649), 0);
    assert_tac(ir_start->next, X_ARG, 0, c(0), v(1));

    // arg r
    start_ir();
    i(0, IR_ARG, 0, c(0), v(1));
    finish_ir(function);
    assert_tac(ir_start, X_ARG, 0, c(0), v(1));

    // arg s
    start_ir();
    i(0, IR_ARG, 0, c(0), s(1));
    finish_ir(function);
    assert_tac(ir_start,       X_LEA, v(1), s(1), 0   );
    assert_tac(ir_start->next, X_ARG, 0,    c(0), v(1));

    // arg g
    start_ir();
    i(0, IR_ARG, 0, c(0), g(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), g(1), 0   );
    assert_tac(ir_start->next, X_ARG, 0,    c(0), v(1));

    // Store c in g
    start_ir();
    i(0, IR_MOVE, g(1), c(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, g(1), c(1), 0);

    // Store c in g with only the reg fule, forcing c into r1
    start_ir();
    nuke_rule(MEMQ, IR_MOVE, CSTL, 0);
    i(0, IR_MOVE, g(1), c(1), 0);
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(1), 0);
    assert_tac(ir_start->next, X_MOV, g(1), v(1), 0);

    // Store v1 in g using IR_MOVE
    start_ir();
    i(0, IR_MOVE, g(1), v(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, g(1), v(1), 0);

    // Store a1 in g using IR_MOVE
    si(function, 0, IR_MOVE, gsz(1, TYPE_PTR + TYPE_CHAR),  asz(1, TYPE_VOID), 0); assert_x86_op("movq    r1q, g1(%rip)");
    si(function, 0, IR_MOVE, gsz(1, TYPE_PTR + TYPE_SHORT), asz(1, TYPE_VOID), 0); assert_x86_op("movq    r1q, g1(%rip)");
    si(function, 0, IR_MOVE, gsz(1, TYPE_PTR + TYPE_INT),   asz(1, TYPE_VOID), 0); assert_x86_op("movq    r1q, g1(%rip)");
    si(function, 0, IR_MOVE, gsz(1, TYPE_PTR + TYPE_LONG),  asz(1, TYPE_VOID), 0); assert_x86_op("movq    r1q, g1(%rip)");

    // Load g into r1 using IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), g(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, v(1), g(1), 0);

    // Load S into r1
    start_ir();
    i(0, IR_MOVE, v(1), S(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, v(1), S(1), 0);

    // Load g into r1 using IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), g(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, v(1), g(1), 0);

    // Load S into r1 using IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), S(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, v(1), S(1), 0);

    // Load Si into r1l using IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), Ssz(1, TYPE_INT), 0);
    finish_ir(function);
    assert_x86_op("movslq  16(%rbp), r2q");
    assert_x86_op("movq    r2q, r1q");

    // Push a local
    start_ir();
    i(0, IR_ARG, 0, c(0), S(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), S(1), 0   );
    assert_tac(ir_start->next, X_ARG, 0,    c(0), v(1));

    // Assign constant to a local
    start_ir();
    i(0, IR_MOVE, S(1), c(0), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, S(1), c(0), 0);

    // Assign constant to a local. Forces c into a register
    start_ir();
    nuke_rule(MEMQ, IR_MOVE, CSTL, 0);
    i(0, IR_MOVE, S(1), c(0), 0);
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(0), 0);
    assert_tac(ir_start->next, X_MOV, S(1), v(1), 0);

    // jz with r1
    start_ir();
    i(0, IR_JZ,  0,    v(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_TEST, 0, v(1), 0);
    assert_tac(ir_start->next, X_JZ,   0, l(1), 0);

    // jz with a1
    start_ir();
    nuke_rule(REGQ, 0, ADRQ, 0); // Disable direct ADRQ into register passthrough
    i(0, IR_JZ,  0,    a(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_TEST, 0, v(1), 0);
    assert_tac(ir_start->next, X_JZ,   0, l(1), 0);

    // jz with a1 *void
    start_ir();
    nuke_rule(REGQ, 0, ADRV, 0); // Disable direct ADRQ into register passthrough
    i(0, IR_JZ,  0,    asz(1, TYPE_VOID), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_TEST, 0, v(1), 0);
    assert_tac(ir_start->next, X_JZ,   0, l(1), 0);

    // jz with global
    start_ir();
    i(0, IR_JZ,  0,    g(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("cmp     $0, g1(%rip)");
    assert_x86_op("jz      .l1");

    // jnz with r1
    start_ir();
    i(0, IR_JNZ, 0,    v(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_TEST, 0, v(1), 0);
    assert_tac(ir_start->next, X_JNZ,  0, l(1), 0);

    // jz with a1
    start_ir();
    nuke_rule(REGQ, 0, ADRQ, 0); // Disable direct ADRQ into register passthrough
    i(0, IR_JNZ,  0,   a(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_TEST, 0, v(1), 0);
    assert_tac(ir_start->next, X_JNZ,  0, l(1), 0);

    // jz with a1 *void
    start_ir();
    nuke_rule(REGQ, 0, ADRV, 0); // Disable direct ADRQ into register passthrough
    i(0, IR_JNZ,  0,   asz(1, TYPE_VOID), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_tac(ir_start,       X_TEST, 0, v(1), 0);
    assert_tac(ir_start->next, X_JNZ,  0, l(1), 0);

    // jnz with global
    start_ir();
    i(0, IR_JNZ, 0,    g(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("cmp     $0, g1(%rip)");
    assert_x86_op("jnz     .l1");

    // JZ                                                          JNZ
    test_cmp_with_conditional_jmp(function, IR_EQ, IR_JNZ, X_JE ); test_cmp_with_conditional_jmp(function, IR_EQ, IR_JZ, X_JNE);
    test_cmp_with_conditional_jmp(function, IR_NE, IR_JNZ, X_JNE); test_cmp_with_conditional_jmp(function, IR_NE, IR_JZ, X_JE );
    test_cmp_with_conditional_jmp(function, IR_LT, IR_JNZ, X_JLT); test_cmp_with_conditional_jmp(function, IR_LT, IR_JZ, X_JGE);
    test_cmp_with_conditional_jmp(function, IR_GT, IR_JNZ, X_JGT); test_cmp_with_conditional_jmp(function, IR_GT, IR_JZ, X_JLE);
    test_cmp_with_conditional_jmp(function, IR_LE, IR_JNZ, X_JLE); test_cmp_with_conditional_jmp(function, IR_LE, IR_JZ, X_JGT);
    test_cmp_with_conditional_jmp(function, IR_GE, IR_JNZ, X_JGE); test_cmp_with_conditional_jmp(function, IR_GE, IR_JZ, X_JLT);

    // a < b with a conditional with different src1 and src2 operands
    test_less_than_with_conditional_jmp(function, v(1), c(1), 0, "cmpq    $1, r1q"      );
    test_less_than_with_conditional_jmp(function, a(1), c(1), 1, "cmpq    $1, r1q"      );
    test_less_than_with_conditional_jmp(function, g(1), c(1), 0, "cmpq    $1, g1(%rip)" );
    test_less_than_with_conditional_jmp(function, v(1), g(1), 0, "cmpq    g1(%rip), r1q");
    test_less_than_with_conditional_jmp(function, a(1), g(1), 1, "cmpq    g1(%rip), r1q");
    test_less_than_with_conditional_jmp(function, g(1), v(1), 0, "cmpq    r1q, g1(%rip)");
    test_less_than_with_conditional_jmp(function, v(1), S(1), 0, "cmpq    16(%rbp), r1q");
    test_less_than_with_conditional_jmp(function, a(1), S(1), 1, "cmpq    16(%rbp), r1q");
    test_less_than_with_conditional_jmp(function, S(1), v(1), 0, "cmpq    r1q, 16(%rbp)");
    test_less_than_with_conditional_jmp(function, S(1), a(1), 1, "cmpq    r1q, 16(%rbp)");
    test_less_than_with_conditional_jmp(function, S(1), c(1), 0, "cmpq    $1, 16(%rbp)" );

    // Conditional assignment with 2 registers
    test_cmp_with_assignment(function, IR_EQ, X_SETE);
    test_cmp_with_assignment(function, IR_NE, X_SETNE);
    test_cmp_with_assignment(function, IR_LT, X_SETLT);
    test_cmp_with_assignment(function, IR_GT, X_SETGT);
    test_cmp_with_assignment(function, IR_LE, X_SETLE);
    test_cmp_with_assignment(function, IR_GE, X_SETGE);

    // Test r1 = a < b with different src1 and src2 operands
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_CHAR), c(1),              "cmpq    $1, r1q",       "setl    r2b", "movzbq  r2b, r2q");
    test_less_than_with_cmp_assignment(function, v(1),              c(1),              "cmpq    $1, r1q",       "setl    r2b", "movzbq  r2b, r2q");
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_VOID), c(1),              "cmpq    $1, r1q",       "setl    r2b", "movzbq  r2b, r2q");
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_VOID), asz(1, TYPE_VOID), "cmpq    r1q, r2q",      "setl    r3b", "movzbq  r3b, r3q");
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_CHAR), asz(1, TYPE_VOID), "cmpq    r1q, r2q",      "setl    r3b", "movzbq  r3b, r3q");
    test_less_than_with_cmp_assignment(function, asz(1, TYPE_VOID), asz(2, TYPE_CHAR), "cmpq    r2q, r1q",      "setl    r3b", "movzbq  r3b, r3q");
    test_less_than_with_cmp_assignment(function, asz(1, TYPE_VOID), asz(2, TYPE_CHAR), "cmpq    r2q, r1q",      "setl    r3b", "movzbq  r3b, r3q");
    test_less_than_with_cmp_assignment(function, g(1),              c(1),              "cmpq    $1, g1(%rip)",  "setl    r1b", "movzbq  r1b, r1q");
    test_less_than_with_cmp_assignment(function, v(1),              g(1),              "cmpq    g1(%rip), r1q", "setl    r2b", "movzbq  r2b, r2q");
    test_less_than_with_cmp_assignment(function, g(1),              v(1),              "cmpq    r1q, g1(%rip)", "setl    r2b", "movzbq  r2b, r2q");
    test_less_than_with_cmp_assignment(function, S(1),              c(1),              "cmpq    $1, 16(%rbp)",  "setl    r1b", "movzbq  r1b, r1q");
    test_less_than_with_cmp_assignment(function, v(1),              S(1),              "cmpq    16(%rbp), r1q", "setl    r2b", "movzbq  r2b, r2q");
    test_less_than_with_cmp_assignment(function, S(1),              v(1),              "cmpq    r1q, 16(%rbp)", "setl    r2b", "movzbq  r2b, r2q");
}

// Convert (b, w, l, q) -> (1, 2, 3, 4)
int x86_size_to_int(char s) {
         if (s == 'b') return 1;
    else if (s == 'w') return 2;
    else if (s == 'l') return 3;
    else if (s == 'q') return 4;
    else panic1d("Unknown x86 size %d", s);
}

// Test addition with integer type combinations for (dst, src1, src2)
// The number of tests = 4 * 4 * 4 = 64.
void test_instrsel_types_add_vregs() {
    int dst, src1, src2, count;
    int extend_src1, extend_src2, type;
    Tac *tac;


    for (dst = 1; dst <= 4; dst++) {
        for (src1 = 1; src1 <= 4; src1++) {
            for (src2 = 1; src2 <= 4; src2++) {
                start_ir();
                tac = i(0, IR_ADD, v(3), v(1), v(2));
                tac->dst ->type = TYPE_CHAR + dst  - 1;
                tac->src1->type = TYPE_CHAR + src1 - 1;
                tac->src2->type = TYPE_CHAR + src2 - 1;
                finish_ir(function);

                // Count the number of intructions
                tac = ir_start;
                count = 0;
                while (tac) { count++; tac = tac->next; }

                // The type is the max of (dst, src1, src2)
                type = src1;
                if (src2 > type) type = src2;
                if (dst > type) type = dst;

                // Determine which src operands should be extended
                extend_src1 = extend_src2 = 0;
                if (src1 < src2) extend_src1 = 1;
                if (src1 > src2) extend_src2 = 1;
                if (src1 < dst) extend_src1 = 1;
                if (src2 < dst) extend_src2 = 1;

                // Check instruction counts match
                assert(extend_src1 + extend_src2 + 2, count);

                // The first instruction sign extends src1 if necessary
                if (extend_src1) {
                    assert(src1, x86_size_to_int(ir_start->x86_template[4]));
                    assert(type, x86_size_to_int(ir_start->x86_template[5]));
                    ir_start = ir_start->next;
                }

                // The first instruction sign extends src2 if necessary
                if (extend_src2) {
                    assert(src2, x86_size_to_int(ir_start->x86_template[4]));
                    assert(type, x86_size_to_int(ir_start->x86_template[5]));
                    ir_start = ir_start->next;
                }
            }
        }
    }
}

void test_instrsel_types_add_mem_vreg() {
    remove_reserved_physical_registers = 1;

    // A non exhaustive set of tests to check global/reg addition with various integer sizes

    // Test sign extension of globals
    // ------------------------------

    // c = vs + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_SHORT), gsz(1, TYPE_CHAR));
    assert(0, strcmp(rx86op(ir_start            ), "movsbw  g1(%rip), r3w"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movw    r3w, r2w"     ));
    assert(0, strcmp(rx86op(ir_start->next->next), "addw    r1w, r2w"     ));

    // c = vi + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_INT), gsz(1, TYPE_CHAR));
    assert(0, strcmp(rx86op(ir_start            ), "movsbl  g1(%rip), r3l"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movl    r3l, r2l"     ));
    assert(0, strcmp(rx86op(ir_start->next->next), "addl    r1l, r2l"     ));

    // c = vl + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_LONG), gsz(1, TYPE_CHAR));
    assert(0, strcmp(rx86op(ir_start            ), "movsbq  g1(%rip), r3q"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movq    r3q, r2q"     ));
    assert(0, strcmp(rx86op(ir_start->next->next), "addq    r1q, r2q"     ));

    // l = vi + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_LONG), vsz(1, TYPE_INT), gsz(1, TYPE_CHAR));
    assert(0, strcmp(rx86op(ir_start                  ), "movslq  r1l, r3q"     ));
    assert(0, strcmp(rx86op(ir_start->next            ), "movsbq  g1(%rip), r4q"));
    assert(0, strcmp(rx86op(ir_start->next->next      ), "movq    r4q, r2q"     ));
    assert(0, strcmp(rx86op(ir_start->next->next->next), "addq    r3q, r2q"     ));

    // l = vl + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_LONG), vsz(1, TYPE_LONG), gsz(1, TYPE_CHAR));
    assert(0, strcmp(rx86op(ir_start            ), "movsbq  g1(%rip), r3q"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movq    r3q, r2q"     ));
    assert(0, strcmp(rx86op(ir_start->next->next), "addq    r1q, r2q"     ));

    // l = gc + vl
    si(function, 0, IR_ADD, vsz(2, TYPE_LONG), gsz(1, TYPE_CHAR), vsz(1, TYPE_LONG));
    assert(0, strcmp(rx86op(ir_start            ), "movsbq  g1(%rip), r3q"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movq    r1q, r2q"     ));
    assert(0, strcmp(rx86op(ir_start->next->next), "addq    r3q, r2q"     ));

    // Test sign extension of locals
    // ------------------------------

    // c = vs + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_SHORT), Ssz(1, TYPE_CHAR));
    assert(0, strcmp(rx86op(ir_start            ), "movsbw  16(%rbp), r3w"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movw    r3w, r2w"     ));
    assert(0, strcmp(rx86op(ir_start->next->next), "addw    r1w, r2w"     ));

    // Test sign extension of registers
    // --------------------------------
    // c = vc + gs
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_CHAR), gsz(1, TYPE_SHORT));
    assert(0, strcmp(rx86op(ir_start            ), "movsbw  r1b, r3w"     ));
    assert(0, strcmp(rx86op(ir_start->next      ), "movw    r3w, r2w"     ));
    assert(0, strcmp(rx86op(ir_start->next->next), "addw    g1(%rip), r2w"));
}

void test_instrsel_types_cmp_assignment() {
    remove_reserved_physical_registers = 1;

    // Test s = l == l
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_SHORT), v(1), v(2));
    finish_ir(function);
    assert(0, strcmp(rx86op(ir_start            ), "cmpq    r2q, r1q"));
    assert(0, strcmp(rx86op(ir_start->next      ), "sete    r3b"     ));
    assert(0, strcmp(rx86op(ir_start->next->next), "movzbw  r3b, r3w"));

    // Test c = s == s
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_CHAR), vsz(1, TYPE_SHORT), vsz(2, TYPE_SHORT));
    finish_ir(function);
    assert(0, strcmp(rx86op(ir_start            ), "cmpw    r2w, r1w"));
    assert(0, strcmp(rx86op(ir_start->next      ), "sete    r3b"     ));

    // Test s = c == l
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_SHORT), vsz(1, TYPE_CHAR), vsz(2, TYPE_LONG));
    finish_ir(function);
    assert(0, strcmp(rx86op(ir_start                  ), "movsbq  r1b, r4q"));
    assert(0, strcmp(rx86op(ir_start->next            ), "cmpq    r2q, r4q"));
    assert(0, strcmp(rx86op(ir_start->next->next      ), "sete    r3b"     ));
    assert(0, strcmp(rx86op(ir_start->next->next->next), "movzbw  r3b, r3w"));
}

void test_instrsel_types_cmp_pointer() {
    int j;

    remove_reserved_physical_registers = 1;

    for (j = TYPE_VOID; j <= TYPE_LONG; j++) {
        start_ir();
        i(0, IR_EQ, v(3), asz(1, j), c(0));
        i(0, IR_JZ,  0,    v(3), l(1));
        i(1, IR_NOP, 0,    0,    0   );
        finish_ir(function);
        assert_x86_op("cmpq    $0, r1q");
        assert_x86_op("jne     .l1");

        start_ir();
        i(0, IR_EQ, v(3), asz(1, j), c(0));
        i(0, IR_JNZ, 0,    v(3), l(1));
        i(1, IR_NOP, 0,    0,    0   );
        finish_ir(function);
        assert_x86_op("cmpq    $0, r1q");
        assert_x86_op("je      .l1");
    }
}

void test_instrsel_returns() {
    remove_reserved_physical_registers = 1;

    // Return constant & vregs
    si(function, 0, IR_RETURN, 0, c(4294967296),      0); assert_x86_op("mov     $4294967296, %rax");
    si(function, 0, IR_RETURN, 0, c(1),               0); assert_x86_op("mov     $1, %rax");
    si(function, 0, IR_RETURN, 0, vsz(1, TYPE_CHAR),  0); assert_x86_op("movsbq  r1b, %rax");
    si(function, 0, IR_RETURN, 0, vsz(1, TYPE_SHORT), 0); assert_x86_op("movswq  r1w, %rax");
    si(function, 0, IR_RETURN, 0, vsz(1, TYPE_INT),   0); assert_x86_op("movslq  r1l, %rax");
    si(function, 0, IR_RETURN, 0, vsz(1, TYPE_LONG),  0); assert_x86_op("movq    r1q, %rax");

    si(function, 0, IR_RETURN, 0, gsz(1, TYPE_CHAR),  0); assert_x86_op("movsbq  g1(%rip), %rax");
    si(function, 0, IR_RETURN, 0, gsz(1, TYPE_SHORT), 0); assert_x86_op("movswq  g1(%rip), %rax");
    si(function, 0, IR_RETURN, 0, gsz(1, TYPE_INT),   0); assert_x86_op("movslq  g1(%rip), %rax");
    si(function, 0, IR_RETURN, 0, gsz(1, TYPE_LONG),  0); assert_x86_op("movq    g1(%rip), %rax");

    si(function, 0, IR_RETURN, 0, 0, 0); assert(X_RET, ir_start->operation);

    // String literal
    start_ir();
    i(0, IR_MOVE,   asz(1, TYPE_CHAR), s(1), 0);
    i(0, IR_RETURN, 0, asz(1, TYPE_CHAR), 0);
    finish_ir(function);
    assert_x86_op("leaq    .SL1(%rip), r2q");
    assert_x86_op("movq    r2q, %rax");

    // *void
    start_ir();
    // This rule will load the ADRV into memory if the ADRV is the first use
    // Delete it so the specific rule about returning a *void is tested
    nuke_rule(REGQ, 0, ADRV, 0);
    i(0, IR_RETURN, 0, asz(1, TYPE_VOID), 0);
    finish_ir(function);
    assert_x86_op("movq    r1q, %rax");
}

void test_instrsel_function_calls() {
    remove_reserved_physical_registers = 1;

    // The legacy backend extends %rax coming from a call to a quad. These
    // tests don't test much, just that the rules will work.
    si(function, 0, IR_CALL, vsz(1, TYPE_PTR + TYPE_VOID),  fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
    si(function, 0, IR_CALL, vsz(1, TYPE_CHAR),             fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
    si(function, 0, IR_CALL, vsz(1, TYPE_SHORT),            fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
    si(function, 0, IR_CALL, vsz(1, TYPE_INT),              fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
    si(function, 0, IR_CALL, vsz(1, TYPE_LONG),             fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
}

void test_instrsel_function_call_rearranging() {
    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_START_CALL, 0,    c(0),  0);
    i(0, IR_CALL,       0,    fu(1), 0);
    i(0, IR_END_CALL,   0,    c(0),  0);
    i(0, IR_MOVE,       g(1), v(1),  0);
    finish_ir(function);

    assert_tac(ir_start,                   IR_START_CALL, 0,    c(0),  0);
    assert_tac(ir_start->next,             X_CALL,        0,    fu(1), 0);
    assert_tac(ir_start->next->next,       IR_END_CALL,   0,    c(0),  0);
    assert_tac(ir_start->next->next->next, X_MOV,         g(1), v(1),  0);
}

void test_misc_commutative_operations() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_BOR, v(3), v(1), v(2));
    assert(0, strcmp(rx86op(ir_start      ), "movq    r2q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next), "orq     r1q, r3q"));

    si(function, 0, IR_BAND, v(3), v(1), v(2));
    assert(0, strcmp(rx86op(ir_start      ), "movq    r2q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next), "andq    r1q, r3q"));

    si(function, 0, IR_XOR, v(3), v(1), v(2));
    assert(0, strcmp(rx86op(ir_start      ), "movq    r2q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next), "xorq    r1q, r3q"));
}

void test_sub_operations() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_SUB, v(3), v(1), v(2));
    assert_x86_op("movq    r1q, r3q");
    assert_x86_op("subq    r2q, r3q");

    si(function, 0, IR_SUB, v(3), c(1), v(2));
    assert_x86_op("movq    $1, r2q");
    assert_x86_op("subq    r1q, r2q");

    si(function, 0, IR_SUB, v(3), v(1), c(1));
    assert_x86_op("movq    r1q, r2q");
    assert_x86_op("subq    $1, r2q");

    si(function, 0, IR_SUB, v(3), v(1), g(1));
    assert_x86_op("movq    r1q, r2q");
    assert_x86_op("subq    g1(%rip), r2q");

    si(function, 0, IR_SUB, v(3), g(1), v(1));
    assert_x86_op("movq    g1(%rip), r2q");
    assert_x86_op("subq    r1q, r2q");

    si(function, 0, IR_SUB, v(3), c(1), g(1));
    assert_x86_op("movq    $1, r1q");
    assert_x86_op("subq    g1(%rip), r1q");

    si(function, 0, IR_SUB, v(3), g(1), c(1));
    assert_x86_op("movq    g1(%rip), r1q");
    assert_x86_op("subq    $1, r1q");
}

void test_div_operations() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_DIV, v(3), v(1), v(2));
    assert(0, strcmp(rx86op(ir_start                        ), "movq    r1q, %rax"));
    assert(0, strcmp(rx86op(ir_start->next                  ), "cqto"));
    assert(0, strcmp(rx86op(ir_start->next->next            ), "movq    r2q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next->next->next      ), "idivq   r3q"));
    assert(0, strcmp(rx86op(ir_start->next->next->next->next), "movq    %rax, r3q"));

    si(function, 0, IR_MOD, v(3), v(1), v(2));
    assert(0, strcmp(rx86op(ir_start                        ), "movq    r1q, %rax"));
    assert(0, strcmp(rx86op(ir_start->next                  ), "cqto"));
    assert(0, strcmp(rx86op(ir_start->next->next            ), "movq    r2q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next->next->next      ), "idivq   r3q"));
    assert(0, strcmp(rx86op(ir_start->next->next->next->next), "movq    %rdx, r3q"));
}

void test_bnot_operations() {
    remove_reserved_physical_registers = 1;

    // Test ~v with the 4 types
    si(function, 0, IR_BNOT, vsz(3, TYPE_CHAR), vsz(1, TYPE_CHAR), 0);
    assert(0, strcmp(rx86op(ir_start      ), "movb    r1b, r2b"));
    assert(0, strcmp(rx86op(ir_start->next), "notb    r2b"));

    si(function, 0, IR_BNOT, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), 0);
    assert(0, strcmp(rx86op(ir_start      ), "movw    r1w, r2w"));
    assert(0, strcmp(rx86op(ir_start->next), "notw    r2w"));

    si(function, 0, IR_BNOT, vsz(3, TYPE_INT), vsz(1, TYPE_INT), 0);
    assert(0, strcmp(rx86op(ir_start      ), "movl    r1l, r2l"));
    assert(0, strcmp(rx86op(ir_start->next), "notl    r2l"));

    si(function, 0, IR_BNOT, v(3), v(1), 0);
    assert(0, strcmp(rx86op(ir_start      ), "movq    r1q, r2q"));
    assert(0, strcmp(rx86op(ir_start->next), "notq    r2q"));

    // ~g
    si(function, 0, IR_BNOT, v(3), g(1), 0);
    assert(0, strcmp(rx86op(ir_start      ), "movq    g1(%rip), r1q"));
    assert(0, strcmp(rx86op(ir_start->next), "notq    r1q"));

    // ~s
    si(function, 0, IR_BNOT, v(3), S(1), 0);
    assert(0, strcmp(rx86op(ir_start      ), "movq    16(%rbp), r1q"));
    assert(0, strcmp(rx86op(ir_start->next), "notq    r1q"));
}

void test_binary_shift_operations() {
    remove_reserved_physical_registers = 1;

    // v << c
    si(function, 0, IR_BSHL, v(3), v(1), c(1));
    assert(0, strcmp(rx86op(ir_start      ), "movq    r1q, r2q"));
    assert(0, strcmp(rx86op(ir_start->next), "shlq    $1, r2q"));

    // v >> c
    si(function, 0, IR_BSHR, v(3), v(1), c(1));
    assert(0, strcmp(rx86op(ir_start      ), "movq    r1q, r2q"));
    assert(0, strcmp(rx86op(ir_start->next), "sarq    $1, r2q"));

    // g << c
    si(function, 0, IR_BSHL, v(3), g(1), c(1));
    assert(0, strcmp(rx86op(ir_start      ), "movq    g1(%rip), r1q"));
    assert(0, strcmp(rx86op(ir_start->next), "shlq    $1, r1q"));

    // g >> c
    si(function, 0, IR_BSHR, v(3), g(1), c(1));
    assert(0, strcmp(rx86op(ir_start      ), "movq    g1(%rip), r1q"));
    assert(0, strcmp(rx86op(ir_start->next), "sarq    $1, r1q"));

    // vs << cb
    si(function, 0, IR_BSHL, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), vsz(2, TYPE_CHAR));
    assert(0, strcmp(rx86op(ir_start            ), "movb    r2b, %cl"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movw    r1w, r3w"));
    assert(0, strcmp(rx86op(ir_start->next->next), "shlw    %cl, r3w"));

    // vl << cb
    si(function, 0, IR_BSHL, v(3), v(1), vsz(2, TYPE_CHAR));
    assert(0, strcmp(rx86op(ir_start            ), "movb    r2b, %cl"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movq    r1q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next->next), "shlq    %cl, r3q"));

    // vl << cs
    si(function, 0, IR_BSHL, v(3), v(1), vsz(2, TYPE_SHORT));
    assert(0, strcmp(rx86op(ir_start            ), "movw    r2w, %cx"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movq    r1q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next->next), "shlq    %cl, r3q"));

    // vl << ci
    si(function, 0, IR_BSHL, v(3), v(1), vsz(2, TYPE_INT));
    assert(0, strcmp(rx86op(ir_start            ), "movl    r2l, %ecx"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movq    r1q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next->next), "shlq    %cl, r3q"));

    // vl << cl
    si(function, 0, IR_BSHL, v(3), v(1), vsz(2, TYPE_LONG));
    assert(0, strcmp(rx86op(ir_start            ), "movq    r2q, %rcx"));
    assert(0, strcmp(rx86op(ir_start->next      ), "movq    r1q, r3q"));
    assert(0, strcmp(rx86op(ir_start->next->next), "shlq    %cl, r3q"));
}

void test_constant_operations() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_BNOT, v(1), c(1), 0   ); assert_x86_op("movq    $-2, r1q");
    si(function, 0, IR_ADD,  v(1), c(1), c(2)); assert_x86_op("movq    $3, r1q" );
    si(function, 0, IR_SUB,  v(1), c(3), c(1)); assert_x86_op("movq    $2, r1q" );
    si(function, 0, IR_MUL,  v(1), c(3), c(3)); assert_x86_op("movq    $9, r1q" );
    si(function, 0, IR_DIV,  v(1), c(7), c(2)); assert_x86_op("movq    $3, r1q" );
    si(function, 0, IR_MOD,  v(1), c(7), c(2)); assert_x86_op("movq    $1, r1q" );
    si(function, 0, IR_BAND, v(1), c(3), c(1)); assert_x86_op("movq    $1, r1q" );
    si(function, 0, IR_BOR,  v(1), c(1), c(5)); assert_x86_op("movq    $5, r1q" );
    si(function, 0, IR_XOR,  v(1), c(6), c(3)); assert_x86_op("movq    $5, r1q" );
    si(function, 0, IR_EQ,   v(1), c(1), c(1)); assert_x86_op("movq    $1, r1q" );
    si(function, 0, IR_NE,   v(1), c(1), c(1)); assert_x86_op("movq    $0, r1q" );
    si(function, 0, IR_LT,   v(1), c(1), c(2)); assert_x86_op("movq    $1, r1q" );
    si(function, 0, IR_GT,   v(1), c(1), c(2)); assert_x86_op("movq    $0, r1q" );
    si(function, 0, IR_LE,   v(1), c(1), c(2)); assert_x86_op("movq    $1, r1q" );
    si(function, 0, IR_GE,   v(1), c(1), c(2)); assert_x86_op("movq    $0, r1q" );
    si(function, 0, IR_BSHL, v(1), c(1), c(2)); assert_x86_op("movq    $4, r1q" );
    si(function, 0, IR_BSHR, v(1), c(8), c(2)); assert_x86_op("movq    $2, r1q" );

    // 3 * (1 + 2)
    start_ir();
    i(0, IR_ADD, v(1), c(1), c(2));
    i(0, IR_MUL, v(2), v(1), c(3));
    finish_ir(function);
    assert_x86_op("movq    $9, r2q");
}

void _test_indirect(int src, int dst, char *template) {
    start_ir();
    i(0, IR_INDIRECT, vsz(2, dst), asz(1, src), 0);
    finish_ir(function);
    assert_x86_op(template);
}

void test_pointer_to_int_indirects() {
    remove_reserved_physical_registers = 1;

    // There aren't any rules for coercions, only same-type and promotions
    _test_indirect(TYPE_CHAR,  TYPE_CHAR,  "movb    (r1q), r2b");
    _test_indirect(TYPE_CHAR,  TYPE_SHORT, "movsbw  (r1q), r2w");
    _test_indirect(TYPE_CHAR,  TYPE_INT,   "movsbl  (r1q), r2l");
    _test_indirect(TYPE_CHAR,  TYPE_LONG,  "movsbq  (r1q), r2q");
    _test_indirect(TYPE_SHORT, TYPE_SHORT, "movw    (r1q), r2w");
    _test_indirect(TYPE_SHORT, TYPE_INT,   "movswl  (r1q), r2l");
    _test_indirect(TYPE_SHORT, TYPE_LONG,  "movswq  (r1q), r2q");
    _test_indirect(TYPE_INT,   TYPE_INT,   "movl    (r1q), r2l");
    _test_indirect(TYPE_INT,   TYPE_LONG,  "movslq  (r1q), r2q");
    _test_indirect(TYPE_LONG,  TYPE_LONG,  "movq    (r1q), r2q");
}

void test_pointer_to_pointer_to_int_indirects() {
    int src, dst;

    remove_reserved_physical_registers = 1;

    for (src = TYPE_CHAR; src <= TYPE_VOID; src++)
        for (dst = TYPE_CHAR; dst <= TYPE_VOID; dst++)
            _test_indirect(TYPE_PTR + dst,  TYPE_PTR + src,  "movq    (r1q), r2q");
}

void _test_ir_move_to_reg_lvalue(int src, int dst, char *template) {
    start_ir();
    i(0, IR_MOVE_TO_PTR, asz(2, dst), asz(2, dst), asz(1, src));
    finish_ir(function);
    assert_x86_op(template);
}

void test_ir_move_to_reg_lvalues() {
    int src, dst;

    remove_reserved_physical_registers = 1;

    for (src = TYPE_CHAR; src <= TYPE_VOID; src++)
        for (dst = TYPE_CHAR; dst <= TYPE_VOID; dst++)
            _test_ir_move_to_reg_lvalue(src, dst,  "movq    r1q, (r2q)");
}

void test_pointer_inc() {
    remove_reserved_physical_registers = 1;

    // (a1) = a1 + 1, split into a2 = a1 + 1, (a1) = a2
    start_ir();
    i(0, IR_ADD,         a(2), a(1), c(1));
    i(0, IR_MOVE_TO_PTR, a(1), a(1), a(2));
    finish_ir(function);
    assert_x86_op("movq    r1q, r4q"  );
    assert_x86_op("addq    $1, r4q"   );
    assert_x86_op("movq    r4q, (r1q)");
}

void test_pointer_add() {
    remove_reserved_physical_registers = 1;

    // a3 = a1 + a2
    si(function, 0, IR_ADD, a(3), a(1), v(2));
    assert_x86_op("movq    r1q, r3q");
    assert_x86_op("addq    r2q, r3q");

    // a3 = a1 + a2
    si(function, 0, IR_ADD, a(3), a(1), v(2));
    assert_x86_op("movq    r1q, r3q");
    assert_x86_op("addq    r2q, r3q");

    // a2 = c1 + a1
    si(function, 0, IR_ADD, a(2), c(1), a(1));
    assert_x86_op("movq    r1q, r2q");
    assert_x86_op("addq    $1, r2q");

    // a2 = a1 + c1
    si(function, 0, IR_ADD, a(2), c(1), a(1));
    assert_x86_op("movq    r1q, r2q");
    assert_x86_op("addq    $1, r2q");

    // a2 = c1 + a1
    si(function, 0, IR_ADD, asz(2, TYPE_VOID), c(1), asz(1, TYPE_VOID));
    assert_x86_op("movq    r1q, r2q");
    assert_x86_op("addq    $1, r2q");

    // a2 = a1 + c1
    si(function, 0, IR_ADD, asz(2, TYPE_VOID), asz(1, TYPE_VOID), c(1));
    assert_x86_op("movq    r1q, r2q");
    assert_x86_op("addq    $1, r2q");
}

void test_pointer_sub() {
    int i, j;

    remove_reserved_physical_registers = 1;

    // a2 = a1 - 1
    si(function, 0, IR_SUB, a(2), a(1), c(1));
    assert_x86_op("movq    r1q, r2q");
    assert_x86_op("subq    $1, r2q");

    // v3 = a1 - a2 of different types
    for (i = TYPE_VOID; i <= TYPE_LONG; i++) {
        for (j = TYPE_VOID; j <= TYPE_LONG; j++) {
            si(function, 0, IR_SUB, v(3), asz(1, i), asz(2, j));
            assert_x86_op("movq    r1q, r3q");
            assert_x86_op("subq    r2q, r3q");
        }
    }
}

void test_pointer_load_constant() {
    int t;

    remove_reserved_physical_registers = 1;

    for (t = TYPE_CHAR; t <= TYPE_LONG; t++) {
        // Need separate tests for the two constant sizes
        si(function, 0, IR_MOVE, asz(1, t),  c(1), 0);
        assert_x86_op("movq    $1, r1q");
        si(function, 0, IR_MOVE, asz(1, t),  c((long) 1 << 32), 0);
        assert_x86_op("movq    $4294967296, r1q");
    }
}

void test_pointer_eq() {
    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_EQ, v(2), a(1), c(1));
    finish_ir(function);
    assert_x86_op("cmpq    $1, r1q");
}

void test_pointer_string_literal() {
    remove_reserved_physical_registers = 1;

    // Test conversion of a string literal to an address
    start_ir();
    i(0, IR_ADD, asz(1, TYPE_CHAR), s(1), c(1));
    finish_ir(function);
    assert_x86_op("leaq    .SL1(%rip), r2q");
    assert_x86_op("movq    r2q, r1q");
    assert_x86_op("addq    $1, r1q");
}

void test_pointer_indirect_from_stack() {
    remove_reserved_physical_registers = 1;

    // Test a load from the stack followed by an indirect. It handles the
    // case of i = *pi, where pi has been forced onto the stack due to use of
    // &pi elsewhere.
    start_ir();
    i(0, IR_MOVE,     asz(4,  TYPE_INT), Ssz(-3, TYPE_INT + TYPE_PTR),  0);
    i(0, IR_INDIRECT, vsz(5,  TYPE_INT), asz(4,  TYPE_INT),             0);
    finish_spill_ir(function);
    assert_x86_op("movq    -16(%rbp), r3q");
    assert_x86_op("movl    (r3q), r2l");
}

void test_pointer_indirect_global_char_in_struct_to_long() {
    remove_reserved_physical_registers = 1;

    // Test extension of a char in a pointer to a global struct to a long
    // l = gsc->i;
    // r5:*struct sc = gsc:*struct sc
    // r6:char = *r5:*char
    // r10:long = r6:char
    // Note: not using TYPE_STRUCT, but the test is still valid.
    start_ir();
    i(0, IR_MOVE,     vsz(1, TYPE_CHAR + TYPE_PTR), gsz(1, TYPE_CHAR + TYPE_PTR), 0);
    i(0, IR_INDIRECT, vsz(3, TYPE_CHAR),            vsz(1, TYPE_CHAR + TYPE_PTR), 0);
    i(0, IR_MOVE,     vsz(4, TYPE_LONG),            vsz(3, TYPE_CHAR),            0);
    finish_ir(function);
    assert_x86_op("movq    g1(%rip), r4q");
    assert_x86_op("movsbq  (r4q), r5q");
    assert_x86_op("movq    r5q, r3q");
}

void test_pointer_to_void_arg() {
    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_MOVE, v(1), Ssz(1, TYPE_PTR + TYPE_VOID), 0);
    i(0, IR_ARG,  0, c(0), v(1));
    finish_ir(function);
    assert_x86_op("movq    16(%rbp), r2q");
    assert_x86_op("pushq   r2q"          );
}

void test_pointer_assignment_precision_decrease(int type1, int type2, char *template) {
    si(function, 0, IR_MOVE_TO_PTR, vsz(2, type1), vsz(2, type1), vsz(1, type2));
    assert_x86_op(template);
}

void test_pointer_assignment_precision_decreases() {
    int i;

    remove_reserved_physical_registers = 1;

    for (i = TYPE_CHAR; i <= TYPE_LONG; i++) {
        if (i >= TYPE_CHAR)  test_pointer_assignment_precision_decrease(TYPE_CHAR,  i, "movb    r1b, (r2q)");
        if (i >= TYPE_SHORT) test_pointer_assignment_precision_decrease(TYPE_SHORT, i, "movw    r1w, (r2q)");
        if (i >= TYPE_INT)   test_pointer_assignment_precision_decrease(TYPE_INT,   i, "movl    r1l, (r2q)");
        if (i >= TYPE_LONG)  test_pointer_assignment_precision_decrease(TYPE_LONG,  i, "movq    r1q, (r2q)");
    }
}

void test_simple_int_cast() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, vsz(2, TYPE_CHAR ), vsz(1, TYPE_CHAR ), 0); assert_x86_op("movb    r1b, r2b");
    si(function, 0, IR_MOVE, vsz(2, TYPE_CHAR ), vsz(1, TYPE_SHORT), 0); assert_x86_op("movw    r1w, r2w");
    si(function, 0, IR_MOVE, vsz(2, TYPE_CHAR ), vsz(1, TYPE_INT  ), 0); assert_x86_op("movl    r1l, r2l");
    si(function, 0, IR_MOVE, vsz(2, TYPE_CHAR ), vsz(1, TYPE_LONG ), 0); assert_x86_op("movq    r1q, r2q");

    si(function, 0, IR_MOVE, vsz(2, TYPE_SHORT), vsz(1, TYPE_CHAR),  0); assert_x86_op("movsbw  r1b, r2w");
    si(function, 0, IR_MOVE, vsz(2, TYPE_SHORT), vsz(1, TYPE_SHORT), 0); assert_x86_op("movw    r1w, r2w");
    si(function, 0, IR_MOVE, vsz(2, TYPE_SHORT), vsz(1, TYPE_INT),   0); assert_x86_op("movl    r1l, r2l");
    si(function, 0, IR_MOVE, vsz(2, TYPE_SHORT), vsz(1, TYPE_LONG),  0); assert_x86_op("movq    r1q, r2q");

    si(function, 0, IR_MOVE, vsz(2, TYPE_INT  ), vsz(1, TYPE_CHAR),  0); assert_x86_op("movsbl  r1b, r2l");
    si(function, 0, IR_MOVE, vsz(2, TYPE_INT  ), vsz(1, TYPE_SHORT), 0); assert_x86_op("movswl  r1w, r2l");
    si(function, 0, IR_MOVE, vsz(2, TYPE_INT  ), vsz(1, TYPE_INT),   0); assert_x86_op("movl    r1l, r2l");
    si(function, 0, IR_MOVE, vsz(2, TYPE_INT  ), vsz(1, TYPE_LONG),  0); assert_x86_op("movq    r1q, r2q");

    si(function, 0, IR_MOVE, vsz(2, TYPE_LONG ), vsz(1, TYPE_CHAR),  0); assert_x86_op("movsbq  r1b, r2q");
    si(function, 0, IR_MOVE, vsz(2, TYPE_LONG ), vsz(1, TYPE_SHORT), 0); assert_x86_op("movswq  r1w, r2q");
    si(function, 0, IR_MOVE, vsz(2, TYPE_LONG ), vsz(1, TYPE_INT),   0); assert_x86_op("movslq  r1l, r2q");
    si(function, 0, IR_MOVE, vsz(2, TYPE_LONG ), vsz(1, TYPE_LONG),  0); assert_x86_op("movq    r1q, r2q");
}

void test_assign_to_pointer_to_void() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), asz(1, TYPE_CHAR),  0); assert_x86_op("movq    r1q, r2q");
    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), asz(1, TYPE_SHORT), 0); assert_x86_op("movq    r1q, r2q");
    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), asz(1, TYPE_INT),   0); assert_x86_op("movq    r1q, r2q");
    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), asz(1, TYPE_LONG),  0); assert_x86_op("movq    r1q, r2q");
}

void test_pointer_comparisons() {
    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_MOVE, vsz(2, TYPE_PTR + TYPE_VOID), gsz(1, TYPE_PTR + TYPE_VOID), 0   );
    i(0, IR_EQ,   v(3),                         vsz(2, TYPE_PTR + TYPE_VOID), c(1));
    i(0, IR_JZ,   0,                            v(3),                         l(1));
    i(1, IR_NOP,  0,                            0,                            0   );
    finish_ir(function);
    assert_x86_op("movq    g1(%rip), r3q");
    assert_x86_op("cmpq    $1, r3q");
    assert_x86_op("jne     .l1");

    start_ir();
    i(0, IR_MOVE, vsz(2, TYPE_PTR + TYPE_VOID), gsz(1, TYPE_PTR + TYPE_VOID), 0   );
    i(0, IR_EQ,   v(3),                         c(1),                         vsz(2, TYPE_PTR + TYPE_VOID));
    i(0, IR_JZ,   0,                            v(3),                         l(1));
    i(1, IR_NOP,  0,                            0,                            0   );
    finish_ir(function);
    assert_x86_op("movq    g1(%rip), r3q");
    assert_x86_op("cmpq    $1, r3q");
    assert_x86_op("jne     .l1");
}

void test_pointer_double_indirect() {
    int j;

    remove_reserved_physical_registers = 1;

    for (j = TYPE_VOID; j <= TYPE_LONG; j++) {
        // Assignment to a global *... after a double indirect r4 = r1->b->c;
        start_ir();
        i(0, IR_INDIRECT, asz(2, TYPE_PTR + TYPE_VOID), asz(1, TYPE_PTR + TYPE_PTR + TYPE_VOID), 0);
        i(0, IR_INDIRECT, asz(3, TYPE_VOID),            asz(2, TYPE_PTR + TYPE_VOID),            0);
        i(0, IR_MOVE,     gsz(4, TYPE_PTR + j),         asz(3, TYPE_VOID),                       0);
        finish_ir(function);
        assert_x86_op("movq    (r1q), r4q");
        assert_x86_op("movq    (r4q), r5q");
        assert_x86_op("movq    r5q, g4(%rip)");
    }
}

void test_composite_scaled_pointer_indirect_reg(int src_size, int dst_size, int bshl_size) {
    start_ir();
    i(0, IR_BSHL,     vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(bshl_size)    ); // r3 = r1 << bshl_size
    i(0, IR_ADD,      vsz(4, TYPE_LONG), asz(2, src_size), vsz(3, TYPE_LONG)); // r4 = r2 + r3
    i(0, IR_INDIRECT, vsz(4, dst_size),  vsz(4, TYPE_LONG), 0               ); // r5 = *r4
    finish_ir(function);
}

void test_composite_offset_pointer_indirect_reg(int src_size, int dst_size) {
    start_ir();
    i(0, IR_ADD,      vsz(2, src_size), vsz(1, src_size), c(1));
    i(0, IR_INDIRECT, vsz(3, dst_size), vsz(2, src_size), 0);
    finish_ir(function);
}

void test_composite_pointer_indirect() {
    // Test mov(a,b,c), d instruction
    remove_reserved_physical_registers = 1;

    test_composite_scaled_pointer_indirect_reg(TYPE_SHORT,           TYPE_SHORT,           1); assert_x86_op("movw    (r2q,r1q,2), r5w");
    test_composite_scaled_pointer_indirect_reg(TYPE_SHORT,           TYPE_INT,             1); assert_x86_op("movswl  (r2q,r1q,2), r5l");
    test_composite_scaled_pointer_indirect_reg(TYPE_SHORT,           TYPE_LONG,            1); assert_x86_op("movswq  (r2q,r1q,2), r5q");
    test_composite_scaled_pointer_indirect_reg(TYPE_INT,             TYPE_INT,             2); assert_x86_op("movl    (r2q,r1q,4), r5l");
    test_composite_scaled_pointer_indirect_reg(TYPE_INT,             TYPE_LONG,            2); assert_x86_op("movslq  (r2q,r1q,4), r5q");
    test_composite_scaled_pointer_indirect_reg(TYPE_LONG,            TYPE_LONG,            3); assert_x86_op("movq    (r2q,r1q,8), r5q");
    test_composite_scaled_pointer_indirect_reg(TYPE_PTR + TYPE_VOID, TYPE_PTR + TYPE_VOID, 3); assert_x86_op("movq    (r2q,r1q,8), r5q");

    // Adveserial example: the BSHL doesn't match the type. This could happen
    // in e.g. p[i << 1]. In this case, the scale mov rule cannot be used.
    test_composite_scaled_pointer_indirect_reg(TYPE_SHORT, TYPE_SHORT, 2);
    assert_x86_op("movq    r1q, r6q"  );
    assert_x86_op("shlq    $2, r6q"   );
    assert_x86_op("movq    r2q, r7q"  );
    assert_x86_op("addq    r6q, r7q"  );
    assert_x86_op("movw    (r7q), r5w");

    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_CHAR,  TYPE_CHAR);             assert_x86_op("movb    1(r1q), r3b");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_CHAR,  TYPE_SHORT);            assert_x86_op("movsbw  1(r1q), r3w");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_CHAR,  TYPE_INT);              assert_x86_op("movsbl  1(r1q), r3l");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_CHAR,  TYPE_LONG);             assert_x86_op("movsbq  1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_SHORT, TYPE_SHORT);            assert_x86_op("movw    1(r1q), r3w");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_SHORT, TYPE_INT);              assert_x86_op("movswl  1(r1q), r3l");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_SHORT, TYPE_LONG);             assert_x86_op("movswq  1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_INT,   TYPE_INT);              assert_x86_op("movl    1(r1q), r3l");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_INT,   TYPE_LONG);             assert_x86_op("movslq  1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_LONG,  TYPE_LONG);             assert_x86_op("movq    1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_VOID,  TYPE_PTR + TYPE_CHAR);  assert_x86_op("movq    1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_VOID,  TYPE_PTR + TYPE_SHORT); assert_x86_op("movq    1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_VOID,  TYPE_PTR + TYPE_INT);   assert_x86_op("movq    1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_VOID,  TYPE_PTR + TYPE_LONG);  assert_x86_op("movq    1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_PTR + TYPE_VOID,  TYPE_PTR + TYPE_VOID);  assert_x86_op("movq    1(r1q), r3q");
}

void test_composite_pointer_address_of_reg(int bshl_size) {
    start_ir();
    i(0, IR_BSHL,       vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(bshl_size)     ); // r3 = r1 << bshl_size
    i(0, IR_ADD,        vsz(4, TYPE_LONG), asz(2, TYPE_VOID), vsz(3, TYPE_LONG)); // r4 = r2 + r3
    i(0, IR_ADDRESS_OF, asz(2, TYPE_VOID), vsz(4, TYPE_LONG), 0                ); // r5 = &r4
    finish_ir(function);
}


void test_composite_pointer_address_of() {
    int i;
    // Test lea(a,b,c), d instruction
    remove_reserved_physical_registers = 1;

    // Lea from a pointer lookup to different sizes stucts
    test_composite_pointer_address_of_reg(1); assert_x86_op("lea     (r2q,r1q,2), r3q");
    test_composite_pointer_address_of_reg(2); assert_x86_op("lea     (r2q,r1q,4), r3q");
    test_composite_pointer_address_of_reg(3); assert_x86_op("lea     (r2q,r1q,8), r3q");
}

void test_pointer_to_void_from_long_assignment() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), v(1), 0);
    assert_x86_op("movq    r1q, r2q");

    si(function, 0, IR_MOVE, gsz(2, TYPE_PTR + TYPE_VOID), v(1), 0);
    assert_x86_op("movq    r1q, g2(%rip)");
}

void test_ptr_to_void_memory_load_to_ptr() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, vsz(2, TYPE_PTR + TYPE_VOID), Ssz(1, TYPE_PTR + TYPE_VOID), 0);
    assert_x86_op("movq    16(%rbp), r1q");
}

void test_constant_cast_to_ptr() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, asz(1, TYPE_CHAR),  c(1), 0); assert_x86_op("movq    $1, r1q");
    si(function, 0, IR_MOVE, asz(1, TYPE_SHORT), c(1), 0); assert_x86_op("movq    $1, r1q");
    si(function, 0, IR_MOVE, asz(1, TYPE_INT),   c(1), 0); assert_x86_op("movq    $1, r1q");
    si(function, 0, IR_MOVE, asz(1, TYPE_LONG),  c(1), 0); assert_x86_op("movq    $1, r1q");
    si(function, 0, IR_MOVE, asz(1, TYPE_VOID),  c(1), 0); assert_x86_op("movq    $1, r1q");
}

void test_spilling() {
    Tac *tac;

    remove_reserved_physical_registers = 1;
    opt_enable_register_allocation = 0;

    // src1c spill
    start_ir();
    i(0, IR_MOVE, Ssz(1, TYPE_CHAR), vsz(1, TYPE_CHAR), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movb    0(%rbp), %r10b" );
    assert_rx86_preg_op("movb    %r10b, 16(%rbp)");

    // src1s spill
    start_ir();
    i(0, IR_MOVE, Ssz(1, TYPE_SHORT), vsz(1, TYPE_SHORT), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movw    0(%rbp), %r10w" );
    assert_rx86_preg_op("movw    %r10w, 16(%rbp)");

    // src1i spill
    start_ir();
    i(0, IR_MOVE, Ssz(1, TYPE_INT), vsz(1, TYPE_INT), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movl    0(%rbp), %r10d" );
    assert_rx86_preg_op("movl    %r10d, 16(%rbp)");

    // src1q spill
    start_ir();
    i(0, IR_MOVE, S(1), v(1), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movq    0(%rbp), %r10" );
    assert_rx86_preg_op("movq    %r10, 16(%rbp)");

    // src2 spill
    start_ir();
    i(0, IR_EQ, v(3), v(1), c(1));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq    -8(%rbp), %r10" );
    assert_rx86_preg_op("cmpq    $1, %r10"       );

    // src1 and src2 spill
    start_ir();
    i(0, IR_EQ, v(3), v(1), v(2));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq    -8(%rbp), %r10" );
    assert_rx86_preg_op("movq    -16(%rbp), %r11");
    assert_rx86_preg_op("cmpq    %r11, %r10"     );

    // dst spill with no instructions after
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movq    $1, %r11"     );
    assert_rx86_preg_op("movq    %r11, 0(%rbp)");

    // dst spill with an instruction after
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0);
    i(0, IR_NOP,    0,    0,    0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movq    $1, %r11"     );
    assert_rx86_preg_op("movq    %r11, 0(%rbp)");

    // v3 = v1 == v2. This primarily tests the special case for movzbq
    // where src1 == dst
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_SHORT), v(1), v(2));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq    -8(%rbp), %r10" );
    assert_rx86_preg_op("movq    -16(%rbp), %r11");
    assert_rx86_preg_op("cmpq    %r11, %r10"     );
    assert_rx86_preg_op("sete    %r11b"          );
    assert_rx86_preg_op("movw    %r11w, 0(%rbp)" );
    assert_rx86_preg_op("movw    0(%rbp), %r10w" );
    assert_rx86_preg_op("movzbw  %r10b, %r10w"   );
    assert_rx86_preg_op("movw    %r10w, %r11w"   );
    assert_rx86_preg_op("movw    %r11w, 0(%rbp)" );

    // (r2i) = 1. This tests the special case of is_lvalue_in_register=1 when
    // the type is an int.
    start_ir();
    tac = i(0, IR_MOVE,        asz(2, TYPE_INT), asz(1, TYPE_INT), 0);    tac->dst ->type = TYPE_INT; tac ->dst->is_lvalue_in_register = 1;
    tac = i(0, IR_MOVE_TO_PTR, 0,                asz(2, TYPE_INT), c(1)); tac->src1->type = TYPE_INT; tac->src1->is_lvalue_in_register = 1;
    finish_spill_ir(function);
    assert_rx86_preg_op("movq    -16(%rbp), %r10");
    assert_rx86_preg_op("movq    %r10, %r11"     );
    assert_rx86_preg_op("movq    %r11, 0(%rbp)"  );
    assert_rx86_preg_op("movq    0(%rbp), %r10"  );
    assert_rx86_preg_op("movq    %r10, %r11"     );
    assert_rx86_preg_op("movq    %r11, -8(%rbp)" );
    assert_rx86_preg_op("movq    -8(%rbp), %r10" );
    assert_rx86_preg_op("movl    $1, (%r10)"     );
}

int main() {
    failures = 0;
    function = new_function();

    opt_enable_register_allocation = 1;
    opt_optimize_arithmetic_operations = 1;
    string_literals = malloc(MAX_STRING_LITERALS);

    init_allocate_registers();
    init_instruction_selection_rules();

    test_instrsel_tree_merging();
    test_instrsel_tree_merging_type_merges();
    test_instrsel_tree_merging_register_constraint();
    test_instrsel_constant_loading();
    test_instrsel();
    test_instrsel_types_add_vregs();
    test_instrsel_types_add_mem_vreg();
    test_instrsel_types_cmp_assignment();
    test_instrsel_types_cmp_pointer();
    test_instrsel_returns();
    test_instrsel_function_calls();
    test_instrsel_function_call_rearranging();
    test_misc_commutative_operations();
    test_sub_operations();
    test_div_operations();
    test_bnot_operations();
    test_binary_shift_operations();
    test_constant_operations();
    test_pointer_to_int_indirects();
    test_pointer_to_int_indirects();
    test_pointer_to_pointer_to_int_indirects();
    test_ir_move_to_reg_lvalues();
    test_pointer_inc();
    test_pointer_add();
    test_pointer_sub();
    test_pointer_load_constant();
    test_pointer_eq();
    test_pointer_string_literal();
    test_pointer_indirect_from_stack();
    test_pointer_indirect_global_char_in_struct_to_long();
    test_pointer_to_void_arg();
    test_pointer_assignment_precision_decreases();
    test_simple_int_cast();
    test_assign_to_pointer_to_void();
    test_pointer_comparisons();
    test_pointer_double_indirect();
    test_composite_pointer_indirect();
    test_composite_pointer_address_of();
    test_pointer_to_void_from_long_assignment();
    test_ptr_to_void_memory_load_to_ptr();
    test_constant_cast_to_ptr();
    test_spilling();

    if (failures) {
        printf("%d tests failed\n", failures);
        exit(1);
    }
    printf("All tests succeeded\n");
}
