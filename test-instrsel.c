#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

int remove_reserved_physical_registers;

void assert(long expected, long actual) {
    if (expected != actual) {
        printf("Expected %ld, got %ld\n", expected, actual);
        exit(1);
    }
}

void assert_value(Value *v1, Value *v2) {
    // printf("assert_value: ");
    // print_value(stdout, v1, 0);
    // printf(", ");
    // print_value(stdout, v2, 0);
    // printf("\n");

    if (v1->is_constant)
        assert(v1->value, v2->value);
    else if (v1->is_string_literal)
        assert(v1->vreg, v2->vreg);
    else if (v1->vreg) {
        if (v1->vreg == -1)
            assert(1, !!v2->vreg); // Check non-zero
        else
            assert(v1->vreg, v2->vreg);
    }
    else if (v1->global_symbol)
        assert(v1->global_symbol->identifier[1], v2->global_symbol->identifier[1]);
    else if (v1->label)
        assert(v1->label, v2->label);
    else if (v1->stack_index)
        assert(v1->stack_index, v2->stack_index);
    else if (v1->function_symbol)
        assert(v1->function_symbol->identifier[1], v2->function_symbol->identifier[1]);
    else
        panic("Don't know how to assert_value");
}

void assert_tac(Tac *tac, int operation, Value *dst, Value *src1, Value *src2) {
    assert(operation, tac->operation);

    if (dst)
        assert_value(dst,  tac->dst);
    else
        assert(0, !!tac->dst);

    if (src1)
        assert_value(src1, tac->src1);
    else
        assert(0, !!tac->src1);

    if (src2)
        assert_value(src2, tac->src2);
    else
        assert(0, !!tac->src2);

}

