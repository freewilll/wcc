#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../wcc.h"

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

void assert_rx86_preg_op_with_function_pc(int function_param_count, char *expected) {
    char *got;

    if (!ir_start && expected) {
        printf("Expected %s, got nothing\n", expected);
        failures++;
        return;
    }
    else if (!ir_start && !expected) return;

    got = 0;
    while (ir_start && !got) {
        got = render_x86_operation(ir_start, function_param_count, 1);
        n();
    }

    if (!expected) {
        printf("Expected nothing, got %s\n", got);
        failures++;
        return;
    }

    if (got && expected && strcmp(got, expected)) {
        printf("Mismatch:\n  expected: %s\n  got:      %s\n", expected, got);
        failures++;
    }
}

void assert_rx86_preg_op(char *expected) {
    return assert_rx86_preg_op_with_function_pc(0, expected);
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
    Tac *tac;

    // Ensure type conversions are merged correctly. This tests a void * being converted to a char *
    remove_reserved_physical_registers = 1;
    start_ir();
    i(0, IR_MOVE,              asz(2, TYPE_CHAR), asz(1, TYPE_VOID), 0);
    tac = i(0, IR_MOVE_TO_PTR, 0,                 vsz(2, TYPE_CHAR), c(1));
    tac->src1->is_lvalue_in_register = 1;
    finish_ir(function);
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("movb        $1, (r3q)");
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
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("movq        r3q, r1q");
    assert_x86_op("addq        r2q, r1q");
    assert_x86_op("jmp         .L1"     );
}

void test_instrsel_tree_merging() {
    int j;
    Tac *tac;

    remove_reserved_physical_registers = 0;

    // Test edge case of a split block with variables being live on the way
    // out. This should pervent tree merging of the assigns with the first equals.
    // The label splits the block and causes v(1) and v(2) to be in block[0] liveout
    start_ir();
    i(0, IR_MOVE, v(1),             c(1), 0   );
    i(0, IR_MOVE, v(2),             c(2), 0   );
    i(0, IR_EQ,   vsz(3, TYPE_INT), v(1), v(2));
    i(1, IR_EQ,   vsz(4, TYPE_INT), v(1), v(2));
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
    i(0, IR_MOVE, v(1),             c(1), 0   );
    i(0, IR_MOVE, v(2),             c(2), 0   );
    i(1, IR_EQ,   vsz(4, TYPE_INT), v(1), v(2));
    finish_ir(function);
    assert_x86_op("movq        $1, r1q" );
    assert_x86_op("movq        $2, r2q" ); nop();
    assert_x86_op("cmpq        r2q, r1q");
    assert_x86_op("sete        r3b"     );
    assert_x86_op("movzbl      r3b, r3l");

    // Ensure a dst in assign to an lvalue keeps the value alive, so
    // that a merge is prevented later on.
    start_ir();
    i(0, IR_MOVE,        v(3),              c(1),              0   ); // r3 = 1   <- r3 is used twice, so no tree merge happens
    i(0, IR_MOVE,        v(2),              v(3),              0   ); // r2 = r3
    i(0, IR_MOVE_TO_PTR, 0,                 vsz(3, TYPE_LONG), v(4)); // (r3) = r4
    tac->src1->is_lvalue_in_register = 1;
    finish_ir(function);
    assert_x86_op("movq        $1, r2q"   );
    assert_x86_op("movq        r2q, r1q"  );
    assert_x86_op("movq        r3q, (r2q)");

    // Ensure the assign to pointer instruction copies src1 to dst first
    remove_reserved_physical_registers = 1;
    start_ir();
    i(0, IR_ADDRESS_OF,  a(1), g(1), 0);
    i(0, IR_MOVE_TO_PTR, 0,    v(1), c(1)); tac->src1->is_lvalue_in_register = 1;
    finish_ir(function);
    assert_x86_op("leaq        g1(%rip), r2q");
    assert_x86_op("movq        $1, (r2q)"    );

    // Ensure the assign to pointer instruction copies src1 to dst first
    remove_reserved_physical_registers = 1;
    start_ir();
    i(0, IR_ADDRESS_OF,  a(1), g(1), 0);
    i(0, IR_MOVE_TO_PTR, 0,    v(1), c(0x7fffffffffffffff)); tac->src1->is_lvalue_in_register = 1;
    finish_ir(function);
    assert_x86_op("leaq        g1(%rip), r2q");
    assert_x86_op("movq        $9223372036854775807, r3q");
    assert_x86_op("movq        r3q, (r2q)"    );

    // Test tree merges only happening on adjacent trees.
    // This is realistic example of a value swap of two values in memory.
    start_ir();
    i(0, IR_MOVE, v(1), g(1), 0);
    i(0, IR_MOVE, v(2), g(2), 0);
    i(0, IR_MOVE, g(1), v(2), 0);
    i(0, IR_MOVE, g(2), v(1), 0);
    finish_ir(function);
    assert_x86_op("movq        g1(%rip), r1q");
    assert_x86_op("movq        g2(%rip), r3q");
    assert_x86_op("movq        r3q, g1(%rip)");
    assert_x86_op("movq        r1q, g2(%rip)");
}

void test_int_cmp_with_conditional_jmp(Function *function, int cmp_operation, int jmp_operation, char *jmp_instruction) {
    char *template;

    start_ir();
    i(0, cmp_operation,  v(3), v(1), v(2));
    i(0, jmp_operation,  0,    v(3), l(1));
    i(1, IR_NOP,         0,    0,    0   );
    finish_ir(function);
    wasprintf(&template, "%-11s .L1", jmp_instruction);
    assert_x86_op("cmpq        r2q, r1q");
    assert_x86_op(template);
}

void test_fp_cmp_with_conditional_jmp(Function *function, int cmp_operation, int jmp_operation, char *jmp_instruction) {
    char *template;

    start_ir();
    i(0, cmp_operation,  v(3), Ssz(-1, TYPE_LONG_DOUBLE), Ssz(-2, TYPE_LONG_DOUBLE));
    i(0, jmp_operation,  0,    v(3), l(1));
    i(1, IR_NOP,         0,    0,    0   );
    finish_ir(function);

    int src1_first = cmp_operation == IR_LT || cmp_operation == IR_LE;

    if (src1_first) {
        assert_x86_op("fldt        -16(%rbp)");
        assert_x86_op("fldt        -32(%rbp)");
    }
    else {
        assert_x86_op("fldt        -32(%rbp)");
        assert_x86_op("fldt        -16(%rbp)");
    }

    assert_x86_op("fcomip      %st(1), %st");
    assert_x86_op("fstp        %st(0)");
    wasprintf(&template, "%-11s .L1", jmp_instruction);
    assert_x86_op(template);
}

void test_less_than_with_conditional_jmp(Function *function, Value *src1, Value *src2, int disable_adrq_load, char *template1, char *template2) {
    start_ir();
    if (disable_adrq_load) nuke_rule(RI4, 0, RP4, 0); // Disable direct RP4 into register passthrough
    i(0, IR_LT,  v(3), src1, src2);
    i(0, IR_JZ,  0,    v(3), l(1));
    i(1, IR_NOP, 0,    0,    0   );
    src1 = dup_value(src1);
    src2 = dup_value(src2);
    finish_ir(function);
    assert_x86_op(template1);
    assert_x86_op(template2);
    init_instruction_selection_rules();
}

void test_cmp_with_assignment(Function *function, int cmp_operation, char *set_instruction) {
    char *template;

    start_ir();
    i(0, cmp_operation, vsz(3, TYPE_INT), v(1), v(2));
    finish_ir(function);
    wasprintf(&template, "%-11s r3b", set_instruction);
    assert_x86_op("cmpq        r2q, r1q");
    assert_x86_op(template);
    assert_x86_op("movzbl      r3b, r3l");
}

