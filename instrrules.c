#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "wcc.h"

int disable_check_for_duplicate_rules;

static void add_move_rules_ri_to_ri() {
    Rule *r;

    r = add_rule(RI1, IR_MOVE, RI1, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(RI2, IR_MOVE, RI1, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbw %v1b, %vdw");
    r = add_rule(RI3, IR_MOVE, RI1, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbl %v1b, %vdl");
    r = add_rule(RI4, IR_MOVE, RI1, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbq %v1b, %vdq");
    r = add_rule(RI1, IR_MOVE, RI2, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(RI2, IR_MOVE, RI2, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(RI3, IR_MOVE, RI2, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movswl %v1w, %vdl");
    r = add_rule(RI4, IR_MOVE, RI2, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movswq %v1w, %vdq");
    r = add_rule(RI1, IR_MOVE, RI3, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(RI2, IR_MOVE, RI3, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(RI3, IR_MOVE, RI3, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(RI4, IR_MOVE, RI3, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movslq %v1l, %vdq");
    r = add_rule(RI1, IR_MOVE, RI4, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(RI2, IR_MOVE, RI4, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(RI3, IR_MOVE, RI4, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(RI4, IR_MOVE, RI4, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movq   %v1q, %vdq");
}

static void add_move_rules_ru_to_ru() {
    Rule *r;

    r = add_rule(RU1, IR_MOVE, RU1, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(RU2, IR_MOVE, RU1, 0, 1); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzbw %v1b, %vdw");
    r = add_rule(RU3, IR_MOVE, RU1, 0, 1); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzbl %v1b, %vdl");
    r = add_rule(RU4, IR_MOVE, RU1, 0, 1); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzbq %v1b, %vdq");
    r = add_rule(RU1, IR_MOVE, RU2, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(RU2, IR_MOVE, RU2, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(RU3, IR_MOVE, RU2, 0, 1); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzwl %v1w, %vdl");
    r = add_rule(RU4, IR_MOVE, RU2, 0, 1); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzwq %v1w, %vdq");
    r = add_rule(RU1, IR_MOVE, RU3, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(RU2, IR_MOVE, RU3, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(RU3, IR_MOVE, RU3, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(RU4, IR_MOVE, RU3, 0, 1); add_op(r, X_MOVZ, DST, SRC1, 0 , "movl   %v1l, %vdl"); // Note: movzlq doesn't exist
    r = add_rule(RU1, IR_MOVE, RU4, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(RU2, IR_MOVE, RU4, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(RU3, IR_MOVE, RU4, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(RU4, IR_MOVE, RU4, 0, 1); add_op(r, X_MOV , DST, SRC1, 0 , "movq   %v1q, %vdq");
}

static void add_move_rules_ri_to_mi() {
    Rule *r;

    r = add_rule(MI1, IR_MOVE, RI1, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(MI2, IR_MOVE, RI1, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbw %v1b, %v1w"); add_op(r, X_MOV, DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(MI3, IR_MOVE, RI1, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbl %v1b, %v1l"); add_op(r, X_MOV, DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(MI4, IR_MOVE, RI1, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbq %v1b, %v1q"); add_op(r, X_MOV, DST, SRC1, 0 , "movq   %v1q, %vdq");
    r = add_rule(MI1, IR_MOVE, RI2, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(MI2, IR_MOVE, RI2, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(MI3, IR_MOVE, RI2, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movswl %v1w, %v1l"); add_op(r, X_MOV, DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(MI4, IR_MOVE, RI2, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movswq %v1w, %v1q"); add_op(r, X_MOV, DST, SRC1, 0 , "movq   %v1q, %vdq");
    r = add_rule(MI1, IR_MOVE, RI3, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(MI2, IR_MOVE, RI3, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(MI3, IR_MOVE, RI3, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(MI4, IR_MOVE, RI3, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movslq %v1l, %v1q"); add_op(r, X_MOV, DST, SRC1, 0 , "movq   %v1q, %vdq");
    r = add_rule(MI1, IR_MOVE, RI4, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(MI2, IR_MOVE, RI4, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(MI3, IR_MOVE, RI4, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(MI4, IR_MOVE, RI4, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movq   %v1q, %vdq");
}

static void add_move_rules_ru_to_mu() {
    Rule *r;

    r = add_rule(MU1, IR_MOVE, RU1, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(MU2, IR_MOVE, RU1, 0, 2); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzbw %v1b, %v1w"); add_op(r, X_MOV, DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(MU3, IR_MOVE, RU1, 0, 2); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzbl %v1b, %v1l"); add_op(r, X_MOV, DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(MU4, IR_MOVE, RU1, 0, 2); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzbq %v1b, %v1q"); add_op(r, X_MOV, DST, SRC1, 0 , "movq   %v1q, %vdq");
    r = add_rule(MU1, IR_MOVE, RU2, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(MU2, IR_MOVE, RU2, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(MU3, IR_MOVE, RU2, 0, 2); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzwl %v1w, %v1l"); add_op(r, X_MOV, DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(MU4, IR_MOVE, RU2, 0, 2); add_op(r, X_MOVZ, DST, SRC1, 0 , "movzwq %v1w, %v1q"); add_op(r, X_MOV, DST, SRC1, 0 , "movq   %v1q, %vdq");
    r = add_rule(MU1, IR_MOVE, RU3, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(MU2, IR_MOVE, RU3, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(MU3, IR_MOVE, RU3, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(MU4, IR_MOVE, RU3, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0 , "movl   %v1l, %v1l"); add_op(r, X_MOV, DST, SRC1, 0 , "movq   %v1q, %vdq"); // Note: movzlq doesn't exist
    r = add_rule(MU1, IR_MOVE, RU4, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movb   %v1b, %vdb");
    r = add_rule(MU2, IR_MOVE, RU4, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movw   %v1w, %vdw");
    r = add_rule(MU3, IR_MOVE, RU4, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movl   %v1l, %vdl");
    r = add_rule(MU4, IR_MOVE, RU4, 0, 2);                                                        add_op(r, X_MOV, DST, SRC1, 0 , "movq   %v1q, %vdq");
}

static Rule *add_move_to_ptr(int src1, int src2, char *sign_extend_template, char *op_template) {
    Rule *r;

    r = add_rule(src1, IR_MOVE_TO_PTR, src1, src2, 3);
    if (sign_extend_template) add_op(r, X_MOVS, SRC2, SRC2, 0, sign_extend_template);
    add_op(r, X_MOV_TO_IND, 0,  SRC1,  SRC2, op_template);
    return r;
}

// Add rules for loads & address of from IR_BSHL + IR_ADD + IR_INDIRECT/IR_ADDRESS_OF
static void add_scaled_rule(int *ntc, int cst, int add_reg, int indirect_reg, int address_of_reg, int op, char *template) {
    int ntc1, ntc2;
    Rule *r;

    ntc1 = ++*ntc;
    ntc2 = ++*ntc;
    r = add_rule(ntc1, IR_BSHL, RI4, cst, 1); add_save_value(r, 1, 2); // Save index register to slot 2
    r = add_rule(ntc2, IR_ADD, add_reg, ntc1, 1); add_save_value(r, 1, 1); // Save address register to slot 1

    if (indirect_reg) r = add_rule(indirect_reg, IR_INDIRECT, ntc2, 0, 1);
    if (address_of_reg) r = add_rule(address_of_reg, IR_ADDRESS_OF, ntc2, 0, 1);
    add_load_value(r, 1, 1); // Load address register from slot 1
    add_load_value(r, 2, 2); // Load index register from slot 2
    add_op(r, op, DST, SRC1, 0, template);
}

static void add_offset_rule(int *ntc, int add_reg, int indirect_reg, char *template) {
    int ntc1;
    Rule *r;

    ntc1 = ++*ntc;
    r = add_rule(ntc1, IR_ADD, add_reg, CI4, 1);
    add_save_value(r, 1, 1); // Save address register to slot 1
    add_save_value(r, 2, 2); // Save offset register to slot 2
    r = add_rule(indirect_reg, IR_INDIRECT, ntc1, 0, 1);
    add_load_value(r, 1, 1); // Load address register from slot 1
    add_load_value(r, 2, 2); // Load index register from slot 2
    add_op(r, X_MOV_FROM_SCALED_IND, DST, SRC1, 0, template);
}

static void add_composite_pointer_rules(int *ntc) {
    // Loads
    add_scaled_rule(ntc, CSTV1, RP2, RI2, 0, X_MOV_FROM_SCALED_IND, "movw   (%v1q,%v2q,2), %vdw"); // from *short to short
    add_scaled_rule(ntc, CSTV1, RP2, RI3, 0, X_MOV_FROM_SCALED_IND, "movswl (%v1q,%v2q,2), %vdl"); // from *short to int
    add_scaled_rule(ntc, CSTV1, RP2, RI4, 0, X_MOV_FROM_SCALED_IND, "movswq (%v1q,%v2q,2), %vdq"); // from *short to long
    add_scaled_rule(ntc, CSTV2, RP3, RI3, 0, X_MOV_FROM_SCALED_IND, "movl   (%v1q,%v2q,4), %vdl"); // from *int to int
    add_scaled_rule(ntc, CSTV2, RP3, RI4, 0, X_MOV_FROM_SCALED_IND, "movslq (%v1q,%v2q,4), %vdq"); // from *int to long
    add_scaled_rule(ntc, CSTV3, RP4, RI4, 0, X_MOV_FROM_SCALED_IND, "movq   (%v1q,%v2q,8), %vdq"); // from *long to long
    add_scaled_rule(ntc, CSTV3, RP4, RP4, 0, X_MOV_FROM_SCALED_IND, "movq   (%v1q,%v2q,8), %vdq"); // from **

    add_offset_rule(ntc, RP1, RI1, "movb   %v2q(%v1q), %vdb"); // from struct member from *char -> char
    add_offset_rule(ntc, RP1, RI2, "movsbw %v2q(%v1q), %vdw"); // from struct member from *char -> short
    add_offset_rule(ntc, RP1, RI3, "movsbl %v2q(%v1q), %vdl"); // from struct member from *char -> int
    add_offset_rule(ntc, RP1, RI4, "movsbq %v2q(%v1q), %vdq"); // from struct member from *char -> long
    add_offset_rule(ntc, RP2, RI2, "movw   %v2q(%v1q), %vdw"); // from struct member from *short -> short
    add_offset_rule(ntc, RP2, RI3, "movswl %v2q(%v1q), %vdl"); // from struct member from *short -> int
    add_offset_rule(ntc, RP2, RI4, "movswq %v2q(%v1q), %vdq"); // from struct member from *short -> long
    add_offset_rule(ntc, RP3, RI3, "movl   %v2q(%v1q), %vdl"); // from struct member from *int -> int
    add_offset_rule(ntc, RP3, RI4, "movslq %v2q(%v1q), %vdq"); // from struct member from *int -> long
    add_offset_rule(ntc, RP4, RI4, "movq   %v2q(%v1q), %vdq"); // from struct member from *long -> long

    add_offset_rule(ntc, RP4, RP1, "movq   %v2q(%v1q), %vdq"); // from ** to *char
    add_offset_rule(ntc, RP4, RP2, "movq   %v2q(%v1q), %vdq"); // from ** to *short
    add_offset_rule(ntc, RP4, RP3, "movq   %v2q(%v1q), %vdq"); // from ** to *int
    add_offset_rule(ntc, RP4, RP4, "movq   %v2q(%v1q), %vdq"); // from ** to *long

    // Address of
    add_scaled_rule(ntc, CSTV1, RP4, 0, RP4, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,2), %vdq");
    add_scaled_rule(ntc, CSTV2, RP4, 0, RP4, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,4), %vdq");
    add_scaled_rule(ntc, CSTV3, RP4, 0, RP4, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,8), %vdq");
}

static void add_pointer_rules(int *ntc) {
    int src, dst;
    Rule *r;

    for (dst = RP1; dst <= RP4; dst++)
        for (src = RP1; src <= RP4; src++) {
            r = add_rule(dst,  IR_MOVE, src, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
        }

    r = add_rule(MI4, IR_MOVE,       XRP, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, IR_ADDRESS_OF, XRP, 0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r); // & for *&*&*pi
    r = add_rule(RP4, IR_MOVE,       MI4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(XRP, 0,             MI4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);

    // Address loads
    r = add_rule(RP1, 0,             STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(RP1, IR_MOVE,       STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(XRP, IR_ADDRESS_OF, XMI,  0, 2); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); fin_rule(r);

    // Crazy shit for some code that uses a (long *) to store pointers to longs.
    // The sane code should have been using (long **)
    // long *sp; (*((long *) *sp))++ = a;
    r = add_rule(RP4, IR_MOVE, RI4, 0, 1); add_op(r, X_MOV, DST, SRC1, 0 , "movq %v1q, %vdq");

    // Loads from pointer
    r = add_rule(RI1, IR_INDIRECT, RP1, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movb   (%v1q), %vdb");
    r = add_rule(RI2, IR_INDIRECT, RP1, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movsbw (%v1q), %vdw");
    r = add_rule(RI3, IR_INDIRECT, RP1, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movsbl (%v1q), %vdl");
    r = add_rule(RI4, IR_INDIRECT, RP1, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movsbq (%v1q), %vdq");
    r = add_rule(RI2, IR_INDIRECT, RP2, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movw   (%v1q), %vdw");
    r = add_rule(RI3, IR_INDIRECT, RP2, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movswl (%v1q), %vdl");
    r = add_rule(RI4, IR_INDIRECT, RP2, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movswq (%v1q), %vdq");
    r = add_rule(RI3, IR_INDIRECT, RP3, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movl   (%v1q), %vdl");
    r = add_rule(RI4, IR_INDIRECT, RP3, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movslq (%v1q), %vdq");
    r = add_rule(RI4, IR_INDIRECT, RP4, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq");

    for (dst = RP1; dst <= RP4; dst++) {
        r = add_rule(dst, IR_INDIRECT, RP4, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movq   (%v1q), %vdq");
    }

    add_composite_pointer_rules(ntc);

    // Stores of a pointer to a pointer
    for (dst = RP1; dst <= RP4; dst++)
        for (src = RP1; src <= RP4; src++)
            add_move_to_ptr(dst, src, 0, "movq %v2q, (%v1q)");

    // Stores to a pointer of differing size
    add_move_to_ptr(RP1, RI1, 0,                   "movb %v2b, (%v1q)");
    add_move_to_ptr(RP2, RI1, "movsbw %v1b, %vdw", "movw %v2w, (%v1q)");
    add_move_to_ptr(RP3, RI1, "movsbl %v1b, %vdl", "movl %v2l, (%v1q)");
    add_move_to_ptr(RP4, RI1, "movsbq %v1b, %vdq", "movq %v2q, (%v1q)");
    add_move_to_ptr(RP1, RI2, 0,                   "movb %v2b, (%v1q)");
    add_move_to_ptr(RP2, RI2, 0,                   "movw %v2w, (%v1q)");
    add_move_to_ptr(RP3, RI2, "movswl %v1w, %vdl", "movl %v2l, (%v1q)");
    add_move_to_ptr(RP4, RI2, "movswq %v1w, %vdq", "movq %v2q, (%v1q)");
    add_move_to_ptr(RP1, RI3, 0,                   "movb %v2b, (%v1q)");
    add_move_to_ptr(RP2, RI3, 0,                   "movw %v2w, (%v1q)");
    add_move_to_ptr(RP3, RI3, 0,                   "movl %v2l, (%v1q)");
    add_move_to_ptr(RP4, RI3, "movslq %v1l, %vdq", "movq %v2q, (%v1q)");
    add_move_to_ptr(RP1, RI4, 0,                   "movb %v2b, (%v1q)");
    add_move_to_ptr(RP2, RI4, 0,                   "movw %v2w, (%v1q)");
    add_move_to_ptr(RP3, RI4, 0,                   "movl %v2l, (%v1q)");
    add_move_to_ptr(RP4, RI4, 0,                   "movq %v2q, (%v1q)");

    add_move_to_ptr(RP1, CI1, 0, "movb $%v2b, (%v1q)");
    add_move_to_ptr(RP2, CI2, 0, "movw $%v2w, (%v1q)");
    add_move_to_ptr(RP3, CI3, 0, "movl $%v2l, (%v1q)");
    add_move_to_ptr(RP4, CI4, 0, "movq $%v2q, (%v1q)");
}

static void add_conditional_zero_jump_rule(int operation, int src1, int src2, int cost, int x86_cmp_operation, char *comparison, char *conditional_jmp, int do_fin_rule) {
    int x86_jmp_operation;
    Rule *r;

    x86_jmp_operation = operation == IR_JZ ? X_JZ : X_JNZ;

    r = add_rule(0, operation, src1, src2, cost);
    add_op(r, x86_cmp_operation, 0, SRC1, 0, comparison);
    add_op(r, x86_jmp_operation, 0, SRC2, 0, conditional_jmp);
    if (do_fin_rule) fin_rule(r);
}

static void add_comparison_conditional_jmp_rule(int *ntc, int src1, int src2, char *template, int op, int x86op1, char *t1, int x86op2, char *t2) {
    Rule *r;

    (*ntc)++;
    r = add_rule(*ntc, op,     src1, src2, 10); add_op(r, X_CMP,  0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, x86op1, 0, SRC2, 0,    t1); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, x86op2, 0, SRC2, 0,    t2); fin_rule(r);
}

static void add_comparison_conditional_jmp_rules(int *ntc, int is_unsigned, int src1, int src2, char *template) {
    add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_EQ, X_JE,  "je %v1",  X_JNE, "jne %v1");
    add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_NE, X_JNE, "jne %v1", X_JE,  "je %v1");

    if (is_unsigned) {
        add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_LT, X_JB,  "jb %v1" , X_JAE, "jae %v1" );
        add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_GT, X_JA,  "ja %v1",  X_JBE, "jbe %v1");
        add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_LE, X_JBE, "jbe %v1", X_JA,  "ja %v1");
        add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_GE, X_JAE, "jae %v1", X_JB,  "jb %v1");
    }
    else {
        add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_LT, X_JLT, "jl %v1" , X_JGE, "jge %v1" );
        add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_GT, X_JGT, "jg %v1",  X_JLE, "jle %v1");
        add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_LE, X_JLE, "jle %v1", X_JGT, "jg %v1");
        add_comparison_conditional_jmp_rule(ntc, src1, src2, template, IR_GE, X_JGE, "jge %v1", X_JLT, "jl %v1");
    }
}

static void add_comparison_assignment_rule(int src1, int src2, char *cmp_template, int operation, int set_operation, char *set_template) {
    int i;
    Rule *r;

    for (i = 1; i <= 4; i++) {
        r = add_rule(RI1 + i - 1, operation, src1, src2, 12 + (i > 1));

        add_op(r, X_CMP,         0,   SRC1, SRC2, cmp_template);
        add_op(r, set_operation, DST, 0,    0,    set_template);

             if (i == 2) add_op(r, X_MOVZ, DST, DST, 0, "movzbw %v1b, %v1w");
        else if (i == 3) add_op(r, X_MOVZ, DST, DST, 0, "movzbl %v1b, %v1l");
        else if (i == 4) add_op(r, X_MOVZ, DST, DST, 0, "movzbq %v1b, %v1q");
        fin_rule(r);
    }
}

static void add_comparison_assignment_rules(int is_unsigned, int src1, int src2, char *template) {
    add_comparison_assignment_rule(src1, src2, template, IR_EQ, X_SETE,  "sete %vdb");
    add_comparison_assignment_rule(src1, src2, template, IR_NE, X_SETNE, "setne %vdb");

    if (is_unsigned) {
        add_comparison_assignment_rule(src1, src2, template, IR_LT, X_SETB,  "setb %vdb");
        add_comparison_assignment_rule(src1, src2, template, IR_GT, X_SETA,  "seta %vdb");
        add_comparison_assignment_rule(src1, src2, template, IR_LE, X_SETBE, "setbe %vdb");
        add_comparison_assignment_rule(src1, src2, template, IR_GE, X_SETAE, "setae %vdb");
    }
    else {
        add_comparison_assignment_rule(src1, src2, template, IR_LT, X_SETLT, "setl %vdb");
        add_comparison_assignment_rule(src1, src2, template, IR_GT, X_SETGT, "setg %vdb");
        add_comparison_assignment_rule(src1, src2, template, IR_LE, X_SETLE, "setle %vdb");
        add_comparison_assignment_rule(src1, src2, template, IR_GE, X_SETGE, "setge %vdb");
    }
}

static void add_commutative_operation_rule(int operation, int x86_mov_operation, int x86_operation, int dst, int src1, int src2, int cost, char *mov_template, char *op_template) {
    Rule *r;

    r = add_rule(dst, operation, src1, src2, cost);
    add_op(r, x86_mov_operation, DST, SRC2, 0,   mov_template);
    add_op(r, x86_operation,     DST, SRC1, DST, op_template);
    fin_rule(r);
}

static void add_bi_directional_commutative_operation_rules(int operation, int x86_mov_operation, int x86_operation, int dst, int src1, int src2, int cost, char *mov_template, char *op_template) {
    Rule *r;

    r = add_rule(dst, operation, src1, src2, cost); add_op(r, x86_mov_operation, DST, SRC2, 0,   mov_template);
                                                    add_op(r, x86_operation,     DST, SRC1, DST, op_template);
                                                    fin_rule(r);
    r = add_rule(dst, operation, src2, src1, cost); add_op(r, x86_mov_operation, DST, SRC1, 0,   mov_template);
                                                    add_op(r, x86_operation,     DST, SRC2, DST, op_template);
                                                    fin_rule(r);
}

static void add_commutative_operation_rules(char *x86_operand, int operation, int x86_operation, int cost) {
    char *op_vv, *op_cv;
    int i;

    asprintf(&op_vv, "%s %%v1, %%v2",  x86_operand); // Perform operation on two registers or memory
    asprintf(&op_cv, "%s $%%v1, %%v2", x86_operand); // Perform operation on a constant and a register

    for (i = RI1; i <= RI4; i++) {
        add_commutative_operation_rule(operation, X_MOV, x86_operation, i, XRI, XRI, cost, "mov%s %v1, %vd", op_vv);
        add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, i, XCI, XRI, cost, "mov%s %v1, %vd", op_cv);
        add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, i, XRI, XMI, cost + 1, "mov%s %v1, %vd", op_vv);
        add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, i, XMI, XCI, cost + 1, "mov%s $%v1, %vd", op_vv);
    }
}

static void add_pointer_plus_int_rule(int dst, int src, int cost, int x86_operation, char *sign_extend_template) {
    Rule *r;

    r = add_rule(dst, IR_ADD, dst, src, cost);
    add_op(r, X_MOV,         DST,  SRC1, 0,   "movq %v1q, %vdq");
    if (sign_extend_template) add_op(r, X_MOVS,        SRC2, SRC2, 0,   sign_extend_template);
    add_op(r, x86_operation, DST,  SRC2, DST, "addq %v1q, %v2q");
}

static void add_pointer_add_rules() {
    int i;

    for (i = RP1; i <= RP4; i++) {
        add_pointer_plus_int_rule(i, RI1, 11, X_ADD, "movsbq %v1b, %vdq");
        add_pointer_plus_int_rule(i, RI2, 11, X_ADD, "movswq %v1w, %vdq");
        add_pointer_plus_int_rule(i, RI3, 11, X_ADD, "movslq %v1l, %vdq");
        add_pointer_plus_int_rule(i, RI4, 10, X_ADD, 0);
    }

    for (i = CI1; i <= CI4; i++)
        add_bi_directional_commutative_operation_rules(IR_ADD, X_MOV, X_ADD, XRP, i, XRP, 10, "movq %v1q, %vdq", "addq $%v1q, %v2q");
}

static void add_sub_rule(int dst, int src1, int src2, int cost, char *mov_template, char *sign_extend_template, char *sub_template) {
    Rule *r;

    r = add_rule(dst, IR_SUB, src1, src2, cost);
    add_op(r, X_MOV, DST, SRC1, 0,   mov_template);
    if (sign_extend_template) add_op(r, X_MOVS,        SRC2, SRC2, 0,   sign_extend_template);
    add_op(r, X_SUB, DST, SRC2, DST, sub_template);
    fin_rule(r);
}

static void add_sub_rules() {
    int i, j;

    for (i = CI1; i <= CI4; i++) {
        add_sub_rule(XRI, i,   XRI,  10, "mov%s $%v1, %vd", 0, "sub%s %v1, %vd");
        add_sub_rule(XRI, XRI, i,    10, "mov%s %v1, %vd",  0, "sub%s $%v1, %vd");
        add_sub_rule(XRP, XRP, i,    10, "movq %v1q, %vdq", 0, "subq $%v1q, %vdq");
        add_sub_rule(XRI, i,   XMI,  11, "mov%s $%v1, %vd", 0, "sub%s %v1, %vd");
        add_sub_rule(XRI, XMI, i,    11, "mov%s %v1, %vd",  0, "sub%s $%v1, %vd");
    }

    add_sub_rule(XRI, XRI, XRI,  10, "mov%s %v1, %vd",  0, "sub%s %v1, %vd");
    add_sub_rule(XRI, XRI, XMI,  11, "mov%s %v1, %vd",  0, "sub%s %v1, %vd");
    add_sub_rule(XRI, XMI, XRI,  11, "mov%s %v1, %vd",  0, "sub%s %v1, %vd");

    // Pointer - int subtraction
    for (i = RP1; i <= RP4; i++) {
        add_sub_rule(i, i, RI1, 11, "movq %v1q, %vdq", "movsbq %v1b, %v1q", "subq %v1q, %vdq");
        add_sub_rule(i, i, RI2, 11, "movq %v1q, %vdq", "movswq %v1w, %v1q", "subq %v1q, %vdq");
        add_sub_rule(i, i, RI3, 11, "movq %v1q, %vdq", "movslq %v1l, %v1q", "subq %v1q, %vdq");
        add_sub_rule(i, i, RI4, 10, "movq %v1q, %vdq", 0,                   "subq %v1q, %vdq");
    }

    for (i = RP1; i <= RP4; i++)
        for (j = RP1; j <= RP4; j++)
            add_sub_rule(RI4, i, j,  10, "movq %v1q, %vdq", 0, "subq %v1q, %vdq");
}

static void add_div_rule(int dst, int src1, int src2, int cost, char *t1, char *t2, char *t3, char *t4, char *tdiv, char *tmod) {
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

static void add_div_rules() {
    add_div_rule(RI3, RI3, RI3, 40, "movl %v1l, %%eax", "cltd", "movl %v1l, %vdl", "idivl %vdl", "movl %%eax, %vdl", "movl %%edx, %vdl");
    add_div_rule(RI4, RI4, RI4, 50, "movq %v1q, %%rax", "cqto", "movq %v1q, %vdq", "idivq %vdq", "movq %%rax, %vdq", "movq %%rdx, %vdq");
}

static void add_bnot_rules() {
    Rule *r;

    r = add_rule(XRI, IR_BNOT, XRI, 0, 3); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd");
                                           add_op(r, X_BNOT, DST, DST,  0, "not%s %vd");
                                           fin_rule(r);
    r = add_rule(XRI, IR_BNOT, XMI, 0, 3); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd");
                                           add_op(r, X_BNOT, DST, DST,  0, "not%s %vd");
                                           fin_rule(r);
}

static void add_binary_shift_rule(int src2, char *template) {
    Rule *r;

    r = add_rule(XRI, IR_BSHL, XRI, src2, 4); add_op(r, X_MOV, DST, SRC2, 0, template);
                                              add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                              add_op(r, X_SHL, DST, SRC2, 0, "shl%s %%cl, %vd");
                                              fin_rule(r);
    r = add_rule(XRI, IR_BSHR, XRI, src2, 4); add_op(r, X_MOV, DST, SRC2, 0, template);
                                              add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                              add_op(r, X_SAR, DST, SRC2, 0, "sar%s %%cl, %vd");
                                              fin_rule(r);
}

static void add_binary_shift_rules() {
    Rule *r;

    r = add_rule(XRI, IR_BSHL, XRI, XCI, 3); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SHL, DST, SRC2, 0, "shl%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(XRI, IR_BSHR, XRI, XCI, 3); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SAR, DST, SRC2, 0, "sar%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(XRI, IR_BSHL, XMI, XCI, 4); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SHL, DST, SRC2, 0, "shl%s $%v1, %vd");
                                             fin_rule(r);
    r = add_rule(XRI, IR_BSHR, XMI, XCI, 4); add_op(r, X_MOV, DST, SRC1, 0, "mov%s %v1, %vd");
                                             add_op(r, X_SAR, DST, SRC2, 0, "sar%s $%v1, %vd");
                                             fin_rule(r);

    add_binary_shift_rule(RI1, "movb %v1b, %%cl");
    add_binary_shift_rule(RI2, "movw %v1w, %%cx");
    add_binary_shift_rule(RI3, "movl %v1l, %%ecx");
    add_binary_shift_rule(RI4, "movq %v1q, %%rcx");
}

static X86Operation *add_function_call_arg_op(Rule *r) {
    add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q");
}

void init_instruction_selection_rules() {
    Rule *r;
    int ntc; // Non terminal counter
    int i;
    char *cmp_vv;
    char *cmpq_vv, *cmpq_vc;

    instr_rule_count = 0;
    disable_merge_constants = 0;
    rule_coverage_file = 0;

    instr_rules = malloc(MAX_RULE_COUNT * sizeof(Rule));
    memset(instr_rules, 0, MAX_RULE_COUNT * sizeof(Rule));

    ntc = AUTO_NON_TERMINAL_START;

    // Identity rules, for matching leaf nodes in the instruction tree
    r = add_rule(XC,    0, XC,    0, 0); fin_rule(r);
    r = add_rule(XR,    0, XR,    0, 0); fin_rule(r);
    r = add_rule(CSTV1, 0, CSTV1, 0, 0);
    r = add_rule(CSTV2, 0, CSTV2, 0, 0);
    r = add_rule(CSTV3, 0, CSTV3, 0, 0);
    r = add_rule(XMI,   0, XMI,   0, 0); fin_rule(r);
    r = add_rule(XRP,   0, XRP,   0, 0); fin_rule(r);
    r = add_rule(RI4,   0, RP4,   0, 0);
    r = add_rule(STL,   0, STL,   0, 0);
    r = add_rule(LAB,   0, LAB,   0, 0);
    r = add_rule(FUN,   0, FUN,   0, 0);

    add_move_rules_ri_to_ri();
    add_move_rules_ru_to_ru();
    add_move_rules_ri_to_mi();
    add_move_rules_ru_to_mu();

    // Register -> register sign extension for precision increases
    r = add_rule(RI2, 0, RI1, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbw %v1b, %vdw");
    r = add_rule(RI3, 0, RI1, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbl %v1b, %vdl");
    r = add_rule(RI4, 0, RI1, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbq %v1b, %vdq");
    r = add_rule(RI3, 0, RI2, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movswl %v1w, %vdl");
    r = add_rule(RI4, 0, RI2, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movswq %v1w, %vdq");
    r = add_rule(RI4, 0, RI3, 0, 1); add_op(r, X_MOVS, DST, SRC1, 0 , "movslq %v1l, %vdq");

    // Memory -> register sign extension
    r = add_rule(RI2, 0, MI1, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbw %v1b, %vdw");
    r = add_rule(RI3, 0, MI1, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbl %v1b, %vdl");
    r = add_rule(RI4, 0, MI1, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movsbq %v1b, %vdq");
    r = add_rule(RI3, 0, MI2, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movswl %v1w, %vdl");
    r = add_rule(RI4, 0, MI2, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movswq %v1w, %vdq");
    r = add_rule(RI4, 0, MI3, 0, 2); add_op(r, X_MOVS, DST, SRC1, 0 , "movslq %v1l, %vdq");

    for (i = 0; i < 4; i++) {
        r = add_rule(XRI, IR_MOVE, CI1 + i, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
        r = add_rule(XRI, IR_MOVE, CU1 + i, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
        r = add_rule(XRU, IR_MOVE, CI1 + i, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
        r = add_rule(XRU, IR_MOVE, CU1 + i, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
        r = add_rule(XMI, IR_MOVE, CI1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
        r = add_rule(XMI, IR_MOVE, CU1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
        r = add_rule(XMU, IR_MOVE, CI1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
        r = add_rule(XMU, IR_MOVE, CU1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
    }

    r = add_rule(XRP, IR_MOVE, CI4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq"); fin_rule(r);

    add_pointer_rules(&ntc);

    r = add_rule(XR1, 0, XC1, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movb $%v1b, %vdb"); fin_rule(r);
    r = add_rule(XR2, 0, XC2, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movw $%v1w, %vdw"); fin_rule(r);
    r = add_rule(XR3, 0, XC3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movl $%v1l, %vdl"); fin_rule(r);
    r = add_rule(XR4, 0, XC4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq"); fin_rule(r);

    r = add_rule(RI1, IR_MOVE, XRP, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(RI2, IR_MOVE, XRP, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(RI3, IR_MOVE, XRP, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(RI4, IR_MOVE, XRP, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);

    r = add_rule(XRI,  0,       XMI,  0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r); // Load temp memory into register
    r = add_rule(XRI,  IR_MOVE, XMI,  0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r); // Load standalone memory into register

    r = add_rule(XRI,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r); // Function call with a return value
    r = add_rule(XRP,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r); // Function call with a return value

    r = add_rule(0, IR_ARG, CI4, XC, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq $%v2q"); fin_rule(r);

    // Add rules for sign extention an arg, but at a high cost, to encourage other rules to take precedence
    r = add_rule(0, IR_ARG, CI4, RI1, 10); add_op(r, X_MOVS, SRC2, SRC2, 0 , "movsbq %v1b, %v1q"); add_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RI2, 10); add_op(r, X_MOVS, SRC2, SRC2, 0 , "movswq %v1w, %v1q"); add_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RI3, 10); add_op(r, X_MOVS, SRC2, SRC2, 0 , "movslq %v1l, %v1q"); add_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RI4, 2);                                                          add_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RP1, 2);                                                          add_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RP2, 2);                                                          add_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RP3, 2);                                                          add_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RP4, 2);                                                          add_function_call_arg_op(r);

    r = add_rule(0,   IR_RETURN, 0,    0, 1); add_op(r, X_RET,  0,     0,    0, 0                 ); fin_rule(r);
    r = add_rule(RI1, IR_RETURN, XCI,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movb $%v1b, %vdb"); fin_rule(r);
    r = add_rule(RI2, IR_RETURN, XCI,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movw $%v1w, %vdw"); fin_rule(r);
    r = add_rule(RI3, IR_RETURN, XCI,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movl $%v1l, %vdl"); fin_rule(r);
    r = add_rule(RI4, IR_RETURN, XCI,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movq $%v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, IR_RETURN, XCI,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movq $%v1q, %vdq"); fin_rule(r);
    r = add_rule(XRI, IR_RETURN, RI1,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r);
    r = add_rule(XRI, IR_RETURN, RI2,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r);
    r = add_rule(XRI, IR_RETURN, RI3,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r);
    r = add_rule(XRI, IR_RETURN, RI4,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "mov%s %v1, %vd"  ); fin_rule(r);
    r = add_rule(XRP, IR_RETURN, XRP,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movq %v1q, %vdq" ); fin_rule(r);
    r = add_rule(RP1, IR_RETURN, RP4,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movq %v1q, %vdq" );
    r = add_rule(RP2, IR_RETURN, RP4,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movq %v1q, %vdq" );
    r = add_rule(RP3, IR_RETURN, RP4,  0, 1); add_op(r, X_RET,  DST,   SRC1, 0, "movq %v1q, %vdq" );

    // Jump rules
    r = add_rule(0, IR_JMP, LAB, 0,1);  add_op(r, X_JMP, 0, SRC1, 0, "jmp %v1"); fin_rule(r);

    add_conditional_zero_jump_rule(IR_JZ,  XR,  LAB, 3, X_TEST, "test%s %v1, %v1",  "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  XRP, LAB, 3, X_TEST, "testq %v1q, %v1q", "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  XMI, LAB, 3, X_CMPZ, "cmp $0, %v1",      "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JNZ, XRI, LAB, 3, X_TEST, "test%s %v1, %v1",  "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, XRP, LAB, 3, X_TEST, "testq %v1q, %v1q", "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, XMI, LAB, 3, X_CMPZ, "cmp $0, %v1",      "jnz %v1",  1);

    cmp_vv = "cmp%s %v2, %v1";
    cmpq_vv = "cmpq %v2q, %v1q";
    cmpq_vc = "cmpq $%v2q, %v1q";

    // immediate comparisons can only be done on 32 bit integers
    // https://www.felixcloutier.com/x86/cmp

    // Comparision + conditional jump
    add_comparison_conditional_jmp_rules(&ntc, 0, XRP, XRP, cmpq_vv);

    add_comparison_conditional_jmp_rules(&ntc, 0, XRI, XRI, cmp_vv);            add_comparison_conditional_jmp_rules(&ntc, 1, XRU, XRU, cmp_vv);
    add_comparison_conditional_jmp_rules(&ntc, 0, XRI, XMI, cmp_vv);            add_comparison_conditional_jmp_rules(&ntc, 1, XRU, XMU, cmp_vv);
    add_comparison_conditional_jmp_rules(&ntc, 0, XMI, XRI, cmp_vv);            add_comparison_conditional_jmp_rules(&ntc, 1, XMU, XRU, cmp_vv);
    add_comparison_conditional_jmp_rules(&ntc, 0, XRI, XRP, cmpq_vv);           add_comparison_conditional_jmp_rules(&ntc, 1, XRU, XRP, cmpq_vv);
    add_comparison_conditional_jmp_rules(&ntc, 0, XRP, XRI, cmpq_vv);           add_comparison_conditional_jmp_rules(&ntc, 1, XRP, XRU, cmpq_vv);
    add_comparison_conditional_jmp_rules(&ntc, 0, XRP, XCI, cmpq_vc);           add_comparison_conditional_jmp_rules(&ntc, 1, XRP, XCU, cmpq_vc);
    add_comparison_conditional_jmp_rules(&ntc, 0, XRP, XMI, cmpq_vv);           add_comparison_conditional_jmp_rules(&ntc, 1, XRP, XMU, cmpq_vv);
    add_comparison_conditional_jmp_rules(&ntc, 0, XMI, XRP, cmpq_vv);           add_comparison_conditional_jmp_rules(&ntc, 1, XMU, XRP, cmpq_vv);
    add_comparison_conditional_jmp_rules(&ntc, 0, RI1, CI1, "cmpb $%v2, %v1b"); add_comparison_conditional_jmp_rules(&ntc, 1, RU1, CU1, "cmpb $%v2, %v1b");
    add_comparison_conditional_jmp_rules(&ntc, 0, RI2, CI2, "cmpw $%v2, %v1w"); add_comparison_conditional_jmp_rules(&ntc, 1, RU2, CU2, "cmpw $%v2, %v1w");
    add_comparison_conditional_jmp_rules(&ntc, 0, RI3, CI3, "cmpl $%v2, %v1l"); add_comparison_conditional_jmp_rules(&ntc, 1, RU3, CU3, "cmpl $%v2, %v1l");
    add_comparison_conditional_jmp_rules(&ntc, 0, RI4, CI3, "cmpq $%v2, %v1q"); add_comparison_conditional_jmp_rules(&ntc, 1, RU4, CU3, "cmpq $%v2, %v1q");
    add_comparison_conditional_jmp_rules(&ntc, 0, MI1, CI1, "cmpb $%v2, %v1b"); add_comparison_conditional_jmp_rules(&ntc, 1, MU1, CU1, "cmpb $%v2, %v1b");
    add_comparison_conditional_jmp_rules(&ntc, 0, MI2, CI2, "cmpw $%v2, %v1w"); add_comparison_conditional_jmp_rules(&ntc, 1, MU2, CU2, "cmpw $%v2, %v1w");
    add_comparison_conditional_jmp_rules(&ntc, 0, MI3, CI3, "cmpl $%v2, %v1l"); add_comparison_conditional_jmp_rules(&ntc, 1, MU3, CU3, "cmpl $%v2, %v1l");
    add_comparison_conditional_jmp_rules(&ntc, 0, MI4, CI3, "cmpq $%v2, %v1q"); add_comparison_conditional_jmp_rules(&ntc, 1, MU4, CU3, "cmpq $%v2, %v1q");

    // Comparision + conditional assignment
    add_comparison_assignment_rules(0, XRP, XRP, cmpq_vv);

    add_comparison_assignment_rules(0, XRI, XRI, cmp_vv);            add_comparison_assignment_rules(1, XRU, XRU, cmp_vv);
    add_comparison_assignment_rules(0, XRI, XMI, cmp_vv);            add_comparison_assignment_rules(1, XRU, XMU, cmp_vv);
    add_comparison_assignment_rules(0, XMI, XRI, cmp_vv);            add_comparison_assignment_rules(1, XMU, XRU, cmp_vv);
    add_comparison_assignment_rules(0, XRP, XCI, cmpq_vc);           add_comparison_assignment_rules(1, XRP, XCU, cmpq_vc);
    add_comparison_assignment_rules(0, RI1, CI1, "cmpb $%v2, %v1b"); add_comparison_assignment_rules(1, RU1, CU1, "cmpb $%v2, %v1b");
    add_comparison_assignment_rules(0, RI2, CI2, "cmpw $%v2, %v1w"); add_comparison_assignment_rules(1, RU2, CU2, "cmpw $%v2, %v1w");
    add_comparison_assignment_rules(0, RI3, CI3, "cmpl $%v2, %v1l"); add_comparison_assignment_rules(1, RU3, CU3, "cmpl $%v2, %v1l");
    add_comparison_assignment_rules(0, RI4, CI3, "cmpq $%v2, %v1q"); add_comparison_assignment_rules(1, RU4, CU3, "cmpq $%v2, %v1q");
    add_comparison_assignment_rules(0, MI1, CI1, "cmpb $%v2, %v1b"); add_comparison_assignment_rules(1, MU1, CU1, "cmpb $%v2, %v1b");
    add_comparison_assignment_rules(0, MI2, CI2, "cmpw $%v2, %v1w"); add_comparison_assignment_rules(1, MU2, CU2, "cmpw $%v2, %v1w");
    add_comparison_assignment_rules(0, MI3, CI3, "cmpl $%v2, %v1l"); add_comparison_assignment_rules(1, MU3, CU3, "cmpl $%v2, %v1l");
    add_comparison_assignment_rules(0, MI4, CI3, "cmpq $%v2, %v1q"); add_comparison_assignment_rules(1, MU4, CU3, "cmpq $%v2, %v1q");

    // Operations
    add_commutative_operation_rules("add%s",  IR_ADD,  X_ADD,  10);
    add_commutative_operation_rules("imul%s", IR_MUL,  X_MUL,  30);
    add_commutative_operation_rules("or%s",   IR_BOR,  X_BOR,  3);
    add_commutative_operation_rules("and%s",  IR_BAND, X_BAND, 3);
    add_commutative_operation_rules("xor%s",  IR_XOR,  X_XOR,  3);

    add_pointer_add_rules();
    add_sub_rules();
    add_div_rules();
    add_bnot_rules();
    add_binary_shift_rules();

    if (ntc >= AUTO_NON_TERMINAL_END)
        panic2d("terminal rules exceeded: %d > %d\n", ntc, AUTO_NON_TERMINAL_END);

    if (!disable_check_for_duplicate_rules) check_for_duplicate_rules();

    rule_coverage = new_set(instr_rule_count - 1);
}