void remove_reserved_physical_register_count_from_tac(Tac *ir) {
    Tac *tac;

    if (!remove_reserved_physical_registers) return;

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
    eis1(function, 0);
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

void test_instrsel_tree_merging() {
    int j;
    Function *function;
    Tac *tac;

    function = new_function();
    remove_reserved_physical_registers = 0;

    // Test edge case of a split block with variables being live on the way
    // out. This should pervent tree merging of the assigns with the first equals.
    // The label splits the block and causes v(1) and v(2) to be in block[0] liveout
    start_ir();
    i(0, IR_ASSIGN, v(1), c(1), 0   );
    i(0, IR_ASSIGN, v(2), c(2), 0   );
    i(0, IR_EQ,     v(3), v(1), v(2));
    i(1, IR_EQ,     v(4), v(1), v(2));
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
    start_ir();
    i(0, IR_ASSIGN, v(1), c(1), 0   );
    i(0, IR_ASSIGN, v(2), c(2), 0   );
    i(1, IR_EQ,     v(4), v(1), v(2));
    finish_ir(function);
}

void test_cmp_with_conditional_jmp(Function *function, int cmp_operation, int jmp_operation, int x86_jmp_operation) {
    start_ir();
    i(0, cmp_operation,  v(3), v(1), v(2));
    i(0, jmp_operation,  0,    v(3), l(1));
    i(1, IR_NOP,         0,    0,    0   );
    finish_ir(function);
    assert_tac(ir_start,       X_CMP,             v(-1), v(1), v(2));  // v(4) is allocated but not used
    assert_tac(ir_start->next, x86_jmp_operation, 0,     l(1), 0   );
}

void test_less_than_with_conditional_jmp(Function *function, Value *src1, Value *src2) {
    start_ir();
    i(0, IR_LT,  v(3), src1, src2);
    i(0, IR_JZ,  0,    v(3), l(1));
    i(1, IR_NOP, 0,    0,    0   );
    src1 = dup_value(src1);
    src2 = dup_value(src2);
    finish_ir(function);
    assert_tac(ir_start,       X_CMP, v(-1), src1, src2);
    assert_tac(ir_start->next, X_JLT, 0,     l(1), 0   );
}

void test_cmp_with_assignment(Function *function, int cmp_operation, int x86_set_operation) {
    start_ir();
    i(0, cmp_operation, v(3), v(1), v(2));
    finish_ir(function);
    assert_tac(ir_start,             X_CMP,             v(-1), v(1), v(2));
    assert_tac(ir_start->next,       x86_set_operation, v(3),  0,    0   );
    assert_tac(ir_start->next->next, X_MOVZBQ,          v(3),  v(3), 0   );
}

void test_less_than_with_cmp_assignment(Function *function, Value *src1, Value *src2, Value *dst) {
    // dst is the renumbered live range that the output goes to. It's basically the first free register after src1 and src2.
    start_ir();
    i(0, IR_LT, v(3), src1, src2);
    src1 = dup_value(src1);
    src2 = dup_value(src2);
    finish_ir(function);
    assert_tac(ir_start,             X_CMP,    v(-1), src1, src2);
    assert_tac(ir_start->next,       X_SETLT,  v(-1), 0,   0    );
    assert_tac(ir_start->next->next, X_MOVZBQ, dst,   dst, 0    );
}

void test_instrsel() {
    Function *function;
    Tac *tac;

    function = new_function();
    remove_reserved_physical_registers = 1;

    // Load constant into register with IR_ASSIGN
    start_ir();
    i(0, IR_ASSIGN, v(1), c(1), 0);
    finish_ir(function);
    assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movq    $1, r1q"));

    // c1 + c2, with both cst/reg & reg/cst rules missing, forcing two register loads.
    // c1 goes into v2 and c2 goes into v3
    start_ir();
    nuke_rule(REGQ, IR_ADD, CST, REGQ); nuke_rule(REGQ, IR_ADD, REGQ, CST);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,                   X_MOV, v(2), c(1), 0   );
    assert_tac(ir_start->next,             X_MOV, v(3), c(2), 0   );
    assert_tac(ir_start->next->next,       X_MOV, v(1), v(3), 0   );
    assert_tac(ir_start->next->next->next, X_ADD, v(1), v(2), v(1));

    // c1 + c2, with only the cst/reg rule, forcing a register load for c2 into v2.
    start_ir();
    nuke_rule(REG, IR_ADD, REGQ, CST);
    i(0, IR_ADD, v(1), c(1), c(2));
    finish_ir(function);
    assert_tac(ir_start,             X_MOV, v(2), c(2), 0   );
    assert_tac(ir_start->next,       X_MOV, v(1), v(2), 0   );
    assert_tac(ir_start->next->next, X_ADD, v(1), c(1), v(1));

    // c1 + c2, with only the reg/cst rule, forcing a register load for c1 into v2.
    start_ir();
    nuke_rule(REGQ, IR_ADD, CST, REGQ);
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

    // arg c with only the reg rule. Forces a load of c into r1
    start_ir();
    nuke_rule(0, IR_ARG, CST, CST);
    i(0, IR_ARG, 0, c(0), c(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(1), 0   );
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
    i(0, IR_ASSIGN, g(1), c(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, g(1), c(1), 0);

    // Store c in g with only the reg fule, forcing c into r1
    start_ir();
    nuke_rule(MEMQ, IR_ASSIGN, CST, 0);
    i(0, IR_ASSIGN, g(1), c(1), 0);
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(1), 0);
    assert_tac(ir_start->next, X_MOV, g(1), v(1), 0);

    // Store v1 in g using IR_ASSIGN
    start_ir();
    i(0, IR_ASSIGN, g(1), v(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, g(1), v(1), 0);

    // Load g into r1 using IR_ASSIGN
    start_ir();
    i(0, IR_ASSIGN, v(1), g(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, v(1), g(1), 0);

    // Load S into r1
    start_ir();
    i(0, IR_ASSIGN, v(1), S(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, v(1), S(1), 0);

    // Load g into r1 using IR_LOAD_VARIABLE
    start_ir();
    i(0, IR_LOAD_VARIABLE, v(1), g(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, v(1), g(1), 0);

    // Load S into r1 using IR_LOAD_VARIABLE
    start_ir();
    i(0, IR_LOAD_VARIABLE, v(1), S(1), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, v(1), S(1), 0);

    // Push a local
    start_ir();
    i(0, IR_ARG, 0, c(0), S(1));
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), S(1), 0   );
    assert_tac(ir_start->next, X_ARG, 0,    c(0), v(1));

    // Assign constant to a local
    start_ir();
    i(0, IR_ASSIGN, S(1), c(0), 0);
    finish_ir(function);
    assert_tac(ir_start, X_MOV, S(1), c(0), 0);

    // Assign constant to a local. Forces c into a register
    start_ir();
    nuke_rule(MEMQ, IR_ASSIGN, CST, 0);
    i(0, IR_ASSIGN, S(1), c(0), 0);
    finish_ir(function);
    assert_tac(ir_start,       X_MOV, v(1), c(0), 0);
    assert_tac(ir_start->next, X_MOV, S(1), v(1), 0);

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

    // JZ                                                          JNZ
    test_cmp_with_conditional_jmp(function, IR_EQ, IR_JZ,  X_JE ); test_cmp_with_conditional_jmp(function, IR_EQ, IR_JNZ, X_JNE);
    test_cmp_with_conditional_jmp(function, IR_NE, IR_JZ,  X_JNE); test_cmp_with_conditional_jmp(function, IR_NE, IR_JNZ, X_JE );
    test_cmp_with_conditional_jmp(function, IR_LT, IR_JZ,  X_JLT); test_cmp_with_conditional_jmp(function, IR_LT, IR_JNZ, X_JGE);
    test_cmp_with_conditional_jmp(function, IR_GT, IR_JZ,  X_JGT); test_cmp_with_conditional_jmp(function, IR_GT, IR_JNZ, X_JLE);
    test_cmp_with_conditional_jmp(function, IR_LE, IR_JZ,  X_JLE); test_cmp_with_conditional_jmp(function, IR_LE, IR_JNZ, X_JGT);
    test_cmp_with_conditional_jmp(function, IR_GE, IR_JZ,  X_JGE); test_cmp_with_conditional_jmp(function, IR_GE, IR_JNZ, X_JLT);

    // a < b with a conditional with different src1 and src2 operands
    test_less_than_with_conditional_jmp(function, v(1), c(1));
    test_less_than_with_conditional_jmp(function, g(1), c(1));
    test_less_than_with_conditional_jmp(function, v(1), g(1));
    test_less_than_with_conditional_jmp(function, g(1), v(1));
    test_less_than_with_conditional_jmp(function, v(1), S(1));
    test_less_than_with_conditional_jmp(function, S(1), v(1));
    test_less_than_with_conditional_jmp(function, S(1), c(1));

    // Conditional assignment with 2 registers
    test_cmp_with_assignment(function, IR_EQ, X_SETE);
    test_cmp_with_assignment(function, IR_NE, X_SETNE);
    test_cmp_with_assignment(function, IR_LT, X_SETLT);
    test_cmp_with_assignment(function, IR_GT, X_SETGT);
    test_cmp_with_assignment(function, IR_LE, X_SETLE);
    test_cmp_with_assignment(function, IR_GE, X_SETGE);

    // Test r1 = a < b with different src1 and src2 operands
    test_less_than_with_cmp_assignment(function, v(1), c(1), v(2));
    test_less_than_with_cmp_assignment(function, g(1), c(1), v(1));
    test_less_than_with_cmp_assignment(function, v(1), g(1), v(2));
    test_less_than_with_cmp_assignment(function, g(1), v(1), v(2));
    test_less_than_with_cmp_assignment(function, S(1), c(1), v(1));
    test_less_than_with_cmp_assignment(function, v(1), S(1), v(2));
    test_less_than_with_cmp_assignment(function, S(1), v(1), v(2));
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
    Function *function;
    Tac *tac;

    function = new_function();

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

// Test instruction add
Tac *si(Function *function, int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    start_ir();
    i(label, operation, dst, src1, src2);
    finish_ir(function);

    return tac;
}

void test_instrsel_types_add_mem_vreg() {
    Function *function;

    function = new_function();
    remove_reserved_physical_registers = 1;

    // A non exhaustive set of tests to check global/reg addition with various integer sizes

    // Test sign extension of globals
    // ------------------------------

    // c = vs + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_SHORT), gsz(1, TYPE_CHAR));
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "movsbw  g1(%rip), r3w"));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "movw    r3w, r2w"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next, 0, 0, 0), "addw    r1w, r2w"     ));

    // c = vi + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_INT), gsz(1, TYPE_CHAR));
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "movsbl  g1(%rip), r3l"));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "movl    r3l, r2l"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next, 0, 0, 0), "addl    r1l, r2l"     ));

    // c = vl + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_LONG), gsz(1, TYPE_CHAR));
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "movsbq  g1(%rip), r3q"));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "movq    r3q, r2q"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next, 0, 0, 0), "addq    r1q, r2q"     ));

    // l = vi + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_LONG), vsz(1, TYPE_INT), gsz(1, TYPE_CHAR));
    assert(0, strcmp(render_x86_operation(ir_start,                   0, 0, 0), "movslq  r1l, r3q"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next,             0, 0, 0), "movsbq  g1(%rip), r4q"));
    assert(0, strcmp(render_x86_operation(ir_start->next->next,       0, 0, 0), "movq    r4q, r2q"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next->next, 0, 0, 0), "addq    r3q, r2q"     ));

    // l = vl + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_LONG), vsz(1, TYPE_LONG), gsz(1, TYPE_CHAR));
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "movsbq  g1(%rip), r3q"));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "movq    r3q, r2q"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next, 0, 0, 0), "addq    r1q, r2q"     ));

    // l = gc + vl
    si(function, 0, IR_ADD, vsz(2, TYPE_LONG), gsz(1, TYPE_CHAR), vsz(1, TYPE_LONG));
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "movsbq  g1(%rip), r3q"));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "movq    r1q, r2q"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next, 0, 0, 0), "addq    r3q, r2q"     ));

    // Test sign extension of locals
    // ------------------------------

    // c = vs + gc
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_SHORT), Ssz(1, TYPE_CHAR));
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "movsbw  16(%rbp), r3w"));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "movw    r3w, r2w"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next, 0, 0, 0), "addw    r1w, r2w"     ));

    // Test sign extension of registers
    // --------------------------------
    // c = vc + gs
    si(function, 0, IR_ADD, vsz(2, TYPE_CHAR), vsz(1, TYPE_CHAR), gsz(1, TYPE_SHORT));
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "movsbw  r1b, r3w"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "movw    r3w, r2w"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next, 0, 0, 0), "addw    g1(%rip), r2w"));
}