void test_less_than_with_cmp_assignment(Function *function, Value *src1, Value *src2, char *t1, char *t2, char *t3, char *t4) {
    // dst is the renumbered live range that the output goes to. It's basically the first free register after src1 and src2.
    start_ir();
    i(0, IR_LT, vsz(3, TYPE_INT), src1, src2);
    src1 = dup_value(src1);
    src2 = dup_value(src2);
    finish_ir(function);
    assert_x86_op(t1);
    assert_x86_op(t2);
    assert_x86_op(t3);
    if (t4) assert_x86_op(t4);
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

    // with a 32 bit signed int
    test_cst_load(IR_MOVE, vsz(3, TYPE_CHAR),   c(1), "movb        $1, r1b");
    test_cst_load(IR_MOVE, vsz(3, TYPE_SHORT),  c(1), "movw        $1, r1w");
    test_cst_load(IR_MOVE, vsz(3, TYPE_INT),    c(1), "movl        $1, r1l");
    test_cst_load(IR_MOVE, vsz(3, TYPE_LONG),   c(1), "movq        $1, r1q");

    // with a 64 bit signed long. The first 3 are overflows, so a programmer error.
    test_cst_load(IR_MOVE, vsz(3, TYPE_CHAR),   c(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_SHORT),  c(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_INT),    c(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_LONG),   c(l), "movq        $4294967296, r2q");

    // with a 32 bit unsigned int
    test_cst_load(IR_MOVE, vusz(3, TYPE_CHAR),  uc(1), "movb        $1, r1b");
    test_cst_load(IR_MOVE, vusz(3, TYPE_SHORT), uc(1), "movw        $1, r1w");
    test_cst_load(IR_MOVE, vusz(3, TYPE_INT),   uc(1), "movl        $1, r1l");
    test_cst_load(IR_MOVE, vusz(3, TYPE_LONG),  uc(1), "movq        $1, r1q");

    // with a 64 bit unsigned long. The first 3 are overflows, so a programmer error.
    test_cst_load(IR_MOVE, vusz(3, TYPE_CHAR),  uc(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vusz(3, TYPE_SHORT), uc(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vusz(3, TYPE_INT),   uc(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vusz(3, TYPE_LONG),  uc(l), "movq        $4294967296, r2q");

    // with a 32 bit signed int -> unsigned
    test_cst_load(IR_MOVE, vusz(3, TYPE_CHAR),  c(1), "movb        $1, r1b");
    test_cst_load(IR_MOVE, vusz(3, TYPE_SHORT), c(1), "movw        $1, r1w");
    test_cst_load(IR_MOVE, vusz(3, TYPE_INT),   c(1), "movl        $1, r1l");
    test_cst_load(IR_MOVE, vusz(3, TYPE_LONG),  c(1), "movq        $1, r1q");

    // with a 64 bit signed long -> unsigned. The first 3 are overflows, so a programmer error.
    test_cst_load(IR_MOVE, vusz(3, TYPE_CHAR),  c(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vusz(3, TYPE_SHORT), c(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vusz(3, TYPE_INT),   c(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vusz(3, TYPE_LONG),  c(l), "movq        $4294967296, r2q");

    // with a 32 bit unsigned int -> signed
    test_cst_load(IR_MOVE, vsz(3, TYPE_CHAR),  uc(1), "movb        $1, r1b");
    test_cst_load(IR_MOVE, vsz(3, TYPE_SHORT), uc(1), "movw        $1, r1w");
    test_cst_load(IR_MOVE, vsz(3, TYPE_INT),   uc(1), "movl        $1, r1l");
    test_cst_load(IR_MOVE, vsz(3, TYPE_LONG),  uc(1), "movq        $1, r1q");

    // with a 64 bit unsigned long -> signed. The first 3 are overflows, so a programmer error.
    test_cst_load(IR_MOVE, vsz(3, TYPE_CHAR),  uc(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_SHORT), uc(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_INT),   uc(l), "movq        $4294967296, r2q");
    test_cst_load(IR_MOVE, vsz(3, TYPE_LONG),  uc(l), "movq        $4294967296, r2q");
}

void test_instrsel_expr() {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // Load constant into register with IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0);
    finish_ir(function);
    assert_x86_op("movq        $1, r1q");

    // r1 + r2. No loads are necessary, this is the simplest add operation.
    start_ir();
    i(0, IR_ADD, v(3), v(1), v(2));
    finish_ir(function);
    assert_x86_op("movq        r2q, r3q");
    assert_x86_op("addq        r1q, r3q");

    // r1 + S1
    start_ir();
    i(0, IR_ADD, v(2), v(1), S(-2));
    finish_ir(function);
    assert_x86_op("movq        -8(%rbp), r2q");
    assert_x86_op("addq        r1q, r2q");

    // S1 + r1
    start_ir();
    i(0, IR_ADD, v(2), v(1), S(-2));
    finish_ir(function);
    assert_x86_op("movq        -8(%rbp), r2q");
    assert_x86_op("addq        r1q, r2q");

    // r1 + g1
    start_ir();
    i(0, IR_ADD, v(2), v(1), g(1));
    finish_ir(function);
    assert_x86_op("movq        g1(%rip), r2q");
    assert_x86_op("addq        r1q, r2q");

    // g1 + r1
    start_ir();
    i(0, IR_ADD, v(2), v(1), g(1));
    finish_ir(function);
    assert_x86_op("movq        g1(%rip), r2q");
    assert_x86_op("addq        r1q, r2q");

    // c1 + g1
    start_ir();
    i(0, IR_ADD, v(2), c(1), g(1));
    finish_ir(function);
    assert_x86_op("movq        $1, r2q");
    assert_x86_op("movq        g1(%rip), r1q");
    assert_x86_op("addq        r2q, r1q");

    // g1 + c1
    start_ir();
    i(0, IR_ADD, v(2), g(1), c(1));
    finish_ir(function);
    assert_x86_op("movq        $1, r2q");
    assert_x86_op("movq        g1(%rip), r1q");
    assert_x86_op("addq        r2q, r1q");

    // Store c in g
    start_ir();
    i(0, IR_MOVE, g(1), c(1), 0);
    finish_ir(function);
    assert_x86_op("movq        $1, g1(%rip)");

    // Store c in g with only the reg fule, forcing c into r1
    start_ir();
    nuke_rule(MI4, IR_MOVE, XCI, 0);
    i(0, IR_MOVE, g(1), c(1), 0);
    finish_ir(function);
    assert_x86_op("movq        $1, g1(%rip)");
    init_instruction_selection_rules();

    // Store v1 in g using IR_MOVE
    start_ir();
    i(0, IR_MOVE, g(1), v(1), 0);
    finish_ir(function);
    assert_x86_op("movq        r1q, g1(%rip)");

    // // Store a1 in g using IR_MOVE
    // si(function, 0, IR_MOVE, p(gsz(1, TYPE_CHAR)),  asz(1, TYPE_VOID), 0); assert_x86_op("movq        r1q, g1(%rip)");
    // si(function, 0, IR_MOVE, p(gsz(1, TYPE_SHORT)), asz(1, TYPE_VOID), 0); assert_x86_op("movq        r1q, g1(%rip)");
    // si(function, 0, IR_MOVE, p(gsz(1, TYPE_INT)),   asz(1, TYPE_VOID), 0); assert_x86_op("movq        r1q, g1(%rip)");
    // si(function, 0, IR_MOVE, p(gsz(1, TYPE_LONG)),  asz(1, TYPE_VOID), 0); assert_x86_op("movq        r1q, g1(%rip)");

    // Load g into r1 using IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), g(1), 0);
    finish_ir(function);
    assert_x86_op("movq        g1(%rip), r1q");

    // Load S into r1
    start_ir();
    i(0, IR_MOVE, v(1), S(-2), 0);
    finish_ir(function);
    assert_x86_op("movq        -8(%rbp), r1q");

    // Load g into r1 using IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), g(1), 0);
    finish_ir(function);
    assert_x86_op("movq        g1(%rip), r1q");

    // Load S into r1 using IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), S(-2), 0);
    finish_ir(function);
    assert_x86_op("movq        -8(%rbp), r1q");

    // Load Si into r1l using IR_MOVE
    start_ir();
    i(0, IR_MOVE, v(1), Ssz(-1, TYPE_INT), 0);
    finish_ir(function);
    assert_x86_op("movl        -4(%rbp), r2l");
    assert_x86_op("movslq      r2l, r1q");

    // Assign constant to a local
    start_ir();
    i(0, IR_MOVE, S(-2), c(0), 0);
    finish_ir(function);
    assert_x86_op("movq        $0, -8(%rbp)");

    // Assign constant to a local. Forces c into a register
    start_ir();
    nuke_rule(MI4, IR_MOVE, XCI, 0);
    i(0, IR_MOVE, S(-2), c(0), 0);
    finish_ir(function);
    assert_x86_op("movq        $0, -8(%rbp)");
    init_instruction_selection_rules();

    // jz with r1
    start_ir();
    i(0, IR_JZ,  0,    v(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("testq       r1q, r1q");
    assert_x86_op("jz          .L1");

    // jz with a1
    start_ir();
    nuke_rule(RI4, 0, RP4, 0); // Disable direct RP4 into register passthrough
    i(0, IR_JZ,  0,    a(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("testq       r1q, r1q");
    assert_x86_op("jz          .L1");
    init_instruction_selection_rules();

    // jz with a1 *void
    start_ir();
    i(0, IR_JZ,  0,    asz(1, TYPE_VOID), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("testq       r1q, r1q");
    assert_x86_op("jz          .L1");

    // jz with function pointer in register
    start_ir();
    i(0, IR_JZ,  0,    pfu(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("testq       r1q, r1q");
    assert_x86_op("jz          .L1");

    // jz with global
    start_ir();
    i(0, IR_JZ,  0,    g(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("cmp         $0, g1(%rip)");
    assert_x86_op("jz          .L1");

    // jnz with r1
    start_ir();
    i(0, IR_JNZ, 0,    v(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("testq       r1q, r1q");
    assert_x86_op("jnz         .L1");

    // jz with a1
    start_ir();
    nuke_rule(RI4, 0, RP4, 0); // Disable direct RP4 into register passthrough
    i(0, IR_JNZ,  0,   a(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("testq       r1q, r1q");
    assert_x86_op("jnz         .L1");
    init_instruction_selection_rules();

    // jnz with a1 *void
    start_ir();
    i(0, IR_JNZ,  0,   asz(1, TYPE_VOID), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("testq       r1q, r1q");
    assert_x86_op("jnz         .L1");

    // jnz with function pointer in register
    start_ir();
    i(0, IR_JNZ, 0,    pfu(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("testq       r1q, r1q");
    assert_x86_op("jnz         .L1");

    // jnz with global
    start_ir();
    i(0, IR_JNZ, 0,    g(1), l(1));
    i(1, IR_NOP, 0,    0,    0);
    finish_ir(function);
    assert_x86_op("cmp         $0, g1(%rip)");
    assert_x86_op("jnz         .L1");
}

void test_instrsel_conditionals() {
    long l;

    l = 4294967296;
    // JZ                                                              JNZ
    test_int_cmp_with_conditional_jmp(function, IR_EQ, IR_JNZ, "je" ); test_int_cmp_with_conditional_jmp(function, IR_EQ, IR_JZ, "jne");
    test_int_cmp_with_conditional_jmp(function, IR_NE, IR_JNZ, "jne"); test_int_cmp_with_conditional_jmp(function, IR_NE, IR_JZ, "je" );
    test_int_cmp_with_conditional_jmp(function, IR_LT, IR_JNZ, "jl" ); test_int_cmp_with_conditional_jmp(function, IR_LT, IR_JZ, "jge");
    test_int_cmp_with_conditional_jmp(function, IR_GT, IR_JNZ, "jg" ); test_int_cmp_with_conditional_jmp(function, IR_GT, IR_JZ, "jle");
    test_int_cmp_with_conditional_jmp(function, IR_LE, IR_JNZ, "jle"); test_int_cmp_with_conditional_jmp(function, IR_LE, IR_JZ, "jg" );
    test_int_cmp_with_conditional_jmp(function, IR_GE, IR_JNZ, "jge"); test_int_cmp_with_conditional_jmp(function, IR_GE, IR_JZ, "jl" );

    test_fp_cmp_with_conditional_jmp(function, IR_LT, IR_JNZ, "ja" ); test_fp_cmp_with_conditional_jmp(function, IR_LT, IR_JZ, "jbe");
    test_fp_cmp_with_conditional_jmp(function, IR_GT, IR_JNZ, "ja" ); test_fp_cmp_with_conditional_jmp(function, IR_GT, IR_JZ, "jbe");
    test_fp_cmp_with_conditional_jmp(function, IR_LE, IR_JNZ, "jae"); test_fp_cmp_with_conditional_jmp(function, IR_LE, IR_JZ, "jb" );
    test_fp_cmp_with_conditional_jmp(function, IR_GE, IR_JNZ, "jae"); test_fp_cmp_with_conditional_jmp(function, IR_GE, IR_JZ, "jb" );

    // a < b with a conditional with different src1 and src2 operands
    test_less_than_with_conditional_jmp(function, v(1), c(1), 0,  "cmpq        $1, r1q",       "jge         .L1");
    test_less_than_with_conditional_jmp(function, a(1), c(1), 1,  "cmpq        $1, r1q",       "jae         .L1");
    test_less_than_with_conditional_jmp(function, g(1), c(1), 0,  "cmpq        $1, g1(%rip)",  "jge         .L1");
    test_less_than_with_conditional_jmp(function, v(1), g(1), 0,  "cmpq        g1(%rip), r1q", "jge         .L1");
    test_less_than_with_conditional_jmp(function, a(1), g(1), 1,  "cmpq        g1(%rip), r1q", "jae         .L1");
    test_less_than_with_conditional_jmp(function, g(1), v(1), 0,  "cmpq        r1q, g1(%rip)", "jge         .L1");
    test_less_than_with_conditional_jmp(function, v(1), S(-2), 0, "cmpq        -8(%rbp), r1q", "jge         .L1");
    test_less_than_with_conditional_jmp(function, a(1), S(-2), 1, "cmpq        -8(%rbp), r1q", "jae         .L1");
    test_less_than_with_conditional_jmp(function, S(-2), v(1), 0, "cmpq        r1q, -8(%rbp)", "jge         .L1");
    test_less_than_with_conditional_jmp(function, S(-2), a(1), 1, "cmpq        r1q, -8(%rbp)", "jae         .L1");
    test_less_than_with_conditional_jmp(function, S(-2), c(1), 0, "cmpq        $1, -8(%rbp)" , "jge         .L1");

    // Conditional assignment with 2 registers
    test_cmp_with_assignment(function, IR_EQ, "sete");
    test_cmp_with_assignment(function, IR_NE, "setne");
    test_cmp_with_assignment(function, IR_LT, "setl");
    test_cmp_with_assignment(function, IR_GT, "setg");
    test_cmp_with_assignment(function, IR_LE, "setle");
    test_cmp_with_assignment(function, IR_GE, "setge");

    // Test r1 = a < b with different src1 and src2 operands
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_CHAR),  c(1),               "cmpq        $1, r1q",        "setb        r2b", "movzbl      r2b, r2l", 0);
    test_less_than_with_cmp_assignment(function, v(1),               c(1),               "cmpq        $1, r1q",        "setl        r2b", "movzbl      r2b, r2l", 0);
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_VOID),  c(1),               "cmpq        $1, r1q",        "setb        r2b", "movzbl      r2b, r2l", 0);
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_CHAR),  uc(1),              "cmpq        $1, r1q",        "setb        r2b", "movzbl      r2b, r2l", 0);
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_VOID),  uc(1),              "cmpq        $1, r1q",        "setb        r2b", "movzbl      r2b, r2l", 0);
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_VOID),  asz(1, TYPE_VOID),  "cmpq        r1q, r2q",       "setb        r3b", "movzbl      r3b, r3l", 0);
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_CHAR),  asz(1, TYPE_CHAR),  "cmpq        r1q, r2q",       "setb        r3b", "movzbl      r3b, r3l", 0);
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_SHORT), asz(1, TYPE_SHORT), "cmpq        r1q, r2q",       "setb        r3b", "movzbl      r3b, r3l", 0);
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_INT),   asz(1, TYPE_INT),   "cmpq        r1q, r2q",       "setb        r3b", "movzbl      r3b, r3l", 0);
    test_less_than_with_cmp_assignment(function, asz(2, TYPE_LONG),  asz(1, TYPE_LONG),  "cmpq        r1q, r2q",       "setb        r3b", "movzbl      r3b, r3l", 0);
    test_less_than_with_cmp_assignment(function, g(1),               c(1),               "cmpq        $1, g1(%rip)",   "setl        r1b", "movzbl      r1b, r1l", 0);
    test_less_than_with_cmp_assignment(function, v(1),               g(1),               "cmpq        g1(%rip), r1q",  "setl        r2b", "movzbl      r2b, r2l", 0);
    test_less_than_with_cmp_assignment(function, g(1),               v(1),               "cmpq        r1q, g1(%rip)",  "setl        r2b", "movzbl      r2b, r2l", 0);
    test_less_than_with_cmp_assignment(function, S(-2),              c(1),               "cmpq        $1, -8(%rbp)",   "setl        r1b", "movzbl      r1b, r1l", 0);
    test_less_than_with_cmp_assignment(function, v(1),               S(-2),              "cmpq        -8(%rbp), r1q",  "setl        r2b", "movzbl      r2b, r2l", 0);
    test_less_than_with_cmp_assignment(function, S(-2),              v(1),               "cmpq        r1q, -8(%rbp)",  "setl        r2b", "movzbl      r2b, r2l", 0);

    // A 64 bit long cannot be used in a comparison directly and must be moved into
    // a register first
    test_less_than_with_cmp_assignment(function, v(1), c(l),
        "movq        $4294967296, r3q",
        "cmpq        r3q, r1q",
        "setl        r2b",
        "movzbl      r2b, r2l"
    );

    test_less_than_with_cmp_assignment(function, uv(1), uc(l),
        "movq        $4294967296, r3q",
        "cmpq        r3q, r1q",
        "setb        r2b",
        "movzbl      r2b, r2l"
    );
}

void run_function_call_single_arg(Value *src) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_ARG, 0, make_arg_src1(), src);
    tac = i(0, IR_START_CALL, 0, c(0), 0);
    tac = i(0, IR_CALL, v(1), fu(1), 0);
    tac->src1->type->function->param_count = 1;
    tac->src1->type->function->param_types = new_list(1);
    append_to_list(tac->src1->type->function->param_types, dup_type(src->type));
    tac->src1->return_value_live_ranges = new_set(LIVE_RANGE_PREG_XMM01_INDEX);
    i(0, IR_MOVE, v(2), v(1), 0);
    finish_spill_ir(function);
}

void test_function_args() {
    int j;

    remove_reserved_physical_registers = 0;

    // regular constant
    run_function_call_single_arg(c(1));
    assert_rx86_preg_op("movq        $1, %rdi" );
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // regular unsigned constant
    run_function_call_single_arg(uc(1));
    assert_rx86_preg_op("movq        $1, %rdi" );
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // quad constant
    run_function_call_single_arg(c(4294967296));
    assert_rx86_preg_op("movq        $4294967296, %rax" );
    assert_rx86_preg_op("movq        %rax, %rdi" );
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // quad unsigned constant
    run_function_call_single_arg(uc(4294967296));
    assert_rx86_preg_op("movq        $4294967296, %rax" );
    assert_rx86_preg_op("movq        %rax, %rdi" );
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // long constant
    run_function_call_single_arg(c(2147483648));
    assert_rx86_preg_op("movq        $2147483648, %rax" );
    assert_rx86_preg_op("movq        %rax, %rdi" );
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // negative long constant
    run_function_call_single_arg(c(-2147483649));
    assert_rx86_preg_op("movq        $-2147483649, %rax" );
    assert_rx86_preg_op("movq        %rax, %rdi" );
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // register
    run_function_call_single_arg(v(1));
    assert_rx86_preg_op("movq        %rax, %rdi" );
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // global
    start_ir();
    run_function_call_single_arg(g(1));
    assert_rx86_preg_op("movq        g1(%rip), %rdi");
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // local on stack
    start_ir();
    run_function_call_single_arg(S(-1));
    assert_rx86_preg_op("movq        -8(%rbp), %rdi");
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // pointer to void on stack
    start_ir();
    run_function_call_single_arg(p(Ssz(-1, TYPE_VOID)));
    assert_rx86_preg_op("movq        -8(%rbp), %rdi");
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);

    // string literal. This is a special case since the parser always loads a string
    // literal into a register first
    remove_reserved_physical_registers = 1;
    start_ir();
    i(0, IR_MOVE, asz(1, TYPE_CHAR), s(1), 0);
    i(0, IR_ARG, 0, make_arg_src1(), asz(1, TYPE_CHAR));
    i(0, IR_START_CALL, 0, c(0), 0);
    Tac *tac = i(0, IR_CALL, v(2), fu(1), 0);
    tac->src1->return_value_live_ranges = new_set(LIVE_RANGE_PREG_XMM01_INDEX);
    i(0, IR_MOVE, v(3), v(2), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("leaq        .SL1(%rip), %rax");
    assert_rx86_preg_op("movq        %rax, %rdi" );
    assert_rx86_preg_op("movq        %rax, %rax" );
    assert_rx86_preg_op(0);
}

void assert_instrsel_types_add_vregs(int dst, int src, char *mov, char *add) {
    si(function, 0, IR_ADD, vsz(3, dst), vsz(1, src), vsz(2, src));
    assert_x86_op(mov);
    assert_x86_op(add);
}

void test_instrsel_types_add_vregs() {
    int dst, src1, src2, count;
    int extend_src1, extend_src2, type;
    Tac *tac;

    // assert_instrsel_types_add_vregs(TYPE_CHAR,  TYPE_CHAR,  "movb    r2b, r3b", "addb    r1b, r3b");
    assert_instrsel_types_add_vregs(TYPE_CHAR,  TYPE_CHAR,  "movb        r2b, r3b", "addb        r1b, r3b");
    assert_instrsel_types_add_vregs(TYPE_SHORT, TYPE_CHAR,  "movb        r2b, r3b", "addb        r1b, r3b");
    assert_instrsel_types_add_vregs(TYPE_INT,   TYPE_CHAR,  "movb        r2b, r3b", "addb        r1b, r3b");
    assert_instrsel_types_add_vregs(TYPE_LONG,  TYPE_CHAR,  "movb        r2b, r3b", "addb        r1b, r3b");
    assert_instrsel_types_add_vregs(TYPE_SHORT, TYPE_SHORT, "movw        r2w, r3w", "addw        r1w, r3w");
    assert_instrsel_types_add_vregs(TYPE_INT,   TYPE_SHORT, "movw        r2w, r3w", "addw        r1w, r3w");
    assert_instrsel_types_add_vregs(TYPE_LONG,  TYPE_SHORT, "movw        r2w, r3w", "addw        r1w, r3w");
    assert_instrsel_types_add_vregs(TYPE_INT,   TYPE_INT,   "movl        r2l, r3l", "addl        r1l, r3l");
    assert_instrsel_types_add_vregs(TYPE_LONG,  TYPE_INT,   "movl        r2l, r3l", "addl        r1l, r3l");
    assert_instrsel_types_add_vregs(TYPE_LONG,  TYPE_LONG,  "movq        r2q, r3q", "addq        r1q, r3q");
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
        assert_x86_op("cmpq        $0, r1q");
        assert_x86_op("jne         .L1");

        start_ir();
        i(0, IR_EQ, v(3), asz(1, j), c(0));
        i(0, IR_JNZ, 0,    v(3), l(1));
        i(1, IR_NOP, 0,    0,    0   );
        finish_ir(function);
        assert_x86_op("cmpq        $0, r1q");
        assert_x86_op("je          .L1");
    }
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
    assert_x86_op("movq        r2q, r3q");
    assert_x86_op("orq         r1q, r3q");

    si(function, 0, IR_BAND, v(3), v(1), v(2));
    assert_x86_op("movq        r2q, r3q");
    assert_x86_op("andq        r1q, r3q");

    si(function, 0, IR_XOR, v(3), v(1), v(2));
    assert_x86_op("movq        r2q, r3q");
    assert_x86_op("xorq        r1q, r3q");
}

void test_sub_operations() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_SUB, v(3), v(1), v(2));
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("subq        r2q, r3q");

    si(function, 0, IR_SUB, v(3), c(1), v(2));
    assert_x86_op("movq        $1, r2q");
    assert_x86_op("subq        r1q, r2q");

    si(function, 0, IR_SUB, v(3), v(1), c(1));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("subq        $1, r2q");

    si(function, 0, IR_SUB, v(3), v(1), g(1));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("subq        g1(%rip), r2q");

    si(function, 0, IR_SUB, v(3), g(1), v(1));
    assert_x86_op("movq        g1(%rip), r2q");
    assert_x86_op("subq        r1q, r2q");

    si(function, 0, IR_SUB, v(3), c(1), g(1));
    assert_x86_op("movq        $1, r1q");
    assert_x86_op("subq        g1(%rip), r1q");

    si(function, 0, IR_SUB, v(3), g(1), c(1));
    assert_x86_op("movq        g1(%rip), r1q");
    assert_x86_op("subq        $1, r1q");
}

void test_div_operation(int operation, int type, Value *dst, Value *src1, Value *src2, char *t1, char *t2, char *t3, char *t4) {
    dst->type->type = type;
    src1->type->type = type;
    src2->type->type = type;

    si(function, 0, operation, dst, src1, src2);
    assert_x86_op(t1);
    assert_x86_op(t2);
    assert_x86_op(t3);
    assert_x86_op(t4);
}

void test_div_mod_operations() {
    remove_reserved_physical_registers = 1;

    test_div_operation(IR_DIV, TYPE_INT,   v(3),  v(1),  v(2), "movl        r1l, %eax", "cltd",                 "idivl       r2l", "movl        %eax, r3l");
    test_div_operation(IR_MOD, TYPE_INT,   v(3),  v(1),  v(2), "movl        r1l, %eax", "cltd",                 "idivl       r2l", "movl        %edx, r3l");
    test_div_operation(IR_DIV, TYPE_INT,  uv(3), uv(1), uv(2), "movl        r1l, %eax", "movl        $0, %edx", "divl        r2l", "movl        %eax, r3l");
    test_div_operation(IR_MOD, TYPE_INT,  uv(3), uv(1), uv(2), "movl        r1l, %eax", "movl        $0, %edx", "divl        r2l", "movl        %edx, r3l");
    test_div_operation(IR_DIV, TYPE_LONG,  v(3),  v(1),  v(2), "movq        r1q, %rax", "cqto",                 "idivq       r2q", "movq        %rax, r3q");
    test_div_operation(IR_MOD, TYPE_LONG,  v(3),  v(1),  v(2), "movq        r1q, %rax", "cqto",                 "idivq       r2q", "movq        %rdx, r3q");
    test_div_operation(IR_DIV, TYPE_LONG, uv(3), uv(1), uv(2), "movq        r1q, %rax", "movq        $0, %rdx", "divq        r2q", "movq        %rax, r3q");
    test_div_operation(IR_MOD, TYPE_LONG, uv(3), uv(1), uv(2), "movq        r1q, %rax", "movq        $0, %rdx", "divq        r2q", "movq        %rdx, r3q");
}

void test_bnot_operations() {
    remove_reserved_physical_registers = 1;

    // Test ~v with the 4 types
    si(function, 0, IR_BNOT, vsz(3, TYPE_CHAR), vsz(1, TYPE_CHAR), 0);
    assert_x86_op("movb        r1b, r2b");
    assert_x86_op("notb        r2b");

    si(function, 0, IR_BNOT, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), 0);
    assert_x86_op("movw        r1w, r2w");
    assert_x86_op("notw        r2w");

    si(function, 0, IR_BNOT, vsz(3, TYPE_INT), vsz(1, TYPE_INT), 0);
    assert_x86_op("movl        r1l, r2l");
    assert_x86_op("notl        r2l");

    si(function, 0, IR_BNOT, v(3), v(1), 0);
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("notq        r2q");

    // ~g
    si(function, 0, IR_BNOT, v(3), g(1), 0);
    assert_x86_op("movq        g1(%rip), r1q");
    assert_x86_op("notq        r1q");

    // ~s
    si(function, 0, IR_BNOT, v(3), S(-2), 0);
    assert_x86_op("movq        -8(%rbp), r1q");
    assert_x86_op("notq        r1q");
}

