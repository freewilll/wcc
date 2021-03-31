#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

void add_cast_rules() {
    Rule *r;
    int i, j;

    // Casting from/to any integer types with decreasing precision
    // Similar to generic register register sign extend rules
    r = add_rule(REGW, IR_MOVE, REGB, 0, 2); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b, %vdw");
    r = add_rule(REGL, IR_MOVE, REGB, 0, 2); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b, %vdl");
    r = add_rule(REGQ, IR_MOVE, REGB, 0, 2); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b, %vdq");
    r = add_rule(REGL, IR_MOVE, REGW, 0, 2); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w, %vdl");
    r = add_rule(REGQ, IR_MOVE, REGW, 0, 2); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w, %vdq");
    r = add_rule(REGQ, IR_MOVE, REGL, 0, 2); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l, %vdq");

    // Crazy shit for some code that uses a (long *) to store pointers to longs.
    // The sane code should have been using (long **)
    // long *sp; (*((long *) *sp))++ = a;
    r = add_rule(ADRQ, IR_MOVE, REGQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0 , "movq %v1q, %vdq");
}

Rule *add_store_to_pointer(int src1, int src2, char *template) {
    Rule *r;

    r = add_rule(src1, IR_MOVE_TO_REG_LVALUE, src1, src2, 3);
    add_op(r, X_MOV,        DST, SRC1, 0,    "movq %v1q, %vdq");
    add_op(r, X_MOV_TO_IND, 0,   DST,  SRC2, template);
    return r;
}

// Add rules for loads & address of from IR_BSHL + IR_ADD + IR_INDIRECT/IR_ADDRESS_OF
void add_scaled_rule(int *ntc, int cst, int add_reg, int indirect_reg, int address_of_reg, int match_dst, int op, char *template) {
    int ntc1, ntc2;
    Rule *r;

    ntc1 = ++*ntc;
    ntc2 = ++*ntc;
    r = add_rule(ntc1, IR_BSHL, REGQ, cst, 1); add_save_value(r, 1, 2); // Save index register to slot 2
    r = add_rule(ntc2, IR_ADD, add_reg, ntc1, 1); add_save_value(r, 1, 1); // Save address register to slot 1

    if (indirect_reg) r = add_rule(indirect_reg, IR_INDIRECT, ntc2, 0, 1);
    if (address_of_reg) r = add_rule(address_of_reg, IR_ADDRESS_OF, ntc2, 0, 1);
    r->match_dst = match_dst;
    add_load_value(r, 1, 1); // Load address register from slot 1
    add_load_value(r, 2, 2); // Load index register from slot 2
    add_op(r, op, DST, SRC1, 0, template);
}

void add_composite_pointer_rules(int *ntc) {
    int i;
    char *template;
    Rule *r;

    // Loads
    add_scaled_rule(ntc, CST1, ADRW, REGW, 0, 1, X_MOV_FROM_SCALED_IND, "movw   (%v1q,%v2q,2), %vdw"); // from *short to short
    add_scaled_rule(ntc, CST1, ADRW, REGL, 0, 1, X_MOV_FROM_SCALED_IND, "movswl (%v1q,%v2q,2), %vdl"); // from *short to int
    add_scaled_rule(ntc, CST1, ADRW, REGQ, 0, 1, X_MOV_FROM_SCALED_IND, "movswq (%v1q,%v2q,2), %vdq"); // from *short to long
    add_scaled_rule(ntc, CST2, ADRL, REGL, 0, 1, X_MOV_FROM_SCALED_IND, "movl   (%v1q,%v2q,4), %vdl"); // from *int to int
    add_scaled_rule(ntc, CST2, ADRL, REGQ, 0, 1, X_MOV_FROM_SCALED_IND, "movslq (%v1q,%v2q,4), %vdq"); // from *int to long
    add_scaled_rule(ntc, CST3, ADRQ, REGQ, 0, 1, X_MOV_FROM_SCALED_IND, "movq   (%v1q,%v2q,8), %vdq"); // from *long to long
    add_scaled_rule(ntc, CST3, ADRQ, ADRV, 0, 1, X_MOV_FROM_SCALED_IND, "movq   (%v1q,%v2q,8), %vdq"); // from *struct

    // Address of
    add_scaled_rule(ntc, CST1, ADRV, 0, ADRV, 1, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,2), %vdq");
    add_scaled_rule(ntc, CST2, ADRV, 0, ADRV, 1, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,4), %vdq");
    add_scaled_rule(ntc, CST3, ADRV, 0, ADRV, 1, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,8), %vdq");
}