void test_instrsel_types_cmp_assignment() {
    Function *function;

    function = new_function();
    remove_reserved_physical_registers = 1;

    // Test s = l == l
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_SHORT), v(1), v(2));
    finish_ir(function);
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "cmpq    r2q, r1q"));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "sete    r3b"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next, 0, 0, 0), "movzbw  r3b, r3w"));

    // Test c = s == s
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_CHAR), vsz(1, TYPE_SHORT), vsz(2, TYPE_SHORT));
    finish_ir(function);
    assert(0, strcmp(render_x86_operation(ir_start,             0, 0, 0), "cmpw    r2w, r1w"));
    assert(0, strcmp(render_x86_operation(ir_start->next,       0, 0, 0), "sete    r3b"     ));

    // Test s = c == l
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_SHORT), vsz(1, TYPE_CHAR), vsz(2, TYPE_LONG));
    finish_ir(function);
    assert(0, strcmp(render_x86_operation(ir_start,                   0, 0, 0), "movsbq  r1b, r4q"));
    assert(0, strcmp(render_x86_operation(ir_start->next,             0, 0, 0), "cmpq    r2q, r4q"));
    assert(0, strcmp(render_x86_operation(ir_start->next->next,       0, 0, 0), "sete    r3b"     ));
    assert(0, strcmp(render_x86_operation(ir_start->next->next->next, 0, 0, 0), "movzbw  r3b, r3w"));
}