void test_binary_shift_operations() {
    remove_reserved_physical_registers = 1;

    // v << c
    si(function, 0, IR_BSHL, v(3), v(1), c(1));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("shlq        $1, r2q");

    // v >> c
    si(function, 0, IR_BSHR, v(3), v(1), c(1));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("shrq        $1, r2q");

    // v >> c
    si(function, 0, IR_ASHR, v(3), v(1), c(1));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("sarq        $1, r2q");

    // g << c
    si(function, 0, IR_BSHL, v(3), g(1), c(1));
    assert_x86_op("movq        g1(%rip), r1q");
    assert_x86_op("shlq        $1, r1q");

    // g >> c
    si(function, 0, IR_BSHR, v(3), g(1), c(1));
    assert_x86_op("movq        g1(%rip), r1q");
    assert_x86_op("shrq        $1, r1q");

    // g >> c
    si(function, 0, IR_ASHR, v(3), g(1), c(1));
    assert_x86_op("movq        g1(%rip), r1q");
    assert_x86_op("sarq        $1, r1q");

    // vs << cb
    si(function, 0, IR_BSHL, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), vsz(2, TYPE_CHAR));
    assert_x86_op("movb        r2b, %cl");
    assert_x86_op("movw        r1w, r3w");
    assert_x86_op("shlw        %cl, r3w");

    // vl << cb
    si(function, 0, IR_BSHL, v(3), v(1), vsz(2, TYPE_CHAR));
    assert_x86_op("movb        r2b, %cl");
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("shlq        %cl, r3q");

    // vl << cs
    si(function, 0, IR_BSHL, v(3), v(1), vsz(2, TYPE_SHORT));
    assert_x86_op("movw        r2w, %cx");
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("shlq        %cl, r3q");

    // vl << ci
    si(function, 0, IR_BSHL, v(3), v(1), vsz(2, TYPE_INT));
    assert_x86_op("movl        r2l, %ecx");
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("shlq        %cl, r3q");

    // vl << cl
    si(function, 0, IR_BSHL, v(3), v(1), vsz(2, TYPE_LONG));
    assert_x86_op("movq        r2q, %rcx");
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("shlq        %cl, r3q");
}