void add_pointer_rules(int *ntc) {
    int i, j;
    Rule *r;

    // Loads & stores
    r = add_rule(ADRV, IR_MOVE, ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRV, IR_MOVE, ADRB, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE, ADRW, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE, ADRL, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE, ADRQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE, REGQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1; // For register (* void) v = (long) l;
    r = add_rule(MDRV, IR_MOVE, REGQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1; // For memory (* void) v = (long) l;

    for (i = ADRB; i <= ADRQ; i++) {
        for (j = ADRB; j <= ADRQ; j++) {
            r = add_rule(i,  IR_MOVE, j, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
            r->match_dst = 1;
        }
    }

    for (i = ADRB; i <= ADRQ; i++) {
        r = add_rule(i,  IR_MOVE, ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
        r->match_dst = 1;
    }

    for (i = MDRB; i <= MDRQ; i++) {
        r = add_rule(i,  IR_MOVE, ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
        r->match_dst = 1;
    }

    r = add_rule(MDR,  IR_MOVE,       ADR,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(MDRV, IR_MOVE,       ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADR,  IR_ADDRESS_OF, ADR,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r); // & for *&*&*pi
    r = add_rule(ADRV, IR_ADDRESS_OF, ADRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(REGQ, IR_MOVE,       ADR,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(ADRB, IR_MOVE,       MEMQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRW, IR_MOVE,       MEMQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRL, IR_MOVE,       MEMQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRQ, IR_MOVE,       MEMQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_MOVE,       MDRV, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRB, 0,             MDRB, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRW, 0,             MDRW, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRL, 0,             MDRL, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(ADRQ, 0,             MDRQ, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");

    // Address loads
    r = add_rule(ADRB, 0,             STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(ADRB, IR_MOVE,       STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(ADRB, IR_ADDRESS_OF, MEMQ, 0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); // For **ppi = 1
    r = add_rule(ADRW, IR_ADDRESS_OF, MEMQ, 0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(ADRL, IR_ADDRESS_OF, MEMQ, 0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(ADR,  IR_ADDRESS_OF, MEM,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); fin_rule(r);
    r = add_rule(ADRQ, IR_ADDRESS_OF, MDR,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); fin_rule(r);

    // Loads from pointer
    r = add_rule(REGB, IR_INDIRECT, ADRB, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movb   (%v1q), %vdb"); r->match_dst = 1;
    r = add_rule(REGL, IR_INDIRECT, ADRB, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movsbl (%v1q), %vdl"); r->match_dst = 1;
    r = add_rule(REGQ, IR_INDIRECT, ADRB, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movsbq (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(REGW, IR_INDIRECT, ADRW, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movw   (%v1q), %vdw"); r->match_dst = 1;
    r = add_rule(REGL, IR_INDIRECT, ADRW, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movswl (%v1q), %vdl"); r->match_dst = 1;
    r = add_rule(REGQ, IR_INDIRECT, ADRW, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movswq (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(REGW, IR_INDIRECT, ADRL, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movslw (%v1q), %vdl"); r->match_dst = 1;
    r = add_rule(REGL, IR_INDIRECT, ADRL, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movl   (%v1q), %vdl"); r->match_dst = 1;
    r = add_rule(REGQ, IR_INDIRECT, ADRL, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movslq (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(REGQ, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;

    r = add_rule(ADRB, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(ADRW, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(ADRL, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(ADRQ, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;
    r = add_rule(ADRV, IR_INDIRECT, ADRQ, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq"); r->match_dst = 1;

    add_composite_pointer_rules(ntc);

    // Stores of a pointer to a pointer
    add_store_to_pointer(ADRQ, ADRB, "movq %v2q, (%v1q)");
    add_store_to_pointer(ADRQ, ADRW, "movq %v2q, (%v1q)");
    add_store_to_pointer(ADRQ, ADRL, "movq %v2q, (%v1q)");
    add_store_to_pointer(ADRQ, ADRQ, "movq %v2q, (%v1q)");
    add_store_to_pointer(ADRQ, ADRV, "movq %v2q, (%v1q)");

    // Stores to pointer from registers
    add_store_to_pointer(ADRB, REGB, "movb %v2b, (%v1q)");
    add_store_to_pointer(ADRB, REGW, "movb %v2b, (%v1q)");
    add_store_to_pointer(ADRB, REGL, "movb %v2b, (%v1q)");
    add_store_to_pointer(ADRB, REGQ, "movb %v2b, (%v1q)");
    add_store_to_pointer(ADRW, REGW, "movw %v2w, (%v1q)");
    add_store_to_pointer(ADRW, REGL, "movw %v2w, (%v1q)");
    add_store_to_pointer(ADRW, REGQ, "movw %v2w, (%v1q)");
    add_store_to_pointer(ADRL, REGL, "movl %v2l, (%v1q)");
    add_store_to_pointer(ADRL, REGQ, "movl %v2l, (%v1q)");
    add_store_to_pointer(ADRQ, REGQ, "movq %v2q, (%v1q)");

    // Stores of 32 bit constants in a pointer.
    add_store_to_pointer(ADRB, CSTL, "movb $%v2b, (%v1q)");
    add_store_to_pointer(ADRW, CSTL, "movw $%v2w, (%v1q)");
    add_store_to_pointer(ADRL, CSTL, "movl $%v2l, (%v1q)");
    add_store_to_pointer(ADRQ, CSTL, "movq $%v2q, (%v1q)");
    add_store_to_pointer(ADRV, CSTL, "movq $%v2q, (%v1q)");
}

void add_conditional_zero_jump_rule(int operation, int src1, int src2, int cost, int x86_cmp_operation, char *comparison, char *conditional_jmp, int do_fin_rule) {
    int x86_jmp_operation;
    Rule *r;

    x86_jmp_operation = operation == IR_JZ ? X_JZ : X_JNZ;

    r = add_rule(0, operation, src1, src2, cost);
    add_op(r, x86_cmp_operation, 0, SRC1, 0, comparison);
    add_op(r, x86_jmp_operation, 0, SRC2, 0, conditional_jmp);
    if (do_fin_rule) fin_rule(r);
}

void add_comparison_conditional_jmp_rules(int *ntc, int src1, int src2, char *template) {
    Rule *r;

    (*ntc)++;
    r = add_rule(*ntc, IR_EQ,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JE,  0, SRC2, 0,    "je %v1" ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JNE, 0, SRC2, 0,    "jne %v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_NE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JNE, 0, SRC2, 0,    "jne %v1"); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JE,  0, SRC2, 0,    "je %v1" ); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_LT,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JLT, 0, SRC2, 0,    "jl %v1" ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JGE, 0, SRC2, 0,    "jge %v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_GT,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JGT, 0, SRC2, 0,    "jg %v1" ); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JLE, 0, SRC2, 0,    "jle %v1"); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_LE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JLE, 0, SRC2, 0,    "jle %v1"); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JGT, 0, SRC2, 0,    "jg %v1" ); fin_rule(r);

    (*ntc)++;
    r = add_rule(*ntc, IR_GE,  src1, src2, 10); add_op(r, X_CMP, 0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, X_JGE, 0, SRC2, 0,    "jge %v1"); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, X_JLT, 0, SRC2, 0,    "jl %v1" ); fin_rule(r);
}

void add_comparison_assignment_rule(int src1, int src2, char *cmp_template, int operation, int set_operation, char *set_template) {
    int i;
    Rule *r;

    for (i = 1; i <= 4; i++) {
        r = add_rule(REG + i, operation, src1, src2, 12 + (i > 1));

        add_op(r, X_CMP,         0,   SRC1, SRC2, cmp_template);
        add_op(r, set_operation, DST, 0,    0,    set_template);

             if (i == 2) add_op(r, X_MOVZBW, DST, DST, 0, "movzbw %v1b, %v1w");
        else if (i == 3) add_op(r, X_MOVZBL, DST, DST, 0, "movzbl %v1b, %v1l");
        else if (i == 4) add_op(r, X_MOVZBQ, DST, DST, 0, "movzbq %v1b, %v1q");
        fin_rule(r);
    }
}

void add_comparison_assignment_rules(int src1, int src2, char *template) {
    Rule *r;

    add_comparison_assignment_rule(src1, src2, template, IR_EQ, X_SETE,  "sete %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_NE, X_SETNE, "setne %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_LT, X_SETLT, "setl %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_GT, X_SETGT, "setg %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_LE, X_SETLE, "setle %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_GE, X_SETGE, "setge %vdb");
}

void add_commutative_operation_rules(char *x86_operand, int operation, int x86_operation, int cost) {
    int i;
    char *op_vv, *op_cv;
    Rule *r;

    asprintf(&op_vv, "%s %%v1, %%v2",  x86_operand); // Perform operation on two registers or memory
    asprintf(&op_cv, "%s $%%v1, %%v2", x86_operand); // Perform operation on a constant and a register

    r = add_rule(REG, operation, REG, REG, cost);     add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_vv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, CST, REG, cost);     add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_cv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, REG, CST, cost);     add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_cv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, REG, MEM, cost + 1); add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_vv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, MEM, REG, cost + 1); add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s %v1, %vd" );
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_vv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, CST, MEM, cost + 1); add_op(r, X_MOV,         DST, SRC1, 0,   "mov%s $%v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC2, DST, op_vv            );
                                                      fin_rule(r);
    r = add_rule(REG, operation, MEM, CST, cost + 1); add_op(r, X_MOV,         DST, SRC2, 0,   "mov%s $%v1, %vd");
                                                      add_op(r, x86_operation, DST, SRC1, DST, op_vv            );
                                                      fin_rule(r);


    if (operation == IR_ADD) {
        r = add_rule(ADR, operation, REGQ, ADR, cost);    add_op(r, X_MOV,         DST, SRC2, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC1, DST, "addq %v1q, %v2q");
                                                          fin_rule(r);
        r = add_rule(ADR, operation, ADR, REGQ, cost);    add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq %v1q, %v2q");
                                                          fin_rule(r);

        r = add_rule(ADR, operation, CSTL, ADR, cost);    add_op(r, X_MOV,         DST, SRC2, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC1, DST, "addq $%v1q, %v2q");
                                                          fin_rule(r);
        r = add_rule(ADR, operation, ADR, CSTL, cost);    add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");
                                                          fin_rule(r);

        r = add_rule(ADRV, operation, REGQ, ADRV, cost);  add_op(r, X_MOV,         DST, SRC2, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC1, DST, "addq %v1q, %v2q");
        r = add_rule(ADRV, operation, ADRV, REGQ, cost);  add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq %v1q, %v2q");

        r = add_rule(ADRV, operation, CSTL, ADRV, cost);  add_op(r, X_MOV,         DST, SRC2, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC1, DST, "addq $%v1q, %v2q");
        r = add_rule(ADRV, operation, ADRV, CSTL, cost);  add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");

        r = add_rule(REGQ, operation, ADR, CSTL, cost);   add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq" );
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");
                                                          fin_rule(r);
        r = add_rule(REGQ, operation, ADRV, CSTL, cost);  add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq" );
                                                          add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");

        for (i = ADRB; i <= ADRQ; i++) {
            r = add_rule(i, operation, ADRV, CSTL, cost);
            add_op(r, X_MOV,         DST, SRC1, 0,   "movq %v1q, %vdq");
            add_op(r, x86_operation, DST, SRC2, DST, "addq $%v1q, %v2q");
            r->match_dst = 1;
          }
    }
}

void add_sub_rule(int dst, int src1, int src2, int cost, char *mov_template, char *sub_template) {
    Rule *r;

    r = add_rule(dst, IR_SUB, src1, src2, cost);
    add_op(r, X_MOV, DST, SRC1, 0,   mov_template);
    add_op(r, X_SUB, DST, SRC2, DST, sub_template);
    fin_rule(r);
}

void add_sub_rules() {
    int i, j;
    Rule *r;

    add_sub_rule(REG, REG, REG,  11, "mov%s %v1, %vd",  "sub%s %v1, %vd");
    add_sub_rule(REG, CST, REG,  11, "mov%s $%v1, %vd", "sub%s %v1, %vd");
    add_sub_rule(REG, REG, CST,  11, "mov%s %v1, %vd",  "sub%s $%v1, %vd");
    add_sub_rule(REG, ADRB, REG, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(REG, ADRW, REG, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(REG, ADRL, REG, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(REG, ADRQ, REG, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(ADR, ADR, CST,  10, "movq %v1q, %vdq", "subq $%v1q, %vdq");
    add_sub_rule(ADR, ADR, REGQ, 11, "movq %v1q, %vdq", "subq %v1q, %vdq");
    add_sub_rule(REG, REG, MEM,  11, "mov%s %v1, %vd",  "sub%s %v1, %vd");
    add_sub_rule(REG, MEM, REG,  11, "mov%s %v1, %vd",  "sub%s %v1, %vd");
    add_sub_rule(REG, CST, MEM,  11, "mov%s $%v1, %vd", "sub%s %v1, %vd");
    add_sub_rule(REG, MEM, CST,  11, "mov%s %v1, %vd",  "sub%s $%v1, %vd");

    for (i = ADRB; i <= ADRV; i++) {
        for (j = ADRB; j <= ADRV; j++) {
            add_sub_rule(REGQ, i, j,  11, "movq %v1q, %vdq", "subq %v1q, %vdq");
        }
    }

    add_sub_rule(ADRV, ADRV, CSTL,  11, "movq %v1q, %vdq",  "subq $%v1q, %vdq");
}

void add_div_rule(int dst, int src1, int src2, int cost, char *t1, char *t2, char *t3, char *t4, char *tdiv, char *tmod) {
    Rule *r;

    r = add_rule(dst, IR_DIV, src1,  src2,  cost); add_op(r, X_MOV,  0,    SRC1, 0,    t1);
                                                   add_op(r, X_CLTD, 0,    0,    0,    t2);
                                                   add_op(r, X_MOV,  DST,  SRC2, 0,    t3);
                                                   add_op(r, X_IDIV, DST,  SRC1, SRC2, t4);
                                                   add_op(r, X_MOV,  DST,  0,    0,    tdiv);
                                                   fin_rule(r);
    r = add_rule(dst, IR_MOD, src1,  src2,  cost); add_op(r, X_MOV,  0,    SRC1, 0,    t1);
                                                   add_op(r, X_CLTD, 0,    0,    0,    t2);
                                                   add_op(r, X_MOV,  DST,  SRC2, 0,    t3);
                                                   add_op(r, X_IDIV, DST,  SRC1, SRC2, t4);
                                                   add_op(r, X_MOV,  DST,  0,    0,    tmod);
                                                   fin_rule(r);
}

void add_div_rules() {
    Rule *r;

    add_div_rule(REGL, REGL, REGL, 40, "movl %v1l, %%eax", "cltd", "movl %v1l, %vdl", "idivl %vdl", "movl %%eax, %vdl", "movl %%edx, %vdl");
    add_div_rule(REGQ, REGQ, REGQ, 50, "movq %v1q, %%rax", "cqto", "movq %v1q, %vdq", "idivq %vdq", "movq %%rax, %vdq", "movq %%rdx, %vdq");
}

void add_bnot_rules() {
    Rule *r;

    r = add_rule(REG, IR_BNOT, REG, 0, 3); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd");
                                           add_op(r, X_BNOT, DST, DST,  0, "not%s %vd");
                                           fin_rule(r);
    r = add_rule(REG, IR_BNOT, MEM, 0, 3); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd");
                                           add_op(r, X_BNOT, DST, DST,  0, "not%s %vd");
                                           fin_rule(r);
}

void add_binary_shift_rule(int src2, char *template) {
    Rule *r;

    r = add_rule(REG, IR_BSHL, REG, src2, 4); add_op(r, X_MOV, DST, SRC2, 0, template);
                                              add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                              add_op(r, X_SHL, DST, SRC2, 0, "shl%s %%cl, %vd");
                                              fin_rule(r);
    r = add_rule(REG, IR_BSHR, REG, src2, 4); add_op(r, X_MOV, DST, SRC2, 0, template);
                                              add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                              add_op(r, X_SAR, DST, SRC2, 0, "sar%s %%cl, %vd");
                                              fin_rule(r);
}

void add_binary_shift_rules() {
    Rule *r;

    r = add_rule(REG, IR_BSHL, REG, CST, 3); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SHL, DST, SRC2, 0, "shl%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(REG, IR_BSHR, REG, CST, 3); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SAR, DST, SRC2, 0, "sar%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(REG, IR_BSHL, MEM, CST, 4); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SHL, DST, SRC2, 0, "shl%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(REG, IR_BSHR, MEM, CST, 4); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SAR, DST, SRC2, 0, "sar%s $%v1, %vd");
                                             fin_rule(r);

    add_binary_shift_rule(REGB, "movb %v1b, %%cl");
    add_binary_shift_rule(REGW, "movw %v1w, %%cx");
    add_binary_shift_rule(REGL, "movl %v1l, %%ecx");
    add_binary_shift_rule(REGQ, "movq %v1q, %%rcx");
}

void init_instruction_selection_rules() {
    int i, j;
    Rule *r;
    int ntc; // Non terminal counter
    char *cmp_rr, *cmp_rc, *cmp_rm, *cmp_mr, *cmp_mc;
    char *cmpq_rr, *cmpq_rc, *cmpq_rm, *cmpq_mr, *cmpq_mc;

    instr_rule_count = 0;
    disable_merge_constants = 0;

    instr_rules = malloc(MAX_RULE_COUNT * sizeof(Rule));
    memset(instr_rules, 0, MAX_RULE_COUNT * sizeof(Rule));

    ntc = AUTO_NON_TERMINAL_START;

    // Identity rules, for matching leaf nodes in the instruction tree
    r = add_rule(REG,  0, REG,  0, 0); fin_rule(r);
    r = add_rule(CSTL, 0, CSTL, 0, 0);
    r = add_rule(CSTQ, 0, CSTQ, 0, 0);
    r = add_rule(CST1, 0, CST1, 0, 0);
    r = add_rule(CST2, 0, CST2, 0, 0);
    r = add_rule(CST3, 0, CST3, 0, 0);
    r = add_rule(MEM,  0, MEM,  0, 0); fin_rule(r);
    r = add_rule(ADR,  0, ADR,  0, 0); fin_rule(r);
    r = add_rule(ADRV, 0, ADRV, 0, 0);
    r = add_rule(MDRV, 0, MDRV, 0, 0);
    r = add_rule(MDR,  0, MDR,  0, 0); fin_rule(r);
    r = add_rule(REGQ, 0, ADRQ, 0, 0);
    r = add_rule(REGQ, 0, ADRV, 0, 0);
    r = add_rule(LAB,  0, LAB,  0, 0); fin_rule(r);
    r = add_rule(FUN,  0, FUN,  0, 0); fin_rule(r);

    // This rule is needed for memory-memory moves
    r = add_rule(ADRV, 0, MDRV, 0, 2); add_op(r, X_MOV, DST, SRC1, 0 , "movq %v1q, %vdq");

    add_cast_rules();

    // Register -> register sign extension for precision increases
    r = add_rule(REGW, 0, REGB, 0, 2); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b, %vdw");
    r = add_rule(REGL, 0, REGB, 0, 2); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b, %vdl");
    r = add_rule(REGQ, 0, REGB, 0, 2); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b, %vdq");
    r = add_rule(REGL, 0, REGW, 0, 2); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w, %vdl");
    r = add_rule(REGQ, 0, REGW, 0, 2); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w, %vdq");
    r = add_rule(REGQ, 0, REGL, 0, 2); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l, %vdq");

    // Memory -> register sign extension
    r = add_rule(REGW, 0, MEMB, 0, 2); add_op(r, X_MOVSBW, DST, SRC1, 0 , "movsbw %v1b, %vdw"); fin_rule(r);
    r = add_rule(REGL, 0, MEMB, 0, 2); add_op(r, X_MOVSBL, DST, SRC1, 0 , "movsbl %v1b, %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, MEMB, 0, 2); add_op(r, X_MOVSBQ, DST, SRC1, 0 , "movsbq %v1b, %vdq"); fin_rule(r);
    r = add_rule(REGL, 0, MEMW, 0, 2); add_op(r, X_MOVSWL, DST, SRC1, 0 , "movswl %v1w, %vdl"); fin_rule(r);
    r = add_rule(REGQ, 0, MEMW, 0, 2); add_op(r, X_MOVSWQ, DST, SRC1, 0 , "movswq %v1w, %vdq"); fin_rule(r);
    r = add_rule(REGQ, 0, MEML, 0, 2); add_op(r, X_MOVSLQ, DST, SRC1, 0 , "movslq %v1l, %vdq"); fin_rule(r);

    r = add_rule(0, IR_RETURN, 0,    0, 1); add_op(r, X_RET,  0,   0, 0, 0                      ); fin_rule(r); // Return void
    r = add_rule(0, IR_RETURN, CSTL, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "mov $%v1q, %%rax"  ); fin_rule(r); // Return constant
    r = add_rule(0, IR_RETURN, CSTQ, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "mov $%v1q, %%rax"  ); fin_rule(r); // Return constant
    r = add_rule(0, IR_RETURN, REGB, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movsbq %v1b, %%rax"); fin_rule(r); // Return register byte
    r = add_rule(0, IR_RETURN, REGW, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movswq %v1w, %%rax"); fin_rule(r); // Return register word
    r = add_rule(0, IR_RETURN, REGL, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movslq %v1l, %%rax"); fin_rule(r); // Return register long
    r = add_rule(0, IR_RETURN, REGQ, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movq %v1q, %%rax"  ); fin_rule(r); // Return register quad
    r = add_rule(0, IR_RETURN, MEMB, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movsbq %v1b, %%rax"); fin_rule(r); // Return memory byte
    r = add_rule(0, IR_RETURN, MEMW, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movswq %v1w, %%rax"); fin_rule(r); // Return memory word
    r = add_rule(0, IR_RETURN, MEML, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movslq %v1l, %%rax"); fin_rule(r); // Return memory long
    r = add_rule(0, IR_RETURN, MEMQ, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movq %v1q, %%rax"  ); fin_rule(r); // Return memory quad
    r = add_rule(0, IR_RETURN, ADR,  0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movq %v1q, %%rax"  ); fin_rule(r); // Return a pointer
    r = add_rule(0, IR_RETURN, ADRV, 0, 1); add_op(r, X_RET,  0,   SRC1, 0, "movq %v1q, %%rax"  );              // Return a *void

    r = add_rule(0, IR_ARG, CSTL, CSTL, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq $%v2q"); // Use constant as function arg, must not be a quad

    // Add rules for sign extention an arg, but at a high cost, to encourage other rules to take precedence
    r = add_rule(0, IR_ARG, CSTL, REGB, 10); add_op(r, X_MOVSBQ, SRC2, SRC2, 0 , "movsbq %v1b, %v1q"); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" );
    r = add_rule(0, IR_ARG, CSTL, REGW, 10); add_op(r, X_MOVSWQ, SRC2, SRC2, 0 , "movswq %v1w, %v1q"); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" );
    r = add_rule(0, IR_ARG, CSTL, REGL, 10); add_op(r, X_MOVSLQ, SRC2, SRC2, 0 , "movslq %v1l, %v1q"); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" );

    r = add_rule(0, IR_ARG, CSTL, REGQ, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); // Use register as function arg, must be a quad

    r = add_rule(0, IR_ARG, CSTL, ADRB, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); // Use pointer in register as function arg
    r = add_rule(0, IR_ARG, CSTL, ADRW, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); // Use pointer in register as function arg
    r = add_rule(0, IR_ARG, CSTL, ADRL, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); // Use pointer in register as function arg
    r = add_rule(0, IR_ARG, CSTL, ADRQ, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); // Use pointer in register as function arg
    r = add_rule(0, IR_ARG, CSTL, ADRV, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q" ); // Use pointer in register as function arg

    r = add_rule(REG,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r); // Function call with a return value
    r = add_rule(ADR,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r); // Function call with a return value
    r = add_rule(ADRV, IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0);              // Function call with a *void return value

    r = add_rule(REG, IR_MOVE, REG, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1, %vd"  ); fin_rule(r); // Register to register copy
    r = add_rule(MEM, IR_MOVE, CST, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s $%v1, %vd" ); fin_rule(r); // Store constant in memory
    r = add_rule(MEM, IR_MOVE, REG, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0,    "mov%s %v1, %vd"  ); fin_rule(r); // Store register in memory

    // Constant loads into a register
    r = add_rule(REGL, 0,       CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movl $%v1l, %vdl");
    r = add_rule(REGQ, 0,       CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(REGQ, 0,       CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(REGL, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movl $%v1l, %vdl");
    r = add_rule(REGQ, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(REGQ, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRB, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRB, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRW, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRW, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRL, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRL, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRQ, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRQ, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRV, IR_MOVE, CSTL, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(ADRV, IR_MOVE, CSTQ, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq");

    r = add_rule(REG,  0,       MEM,  0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r); // Load temp memory into register
    r = add_rule(REG,  IR_MOVE, MEM,  0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r); // Load standalone memory into register

    add_pointer_rules(&ntc);

    r = add_rule(0, IR_JMP, LAB, 0,1);  add_op(r, X_JMP, 0, SRC1, 0, "jmp %v1"); fin_rule(r);  // JMP

    add_conditional_zero_jump_rule(IR_JZ,  REG,  LAB,  3,  X_TEST, "test%s %v1, %v1",  "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  ADR,  LAB,  3,  X_TEST, "testq %v1q, %v1q", "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  ADRV, LAB,  3,  X_TEST, "testq %v1q, %v1q", "jz %v1q",  0);
    add_conditional_zero_jump_rule(IR_JZ,  MEM,  LAB,  3,  X_CMPZ, "cmp $0, %v1",      "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JNZ, REG,  LAB,  3,  X_TEST, "test%s %v1, %v1",  "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, ADR,  LAB,  3,  X_TEST, "testq %v1q, %v1q", "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, ADRV, LAB,  3,  X_TEST, "testq %v1q, %v1q", "jnz %v1q", 0);
    add_conditional_zero_jump_rule(IR_JNZ, MEM,  LAB,  3,  X_CMPZ, "cmp $0, %v1",      "jnz %v1",  1);

    // All pairwise combinations of (CST, REG, MEM) that have associated x86 instructions
    cmp_rr = "cmp%s %v2, %v1";  cmpq_rr = "cmpq %v2q, %v1q";
    cmp_rc = "cmp%s $%v2, %v1"; cmpq_rc = "cmpq $%v2q, %v1q";
    cmp_rm = "cmp%s %v2, %v1";  cmpq_rm = "cmpq %v2q, %v1q";
    cmp_mr = "cmp%s %v2, %v1";  cmpq_mr = "cmpq %v2q, %v1q";
    cmp_mc = "cmp%s $%v2, %v1"; cmpq_mc = "cmpq $%v2q, %v1q";

    // Comparision + conditional jump
    add_comparison_conditional_jmp_rules(&ntc, REG, REG, cmp_rr);
    add_comparison_conditional_jmp_rules(&ntc, REG, CST, cmp_rc);
    add_comparison_conditional_jmp_rules(&ntc, REG, MEM, cmp_rm);
    add_comparison_conditional_jmp_rules(&ntc, MEM, REG, cmp_mr);
    add_comparison_conditional_jmp_rules(&ntc, MEM, CST, cmp_mc);

    // Comparision + conditional jump for pointers.
    add_comparison_conditional_jmp_rules(&ntc, REG, ADR, cmpq_rr);
    add_comparison_conditional_jmp_rules(&ntc, ADR, ADR, cmpq_rr);
    add_comparison_conditional_jmp_rules(&ntc, ADR, REG, cmpq_rr);
    add_comparison_conditional_jmp_rules(&ntc, ADR, CST, cmpq_rc);
    add_comparison_conditional_jmp_rules(&ntc, ADR, MEM, cmpq_rm);
    add_comparison_conditional_jmp_rules(&ntc, MEM, ADR, cmpq_mr);

    add_comparison_conditional_jmp_rules(&ntc, ADRV, CSTL, "cmpq $%v2l, %v1q");
    add_comparison_conditional_jmp_rules(&ntc, CSTL, ADRV, "cmpq $%v1l, %v2q");

    // Comparision + conditional assignment
    add_comparison_assignment_rules(REG, REG, cmp_rr);
    add_comparison_assignment_rules(REG, CST, cmp_rc);
    add_comparison_assignment_rules(REG, MEM, cmp_rm);
    add_comparison_assignment_rules(MEM, REG, cmp_mr);
    add_comparison_assignment_rules(MEM, CST, cmp_mc);

    add_comparison_assignment_rules(ADR,  CSTL, "cmpq $%v2, %v1q");
    add_comparison_assignment_rules(ADRV, CSTL, "cmpq $%v2, %v1q");
    add_comparison_assignment_rules(ADR,  ADRV, "cmpq %v2q, %v1q");
    add_comparison_assignment_rules(ADRV, ADR,  "cmpq %v2q, %v1q");
    add_comparison_assignment_rules(ADRV, ADRV, "cmpq %v2q, %v1q");

    add_commutative_operation_rules("add%s",  IR_ADD,  X_ADD,  10);
    add_commutative_operation_rules("imul%s", IR_MUL,  X_MUL,  30);
    add_commutative_operation_rules("or%s",   IR_BOR,  X_BOR,  3);
    add_commutative_operation_rules("and%s",  IR_BAND, X_BAND, 3);
    add_commutative_operation_rules("xor%s",  IR_XOR,  X_XOR,  3);

    add_sub_rules();
    add_div_rules();
    add_bnot_rules();
    add_binary_shift_rules();

    check_for_duplicate_rules();
}