void test_instrsel_returns() {
    Function *function;

    function = new_function();
    remove_reserved_physical_registers = 1;

    // Return constant & vregs
    si(function, 0, IR_RETURN, 0, c(1), 0);               assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "mov     $1, %rax"));
    si(function, 0, IR_RETURN, 0, vsz(1, TYPE_CHAR),  0); assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movsbq  r1b, %rax"));
    si(function, 0, IR_RETURN, 0, vsz(1, TYPE_SHORT), 0); assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movswq  r1w, %rax"));
    si(function, 0, IR_RETURN, 0, vsz(1, TYPE_INT),   0); assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movslq  r1l, %rax"));
    si(function, 0, IR_RETURN, 0, vsz(1, TYPE_LONG),  0); assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movq    r1q, %rax"));

    si(function, 0, IR_RETURN, 0, gsz(1, TYPE_CHAR),  0); assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movsbq  g1(%rip), %rax"));
    si(function, 0, IR_RETURN, 0, gsz(1, TYPE_SHORT), 0); assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movswq  g1(%rip), %rax"));
    si(function, 0, IR_RETURN, 0, gsz(1, TYPE_INT),   0); assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movslq  g1(%rip), %rax"));
    si(function, 0, IR_RETURN, 0, gsz(1, TYPE_LONG),  0); assert(0, strcmp(render_x86_operation(ir_start, 0, 0, 0), "movq    g1(%rip), %rax"));
}