void test_constant_operations() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_BNOT, v(1), c(1), 0   ); assert_x86_op("movq        $-2, r1q");
    si(function, 0, IR_ADD,  v(1), c(1), c(2)); assert_x86_op("movq        $3, r1q" );
    si(function, 0, IR_SUB,  v(1), c(3), c(1)); assert_x86_op("movq        $2, r1q" );
    si(function, 0, IR_MUL,  v(1), c(3), c(3)); assert_x86_op("movq        $9, r1q" );
    si(function, 0, IR_DIV,  v(1), c(7), c(2)); assert_x86_op("movq        $3, r1q" );
    si(function, 0, IR_MOD,  v(1), c(7), c(2)); assert_x86_op("movq        $1, r1q" );
    si(function, 0, IR_BAND, v(1), c(3), c(1)); assert_x86_op("movq        $1, r1q" );
    si(function, 0, IR_BOR,  v(1), c(1), c(5)); assert_x86_op("movq        $5, r1q" );
    si(function, 0, IR_XOR,  v(1), c(6), c(3)); assert_x86_op("movq        $5, r1q" );
    si(function, 0, IR_EQ,   v(1), c(1), c(1)); assert_x86_op("movq        $1, r1q" );
    si(function, 0, IR_NE,   v(1), c(1), c(1)); assert_x86_op("movq        $0, r1q" );
    si(function, 0, IR_LT,   v(1), c(1), c(2)); assert_x86_op("movq        $1, r1q" );
    si(function, 0, IR_GT,   v(1), c(1), c(2)); assert_x86_op("movq        $0, r1q" );
    si(function, 0, IR_LE,   v(1), c(1), c(2)); assert_x86_op("movq        $1, r1q" );
    si(function, 0, IR_GE,   v(1), c(1), c(2)); assert_x86_op("movq        $0, r1q" );
    si(function, 0, IR_BSHL, v(1), c(1), c(2)); assert_x86_op("movq        $4, r1q" );
    si(function, 0, IR_BSHR, v(1), c(8), c(2)); assert_x86_op("movq        $2, r1q" );

    // 3 * (1 + 2)
    start_ir();
    i(0, IR_ADD, v(1), c(1), c(2));
    i(0, IR_MUL, v(2), v(1), c(3));
    finish_ir(function);
    assert_x86_op("movq        $9, r2q");
}

void _test_int_indirect(int type, int is_ptr, char *template) {
    Value *src = vsz(2, type);
    Value *dst = asz(1, type);

    if (is_ptr) {
        src = p(src);
        dst = p(dst);
    }

    start_ir();
    i(0, IR_INDIRECT, src, dst, 0);
    finish_ir(function);
    assert_x86_op(template);
}

void test_pointer_to_int_indirects() {
    remove_reserved_physical_registers = 1;

    _test_int_indirect(TYPE_CHAR,  0, "movb        (r1q), r2b");
    _test_int_indirect(TYPE_SHORT, 0, "movw        (r1q), r2w");
    _test_int_indirect(TYPE_INT,   0, "movl        (r1q), r2l");
    _test_int_indirect(TYPE_LONG,  0, "movq        (r1q), r2q");
}

void _test_uint_indirect(int type, int is_ptr, char *template) {
    Value *src = vusz(2, type);
    Value *dst = ausz(1, type);

    if (is_ptr) {
        src = p(src);
        dst = p(dst);
    }

    start_ir();
    i(0, IR_INDIRECT, src, dst, 0);
    finish_ir(function);
    assert_x86_op(template);
}

void test_pointer_to_uint_indirects() {
    remove_reserved_physical_registers = 1;

    _test_uint_indirect(TYPE_CHAR,  0, "movb        (r1q), r2b");
    _test_uint_indirect(TYPE_SHORT, 0, "movw        (r1q), r2w");
    _test_uint_indirect(TYPE_INT,   0, "movl        (r1q), r2l");
    _test_uint_indirect(TYPE_LONG,  0, "movq        (r1q), r2q");
}

void test_pointer_to_pointer_to_int_indirects() {
    int type;

    remove_reserved_physical_registers = 1;

    for (type = TYPE_CHAR; type <= TYPE_VOID; type++)
        _test_int_indirect(type, 1, "movq        (r1q), r2q");
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
            _test_ir_move_to_reg_lvalue(src, dst,  "movq        r1q, (r2q)");
}

void test_pointer_inc() {
    remove_reserved_physical_registers = 1;

    // (a1) = a1 + 1, split into a2 = a1 + 1, (a1) = a2
    start_ir();
    i(0, IR_ADD,         a(2), a(1), c(1));
    i(0, IR_MOVE_TO_PTR, a(1), a(1), a(2));
    finish_ir(function);
    assert_x86_op("movq        r1q, r4q"  );
    assert_x86_op("addq        $1, r4q"   );
    assert_x86_op("movq        r4q, (r1q)");
}

void test_pointer_add() {
    remove_reserved_physical_registers = 1;

    // a3 = a1 + v2
    si(function, 0, IR_ADD, a(3), a(1), v(2));
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("addq        r2q, r3q");

    si(function, 0, IR_ADD, a(3), a(1), vsz(2, TYPE_CHAR));
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("movsbq      r2b, r2q");
    assert_x86_op("addq        r2q, r3q");

    si(function, 0, IR_ADD, a(3), a(1), vsz(2, TYPE_SHORT));
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("movswq      r2w, r2q");
    assert_x86_op("addq        r2q, r3q");

    si(function, 0, IR_ADD, a(3), a(1), vsz(2, TYPE_INT));
    assert_x86_op("movq        r1q, r3q");
    assert_x86_op("movslq      r2l, r2q");
    assert_x86_op("addq        r2q, r3q");

    // // a2 = c1 + a1
    // si(function, 0, IR_ADD, a(2), c(1), a(1));
    // assert_x86_op("movq    r1q, r2q");
    // assert_x86_op("addq    $1, r2q");

    // a2 = a1 + c1
    si(function, 0, IR_ADD, a(2), a(1), c(1));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("addq        $1, r2q");

    // a2 = c1 + a1
    si(function, 0, IR_ADD, asz(2, TYPE_VOID), c(1), asz(1, TYPE_VOID));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("addq        $1, r2q");

    // a2 = a1 + c1
    si(function, 0, IR_ADD, asz(2, TYPE_VOID), asz(1, TYPE_VOID), c(1));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("addq        $1, r2q");
}

void test_pointer_sub() {
    int i, j;

    remove_reserved_physical_registers = 1;

    // a2 = a1 - 1
    si(function, 0, IR_SUB, a(2), a(1), c(1));
    assert_x86_op("movq        r1q, r2q");
    assert_x86_op("subq        $1, r2q");

    // v3 = a1 - a2 of different types
    for (i = TYPE_VOID; i <= TYPE_LONG; i++) {
        for (j = TYPE_VOID; j <= TYPE_LONG; j++) {
            si(function, 0, IR_SUB, v(3), asz(1, i), asz(2, j));
            assert_x86_op("movq        r1q, r3q");
            assert_x86_op("subq        r2q, r3q");
        }
    }
}

void test_pointer_load_constant() {
    int t;

    remove_reserved_physical_registers = 1;

    for (t = TYPE_CHAR; t <= TYPE_LONG; t++) {
        // Need separate tests for the two constant sizes
        si(function, 0, IR_MOVE, asz(1, t),  c(1), 0);
        assert_x86_op("movq        $1, r1q");
        si(function, 0, IR_MOVE, asz(1, t),  c((long) 1 << 32), 0);
        assert_x86_op("movq        $4294967296, r1q");
    }
}

void test_pointer_eq() {
    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_EQ, vsz(2, TYPE_INT), a(1), c(1));
    finish_ir(function);
    assert_x86_op("cmpq        $1, r1q");
}

void test_pointer_string_literal() {
    remove_reserved_physical_registers = 1;

    // Test conversion of a string literal to an address
    start_ir();
    i(0, IR_ADD, asz(1, TYPE_CHAR), s(1), c(1));
    finish_ir(function);
    assert_x86_op("leaq        .SL1(%rip), r2q");
    assert_x86_op("movq        r2q, r1q");
    assert_x86_op("addq        $1, r1q");
}

void test_pointer_indirect_from_stack() {
    remove_reserved_physical_registers = 1;

    // Test a load from the stack followed by an indirect. It handles the
    // case of i = *pi, where pi has been forced onto the stack due to use of
    // &pi elsewhere.
    start_ir();
    i(0, IR_MOVE,     asz(4,  TYPE_INT), p(Ssz(-3, TYPE_INT)), 0);
    i(0, IR_INDIRECT, vsz(5,  TYPE_INT),   asz(4,  TYPE_INT),  0);
    finish_spill_ir(function);
    assert_x86_op("movq        -8(%rbp), r3q");
    assert_x86_op("movl        (r3q), r2l");
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
    i(0, IR_MOVE,     asz(1, TYPE_CHAR), p(gsz(1, TYPE_CHAR)), 0);
    i(0, IR_INDIRECT, vsz(3, TYPE_CHAR),   asz(1, TYPE_CHAR),  0);
    i(0, IR_MOVE,     vsz(4, TYPE_LONG),   vsz(3, TYPE_CHAR),  0);
    finish_ir(function);
    assert_x86_op("movq        g1(%rip), r4q");
    assert_x86_op("movb        (r4q), r5b");
    assert_x86_op("movsbq      r5b, r3q");
}