void test_instrsel_function_calls() {
    Function *function;

    function = new_function();
    remove_reserved_physical_registers = 1;

    // The legacy backend extends %rax coming from a call to a quad. These
    // tests don't test much, just that the rules will work.
    si(function, 0, IR_CALL, vsz(1, TYPE_CHAR),  fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
    si(function, 0, IR_CALL, vsz(1, TYPE_SHORT), fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
    si(function, 0, IR_CALL, vsz(1, TYPE_INT),   fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
    si(function, 0, IR_CALL, vsz(1, TYPE_LONG),  fu(1), 0); assert_tac(ir_start, X_CALL, v(1), fu(1), 0);
}

void test_instrsel_function_call_rearranging() {

    Function *function;

    function = new_function();
    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_START_CALL, 0,    c(0),  0);
    i(0, IR_CALL,       v(1), fu(1), 0);
    i(0, IR_END_CALL,   0,    c(0),  0);
    i(0, IR_ASSIGN,     g(1), v(1),  0);
    finish_ir(function);

    assert_tac(ir_start,                   IR_START_CALL, 0, c(0), 0);
    assert_tac(ir_start->next,             X_CALL, v(1), fu(1), 0);
    assert_tac(ir_start->next->next,       IR_END_CALL, 0, c(0), 0);
    assert_tac(ir_start->next->next->next, X_MOV, g(1), v(1), 0);
}

int main() {
    ssa_physical_register_count = 12;
    ssa_physical_register_count = 0;
    opt_optimize_arithmetic_operations = 1;
    string_literals = malloc(MAX_STRING_LITERALS);

    init_allocate_registers();
    init_instruction_selection_rules();

    test_instrsel_tree_merging();
    test_instrsel();
    test_instrsel_types_add_vregs();
    test_instrsel_types_add_mem_vreg();
    test_instrsel_types_cmp_assignment();
    test_instrsel_returns();
    test_instrsel_function_calls();
    test_instrsel_function_call_rearranging();

}