void test_pointer_assignment_precision_decrease(int type1, int type2, char *template) {
    Tac *tac;

    tac = si(function, 0, IR_MOVE_TO_PTR, 0, vsz(2, type1), vsz(1, type2));
    assert_x86_op(template);
}

void test_pointer_assignment_precision_decreases() {
    int i;

    remove_reserved_physical_registers = 1;

    for (i = TYPE_CHAR; i <= TYPE_LONG; i++) {
        if (i >= TYPE_CHAR)  test_pointer_assignment_precision_decrease(TYPE_CHAR,  i, "movb        r1b, (r2q)");
        if (i >= TYPE_SHORT) test_pointer_assignment_precision_decrease(TYPE_SHORT, i, "movw        r1w, (r2q)");
        if (i >= TYPE_INT)   test_pointer_assignment_precision_decrease(TYPE_INT,   i, "movl        r1l, (r2q)");
        if (i >= TYPE_LONG)  test_pointer_assignment_precision_decrease(TYPE_LONG,  i, "movq        r1q, (r2q)");
    }
}

void _test_integer_move(Value *dst, Value *src, int dst_type, int src_type, char *template) {
    dst->type->type = dst_type ;
    src->type->type = src_type;
    si(function, 0, IR_MOVE, dup_value(dst), dup_value(src), 0);
    assert_x86_op(template);
}

void test_integer_moves() {
    Value *dst, *src;
    int i;

    // Test signed -> signed and signed -> unsigned moves
    src = vsz(1, TYPE_CHAR);
    for (i = 0; i < 2; i++) {
        dst = i ? vusz(2, TYPE_CHAR) : vsz(2, TYPE_CHAR);
        _test_integer_move(dst, src, TYPE_CHAR,  TYPE_CHAR,  "movb        r1b, r2b");
        _test_integer_move(dst, src, TYPE_CHAR,  TYPE_SHORT, "movb        r1b, r2b");
        _test_integer_move(dst, src, TYPE_CHAR,  TYPE_INT,   "movb        r1b, r2b");
        _test_integer_move(dst, src, TYPE_CHAR,  TYPE_LONG,  "movb        r1b, r2b");
        _test_integer_move(dst, src, TYPE_SHORT, TYPE_CHAR,  "movsbw      r1b, r2w");
        _test_integer_move(dst, src, TYPE_SHORT, TYPE_SHORT, "movw        r1w, r2w");
        _test_integer_move(dst, src, TYPE_SHORT, TYPE_INT,   "movw        r1w, r2w");
        _test_integer_move(dst, src, TYPE_SHORT, TYPE_LONG,  "movw        r1w, r2w");
        _test_integer_move(dst, src, TYPE_INT,   TYPE_CHAR,  "movsbl      r1b, r2l");
        _test_integer_move(dst, src, TYPE_INT,   TYPE_SHORT, "movswl      r1w, r2l");
        _test_integer_move(dst, src, TYPE_INT,   TYPE_INT,   "movl        r1l, r2l");
        _test_integer_move(dst, src, TYPE_INT,   TYPE_LONG,  "movl        r1l, r2l");
        _test_integer_move(dst, src, TYPE_LONG,  TYPE_CHAR,  "movsbq      r1b, r2q");
        _test_integer_move(dst, src, TYPE_LONG,  TYPE_SHORT, "movswq      r1w, r2q");
        _test_integer_move(dst, src, TYPE_LONG,  TYPE_INT,   "movslq      r1l, r2q");
        _test_integer_move(dst, src, TYPE_LONG,  TYPE_LONG,  "movq        r1q, r2q");
    }

    // Test unsigned -> signed and unsigned -> unsigned moves
    src = vusz(1, TYPE_CHAR);
    for (i = 0; i < 2; i++) {
        dst = i ? vusz(2, TYPE_CHAR) : vsz(2, TYPE_CHAR);
        _test_integer_move(dst, src, TYPE_CHAR,  TYPE_CHAR,  "movb        r1b, r2b");
        _test_integer_move(dst, src, TYPE_CHAR,  TYPE_SHORT, "movb        r1b, r2b");
        _test_integer_move(dst, src, TYPE_CHAR,  TYPE_INT,   "movb        r1b, r2b");
        _test_integer_move(dst, src, TYPE_CHAR,  TYPE_LONG,  "movb        r1b, r2b");
        _test_integer_move(dst, src, TYPE_SHORT, TYPE_CHAR,  "movzbw      r1b, r2w");
        _test_integer_move(dst, src, TYPE_SHORT, TYPE_SHORT, "movw        r1w, r2w");
        _test_integer_move(dst, src, TYPE_SHORT, TYPE_INT,   "movw        r1w, r2w");
        _test_integer_move(dst, src, TYPE_SHORT, TYPE_LONG,  "movw        r1w, r2w");
        _test_integer_move(dst, src, TYPE_INT,   TYPE_CHAR,  "movzbl      r1b, r2l");
        _test_integer_move(dst, src, TYPE_INT,   TYPE_SHORT, "movzwl      r1w, r2l");
        _test_integer_move(dst, src, TYPE_INT,   TYPE_INT,   "movl        r1l, r2l");
        _test_integer_move(dst, src, TYPE_INT,   TYPE_LONG,  "movl        r1l, r2l");
        _test_integer_move(dst, src, TYPE_LONG,  TYPE_CHAR,  "movzbq      r1b, r2q");
        _test_integer_move(dst, src, TYPE_LONG,  TYPE_SHORT, "movzwq      r1w, r2q");
        _test_integer_move(dst, src, TYPE_LONG,  TYPE_INT,   "movl        r1l, r2l"); // movzbl doesn't exist
        _test_integer_move(dst, src, TYPE_LONG,  TYPE_LONG,  "movq        r1q, r2q");
    }
}

void test_assign_to_pointer_to_void() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), asz(1, TYPE_CHAR),  0); assert_x86_op("movq        r1q, r2q");
    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), asz(1, TYPE_SHORT), 0); assert_x86_op("movq        r1q, r2q");
    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), asz(1, TYPE_INT),   0); assert_x86_op("movq        r1q, r2q");
    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), asz(1, TYPE_LONG),  0); assert_x86_op("movq        r1q, r2q");
}

void test_pointer_comparisons() {
    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_MOVE, asz(2, TYPE_VOID), p(gsz(1, TYPE_VOID)), 0   );
    i(0, IR_EQ,   v(3),                asz(2, TYPE_VOID),  c(1));
    i(0, IR_JZ,   0,                 v(3),                 l(1));
    i(1, IR_NOP,  0,                 0,                    0   );
    finish_ir(function);
    assert_x86_op("cmpq        $1, g1(%rip)");
    assert_x86_op("jne         .L1");

    start_ir();
    i(0, IR_MOVE, asz(2, TYPE_VOID), p(gsz(1, TYPE_VOID)), 0    );
    i(0, IR_EQ,   v(3),                asz(2, TYPE_VOID),  uc(1));
    i(0, IR_JZ,   0,                 v(3),                 l(1) );
    i(1, IR_NOP,  0,                 0,                    0    );
    finish_ir(function);
    assert_x86_op("cmpq        $1, g1(%rip)");
    assert_x86_op("jne         .L1");

    start_ir();
    i(0, IR_MOVE, asz(2, TYPE_VOID), p(gsz(1, TYPE_VOID)), 0   );
    i(0, IR_EQ,   v(3),              c(1),                 asz(2, TYPE_VOID));
    i(0, IR_JZ,   0,                 v(3),                 l(1));
    i(1, IR_NOP,  0,                 0,                    0   );
    finish_ir(function);
    assert_x86_op("movq        $1, r3q");
    assert_x86_op("cmpq        g1(%rip), r3q");
    assert_x86_op("jne         .L1");

    start_ir();
    i(0, IR_MOVE, asz(2, TYPE_VOID), p(gsz(1, TYPE_VOID)), 0   );
    i(0, IR_EQ,   v(3),              uc(1),                asz(2, TYPE_VOID));
    i(0, IR_JZ,   0,                 v(3),                 l(1));
    i(1, IR_NOP,  0,                 0,                    0   );
    finish_ir(function);
    assert_x86_op("movq        $1, r3q");
    assert_x86_op("cmpq        g1(%rip), r3q");
    assert_x86_op("jne         .L1");
}

void test_pointer_double_indirect() {
    int j;

    remove_reserved_physical_registers = 1;

    for (j = TYPE_VOID; j <= TYPE_LONG; j++) {
        // Assignment to a global *... after a double indirect r4 = r1->b->c;
        start_ir();
        i(0, IR_INDIRECT, p(asz(2, TYPE_VOID)), p(p(asz(1, TYPE_VOID))), 0);
        i(0, IR_INDIRECT,   asz(3, TYPE_VOID),  p(  asz(2, TYPE_VOID)),  0);
        i(0, IR_MOVE,     p(gsz(4, j)),             asz(3, TYPE_VOID),   0);
        finish_ir(function);
        assert_x86_op("movq        (r1q), r4q");
        assert_x86_op("movq        (r4q), r5q");
        assert_x86_op("movq        r5q, g4(%rip)");
    }
}

void test_composite_scaled_pointer_indirect_reg(int src_size, int src_ptr, int dst_size, int dst_ptr, int bshl_size) {
    Value *src  = asz( 2, src_size);
    Value *dsti = vsz( 4, dst_size);
    Value *dstu = vusz(4, dst_size);

    if (src_ptr) src = p(src);
    if (dst_ptr) dsti = p(dsti);
    if (dst_ptr) dstu = p(dstu);

    // signed
    start_ir();
    i(0, IR_BSHL,     vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(bshl_size)    );  // r3 = r1 << bshl_size
    i(0, IR_ADD,      vsz(4, TYPE_LONG), dup_value(src),    vsz(3, TYPE_LONG)); // r4 = r2 + r3
    i(0, IR_INDIRECT, dsti,              vsz(4, TYPE_LONG), 0               );  // r5 = *r4
    finish_ir(function);

    // unsigned
    start_ir();
    i(0, IR_BSHL,     vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(bshl_size)    );  // r3 = r1 << bshl_size
    i(0, IR_ADD,      vsz(4, TYPE_LONG), dup_value(src),    vsz(3, TYPE_LONG)); // r4 = r2 + r3
    i(0, IR_INDIRECT, dstu,              vsz(4, TYPE_LONG), 0               );  // r5 = *r4
    finish_ir(function);
}

void test_composite_offset_pointer_indirect_reg(int src_size, int dst_size, int dst_ptr) {
    Value *dsti = vsz( 3, dst_size);
    Value *dstu = vusz(3, dst_size);

    if (dst_ptr) dsti = p(dsti);
    if (dst_ptr) dstu = p(dstu);

    // signed

    start_ir();
    i(0, IR_ADD,      asz(2, src_size), asz(1, src_size), c(1));
    i(0, IR_INDIRECT, dsti            , asz(2, src_size), 0);
    finish_ir(function);

    // unsigned
    start_ir();
    i(0, IR_ADD,      ausz(2, src_size), ausz(1, src_size), c(1));
    i(0, IR_INDIRECT, dstu             , ausz(2, src_size), 0);
    finish_ir(function);
}

void test_composite_pointer_indirect() {
    // Test mov(a,b,c), d instruction
    remove_reserved_physical_registers = 1;

    test_composite_scaled_pointer_indirect_reg(TYPE_SHORT, 0, TYPE_SHORT, 0, 1); assert_x86_op("movw        (r2q,r1q,2), r5w");
    test_composite_scaled_pointer_indirect_reg(TYPE_INT,   0, TYPE_INT,   0, 2); assert_x86_op("movl        (r2q,r1q,4), r5l");
    test_composite_scaled_pointer_indirect_reg(TYPE_LONG,  0, TYPE_LONG,  0, 3); assert_x86_op("movq        (r2q,r1q,8), r5q");
    test_composite_scaled_pointer_indirect_reg(TYPE_VOID,  1, TYPE_VOID,  1, 3); assert_x86_op("movq        (r2q,r1q,8), r5q");

    // Test mov a(b), c instruction
    test_composite_offset_pointer_indirect_reg(TYPE_CHAR,   TYPE_CHAR,   0); assert_x86_op("movb        1(r1q), r3b");
    test_composite_offset_pointer_indirect_reg(TYPE_SHORT,  TYPE_SHORT,  0); assert_x86_op("movw        1(r1q), r3w");
    test_composite_offset_pointer_indirect_reg(TYPE_INT,    TYPE_INT,    0); assert_x86_op("movl        1(r1q), r3l");
    test_composite_offset_pointer_indirect_reg(TYPE_LONG,   TYPE_LONG,   0); assert_x86_op("movq        1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_FLOAT,  TYPE_FLOAT,  0); assert_x86_op("movss       1(r1q), r3l");
    test_composite_offset_pointer_indirect_reg(TYPE_DOUBLE, TYPE_DOUBLE, 0); assert_x86_op("movsd       1(r1q), r3l");
    test_composite_offset_pointer_indirect_reg(TYPE_VOID,   TYPE_CHAR,   1); assert_x86_op("movq        1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_VOID,   TYPE_SHORT,  1); assert_x86_op("movq        1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_VOID,   TYPE_INT,    1); assert_x86_op("movq        1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_VOID,   TYPE_LONG,   1); assert_x86_op("movq        1(r1q), r3q");
    test_composite_offset_pointer_indirect_reg(TYPE_VOID,   TYPE_VOID,   1); assert_x86_op("movq        1(r1q), r3q");
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
    test_composite_pointer_address_of_reg(1); assert_x86_op("lea         (r2q,r1q,2), r3q");
    test_composite_pointer_address_of_reg(2); assert_x86_op("lea         (r2q,r1q,4), r3q");
    test_composite_pointer_address_of_reg(3); assert_x86_op("lea         (r2q,r1q,8), r3q");
}

void test_pointer_to_void_from_long_assignment() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), v(1), 0);
    assert_x86_op("movq        r1q, r2q");

    si(function, 0, IR_MOVE, p(gsz(2, TYPE_VOID)), v(1), 0);
    assert_x86_op("movq        r1q, g2(%rip)");

    si(function, 0, IR_MOVE, p(gsz(2, TYPE_VOID)), vusz(1, TYPE_LONG), 0);
    assert_x86_op("movq        r1q, g2(%rip)");
}

void test_ptr_to_void_memory_load_to_ptr() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, asz(2, TYPE_VOID), p(Ssz(-2, TYPE_VOID)), 0);
    assert_x86_op("movq        -8(%rbp), r1q");
}

void test_constant_cast_to_ptr() {
    remove_reserved_physical_registers = 1;

    si(function, 0, IR_MOVE, asz(1, TYPE_CHAR),  c(1), 0); assert_x86_op("movq        $1, r1q");
    si(function, 0, IR_MOVE, asz(1, TYPE_SHORT), c(1), 0); assert_x86_op("movq        $1, r1q");
    si(function, 0, IR_MOVE, asz(1, TYPE_INT),   c(1), 0); assert_x86_op("movq        $1, r1q");
    si(function, 0, IR_MOVE, asz(1, TYPE_LONG),  c(1), 0); assert_x86_op("movq        $1, r1q");
    si(function, 0, IR_MOVE, asz(1, TYPE_VOID),  c(1), 0); assert_x86_op("movq        $1, r1q");
}

void test_spilling() {
    Tac *tac;

    remove_reserved_physical_registers = 1;
    live_range_reserved_pregs_offset = 0; // Disable register allocation

    // src1c spill
    start_ir();
    i(0, IR_MOVE, Ssz(-2, TYPE_CHAR), vsz(1, TYPE_CHAR), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movb        -1(%rbp), %r10b");
    assert_rx86_preg_op("movb        %r10b, -2(%rbp)");

    // src1s spill
    start_ir();
    i(0, IR_MOVE, Ssz(-2, TYPE_SHORT), vsz(1, TYPE_SHORT), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movw        -2(%rbp), %r10w");
    assert_rx86_preg_op("movw        %r10w, -4(%rbp)");

    // src1i spill
    start_ir();
    i(0, IR_MOVE, Ssz(-2, TYPE_INT), vsz(1, TYPE_INT), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movl        -4(%rbp), %r10d");
    assert_rx86_preg_op("movl        %r10d, -8(%rbp)");

    // src1q spill
    start_ir();
    i(0, IR_MOVE, S(-2), v(1), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        -8(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, -16(%rbp)");

    // src1c spill with offset
    Value *offsetted_value = asz(1, TYPE_INT);
    offsetted_value->offset = 64;
    start_ir();
    i(0, IR_MOVE, Ssz(-2, TYPE_LONG), offsetted_value, 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        -8(%rbp), %r10");
    assert_rx86_preg_op("addq        $64, %r10");
    assert_rx86_preg_op("movq        %r10, -16(%rbp)");

    // src2 spill
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_INT), v(1), c(1));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        -8(%rbp), %r10");
    assert_rx86_preg_op("cmpq        $1, %r10"       );

    // src1 and src2 spill
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_INT), v(1), v(2));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        -8(%rbp), %r10");
    assert_rx86_preg_op("movq        -16(%rbp), %r11");
    assert_rx86_preg_op("cmpq        %r11, %r10"     );

    // dst spill with no instructions after
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        $1, %r11"     );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)");

    // dst spill with an instruction after
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0);
    i(0, IR_NOP,    0,    0,    0);
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        $1, %r11"     );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)");

    // v3 = v1 == v2. This primarily tests the special case for movzbq
    // where src1 == dst
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_INT), v(1), v(2));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        -8(%rbp), %r10" );
    assert_rx86_preg_op("movq        -16(%rbp), %r11"  );
    assert_rx86_preg_op("cmpq        %r11, %r10"      );
    assert_rx86_preg_op("sete        %r11b"           );
    assert_rx86_preg_op("movl        %r11d, -20(%rbp)");
    assert_rx86_preg_op("movl        -20(%rbp), %r10d");
    assert_rx86_preg_op("movzbl      %r10b, %r10d"    );
    assert_rx86_preg_op("movl        %r10d, %r11d"    );
    assert_rx86_preg_op("movl        %r11d, -20(%rbp)");

    // (r2i) = 1. This tests the special case of is_lvalue_in_register=1 when
    // the type is an int.
    start_ir();
    tac = i(0, IR_MOVE,        asz(2, TYPE_INT), asz(1, TYPE_INT), 0);    tac->dst ->type = new_type(TYPE_INT); tac ->dst->is_lvalue_in_register = 1;
    tac = i(0, IR_MOVE_TO_PTR, 0,                asz(2, TYPE_INT), c(1)); tac->src1->type = new_type(TYPE_INT); tac->src1->is_lvalue_in_register = 1;
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        -16(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %r11"     );
    assert_rx86_preg_op("movq        %r11, -20(%rbp)");
    assert_rx86_preg_op("movq        -20(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %r11"     );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );
    assert_rx86_preg_op("movq        -8(%rbp), %r10" );
    assert_rx86_preg_op("movl        $1, (%r10)"     );

    // BSHL with dst and src2 in registers
    start_ir();
    i(0, IR_BSHL, v(1), c(1), v(2));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        $1, %r11"       );
    assert_rx86_preg_op("movq        %r11, -16(%rbp)");
    assert_rx86_preg_op("movq        -24(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %rcx"     );
    assert_rx86_preg_op("movq        -16(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %r11"     );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );
    assert_rx86_preg_op("movq        -8(%rbp), %r11" );
    assert_rx86_preg_op("shlq        %cl, %r11"      );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );

    // BSHR with dst and src2 in registers
    start_ir();
    i(0, IR_BSHR, v(1), c(1), v(2));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        $1, %r11"       );
    assert_rx86_preg_op("movq        %r11, -16(%rbp)");
    assert_rx86_preg_op("movq        -24(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %rcx"     );
    assert_rx86_preg_op("movq        -16(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %r11"     );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );
    assert_rx86_preg_op("movq        -8(%rbp), %r11" );
    assert_rx86_preg_op("shrq        %cl, %r11"      );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );

    // ASHR with dst and src2 in registers
    start_ir();
    i(0, IR_ASHR, v(1), c(1), v(2));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        $1, %r11"       );
    assert_rx86_preg_op("movq        %r11, -16(%rbp)");
    assert_rx86_preg_op("movq        -24(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %rcx"     );
    assert_rx86_preg_op("movq        -16(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %r11"     );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );
    assert_rx86_preg_op("movq        -8(%rbp), %r11" );
    assert_rx86_preg_op("sarq        %cl, %r11"      );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );

    // BSHL with dst and src1 in registers
    start_ir();
    i(0, IR_BSHL, v(1), v(2), c(1));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        -16(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %r11"     );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );
    assert_rx86_preg_op("movq        -8(%rbp), %r11" );
    assert_rx86_preg_op("shlq        $1, %r11"       );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );

    // BSHR with dst and src1 in registers
    start_ir();
    i(0, IR_BSHR, v(1), v(2), c(1));
    finish_spill_ir(function);
    assert_rx86_preg_op("movq        -16(%rbp), %r10");
    assert_rx86_preg_op("movq        %r10, %r11"     );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );
    assert_rx86_preg_op("movq        -8(%rbp), %r11" );
    assert_rx86_preg_op("shrq        $1, %r11"       );
    assert_rx86_preg_op("movq        %r11, -8(%rbp)" );

    init_allocate_registers(); // Enable register allocation again
}

// This is a quite convoluted test that checks code generation for a function with
// a bunch of args. IR_ADD instructions are added so that all args interfere in the
// interference graph
void test_param_vreg_moves() {
    int j;
    Tac *tac;

    remove_reserved_physical_registers = 0;

    start_ir();

    function->param_count = 10;
    function->param_types = new_list(1);
    for (j = 0; j < function->param_count; j++)
        append_to_list(function->param_types, new_type(TYPE_CHAR));

    i(0, IR_NOP, 0, 0, 0);

    for (j = 0; j < function->param_count; j++)
        tac = i(0, IR_ADD, v(j + 3), v(1), S(j + 2));

    finish_spill_ir(function);

    // The last 4 args are pushed backwards by the caller (i.e. last arg first)
    // arg 0 rdi
    // arg 1 rsi
    // arg 2 rdx
    // arg 3 rcx
    // arg 4 r8
    // arg 5 r9
    // arg 6 stack index 5 16(%rbp)
    // arg 7 stack index 4 24(%rbp)
    // arg 8 stack index 3 32(%rbp)
    // arg 9 stack index 2 40(%rbp)

    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        %dil, %bl");       // arg 0    the first 6 args are already in registers
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        %sil, %dil");      // arg 1
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        %dl, %sil");       // arg 2
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        %cl, %dl");        // arg 3
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        %r8b, %cl");       // arg 4
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        %r9b, %r8b");      // arg 5
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        16(%rbp), %r9b");  // arg 6    pushed args get loaded into registers
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        24(%rbp), %r12b"); // arg 7
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        32(%rbp), %r13b"); // arg 8
    assert_rx86_preg_op_with_function_pc(function->param_count, "movb        40(%rbp), %r14b"); // arg 9

    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %rbx, %rbx");      // arg 0    addition code starts here
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %rdi, %rbx");      // arg 1
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %rsi, %rbx");      // arg 2
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %rdx, %rbx");      // arg 3
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %rcx, %rbx");      // arg 4
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %r8, %rbx");       // arg 5
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %r9, %rbx");       // arg 6
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %r12, %rbx");      // arg 7
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %r13, %rbx");      // arg 8
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
    assert_rx86_preg_op_with_function_pc(function->param_count, "movq        %r14, %rbx");      // arg 9
    assert_rx86_preg_op_with_function_pc(function->param_count, "addq        %rax, %rbx");
}

int main() {
    int verbose;

    verbose = 0;
    failures = 0;

    init_memory_management_for_translation_unit();

    function = new_function();

    opt_optimize_arithmetic_operations = 1;
    string_literals = malloc(MAX_STRING_LITERALS);

    init_allocate_registers();
    init_instruction_selection_rules();

    // Disable inefficient rules check that only needs running once
    disable_check_for_duplicate_rules = 1;

    if (verbose) printf("Running instrsel instrsel_tree_merging\n");                          test_instrsel_tree_merging();
    if (verbose) printf("Running instrsel instrsel_tree_merging_type_merges\n");              test_instrsel_tree_merging_type_merges();
    if (verbose) printf("Running instrsel instrsel_tree_merging_register_constraint\n");      test_instrsel_tree_merging_register_constraint();
    if (verbose) printf("Running instrsel instrsel_constant_loading\n");                      test_instrsel_constant_loading();
    if (verbose) printf("Running instrsel instrsel_expr\n");                                  test_instrsel_expr();
    if (verbose) printf("Running instrsel instrsel_conditionals\n");                          test_instrsel_conditionals();
    if (verbose) printf("Running instrsel function_args\n");                                  test_function_args();
    if (verbose) printf("Running instrsel instrsel_types_add_vregs\n");                       test_instrsel_types_add_vregs();
    if (verbose) printf("Running instrsel instrsel_types_cmp_pointer\n");                     test_instrsel_types_cmp_pointer();
    if (verbose) printf("Running instrsel instrsel_function_call_rearranging\n");             test_instrsel_function_call_rearranging();
    if (verbose) printf("Running instrsel misc_commutative_operations\n");                    test_misc_commutative_operations();
    if (verbose) printf("Running instrsel sub_operations\n");                                 test_sub_operations();
    if (verbose) printf("Running instrsel div_mod_operations\n");                             test_div_mod_operations();
    if (verbose) printf("Running instrsel bnot_operations\n");                                test_bnot_operations();
    if (verbose) printf("Running instrsel binary_shift_operations\n");                        test_binary_shift_operations();
    if (verbose) printf("Running instrsel constant_operations\n");                            test_constant_operations();
    if (verbose) printf("Running instrsel pointer_to_int_indirects\n");                       test_pointer_to_int_indirects();
    if (verbose) printf("Running instrsel pointer_to_uint_indirects\n");                      test_pointer_to_uint_indirects();
    if (verbose) printf("Running instrsel pointer_to_pointer_to_int_indirects\n");            test_pointer_to_pointer_to_int_indirects();
    if (verbose) printf("Running instrsel ir_move_to_reg_lvalues\n");                         test_ir_move_to_reg_lvalues();
    if (verbose) printf("Running instrsel pointer_inc\n");                                    test_pointer_inc();
    if (verbose) printf("Running instrsel pointer_add\n");                                    test_pointer_add();
    if (verbose) printf("Running instrsel pointer_sub\n");                                    test_pointer_sub();
    if (verbose) printf("Running instrsel pointer_load_constant\n");                          test_pointer_load_constant();
    if (verbose) printf("Running instrsel pointer_eq\n");                                     test_pointer_eq();
    if (verbose) printf("Running instrsel pointer_string_literal\n");                         test_pointer_string_literal();
    if (verbose) printf("Running instrsel pointer_indirect_from_stack\n");                    test_pointer_indirect_from_stack();
    if (verbose) printf("Running instrsel pointer_indirect_global_char_in_struct_to_long\n"); test_pointer_indirect_global_char_in_struct_to_long();
    if (verbose) printf("Running instrsel pointer_assignment_precision_decreases\n");         test_pointer_assignment_precision_decreases();
    if (verbose) printf("Running instrsel integer_moves\n");                                  test_integer_moves();
    if (verbose) printf("Running instrsel assign_to_pointer_to_void\n");                      test_assign_to_pointer_to_void();
    if (verbose) printf("Running instrsel pointer_comparisons\n");                            test_pointer_comparisons();
    if (verbose) printf("Running instrsel pointer_double_indirect\n");                        test_pointer_double_indirect();
    if (verbose) printf("Running instrsel composite_pointer_indirect\n");                     test_composite_pointer_indirect();
    if (verbose) printf("Running instrsel composite_pointer_address_of\n");                   test_composite_pointer_address_of();
    if (verbose) printf("Running instrsel pointer_to_void_from_long_assignment\n");           test_pointer_to_void_from_long_assignment();
    if (verbose) printf("Running instrsel ptr_to_void_memory_load_to_ptr\n");                 test_ptr_to_void_memory_load_to_ptr();
    if (verbose) printf("Running instrsel constant_cast_to_ptr\n");                           test_constant_cast_to_ptr();
    if (verbose) printf("Running instrsel spilling\n");                                       test_spilling();
    if (verbose) printf("Running instrsel param_vreg_moves\n");                               test_param_vreg_moves();

    if (failures) {
        printf("%d tests failed\n", failures);
        exit(1);
    }
    printf("All tests succeeded\n");
}
