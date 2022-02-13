#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "wcc.h"

int disable_check_for_duplicate_rules;
char **signed_moves_templates, **unsigned_moves_templates;
int *signed_moves_operations, *unsigned_moves_operations;

static void add_mov_rule(int dst, int src, int operation, char *template) {
    int src_size = make_x86_size_from_non_terminal(src) - 1;
    int dst_size = make_x86_size_from_non_terminal(dst) - 1;

    int is_signed = src == RI1 || src == RI2 || src == RI3 || src == RI4;
    char **moves_templates = (is_signed) ? signed_moves_templates : unsigned_moves_templates;
    int *moves_operations = (is_signed) ? signed_moves_operations : unsigned_moves_operations;

    if (!template) template = moves_templates[src_size * 4 + dst_size];
    if (!operation) operation = moves_operations[src_size * 4 + dst_size];

    Rule *r = add_rule(dst,  IR_MOVE, src, 0, 1);
    add_op(r, operation, DST, SRC1, 0, template);
}

static void init_moves_templates(void) {
    signed_moves_templates = wmalloc(sizeof(char *) * 16);
    unsigned_moves_templates = wmalloc(sizeof(char *) * 16);

    signed_moves_operations = wmalloc(sizeof(int) * 16);
    unsigned_moves_operations = wmalloc(sizeof(int) * 16);

    // A move from src->dst. Index is src * 4 + dst
    signed_moves_templates[0 ] = "movb   %v1b, %vdb"; // 1 -> 1
    signed_moves_templates[1 ] = "movsbw %v1b, %vdw"; // 1 -> 2
    signed_moves_templates[2 ] = "movsbl %v1b, %vdl"; // 1 -> 3
    signed_moves_templates[3 ] = "movsbq %v1b, %vdq"; // 1 -> 4
    signed_moves_templates[4 ] = "movb   %v1b, %vdb"; // 2 -> 1
    signed_moves_templates[5 ] = "movw   %v1w, %vdw"; // 2 -> 2
    signed_moves_templates[6 ] = "movswl %v1w, %vdl"; // 2 -> 3
    signed_moves_templates[7 ] = "movswq %v1w, %vdq"; // 2 -> 4
    signed_moves_templates[8 ] = "movb   %v1b, %vdb"; // 3 -> 1
    signed_moves_templates[9 ] = "movw   %v1w, %vdw"; // 3 -> 2
    signed_moves_templates[10] = "movl   %v1l, %vdl"; // 3 -> 3
    signed_moves_templates[11] = "movslq %v1l, %vdq"; // 3 -> 4
    signed_moves_templates[12] = "movb   %v1b, %vdb"; // 4 -> 1
    signed_moves_templates[13] = "movw   %v1w, %vdw"; // 4 -> 2
    signed_moves_templates[14] = "movl   %v1l, %vdl"; // 4 -> 3
    signed_moves_templates[15] = "movq   %v1q, %vdq"; // 4 -> 4

    unsigned_moves_templates[0 ] = "movb   %v1b, %vdb"; // 1 -> 1
    unsigned_moves_templates[1 ] = "movzbw %v1b, %vdw"; // 1 -> 2
    unsigned_moves_templates[2 ] = "movzbl %v1b, %vdl"; // 1 -> 3
    unsigned_moves_templates[3 ] = "movzbq %v1b, %vdq"; // 1 -> 4
    unsigned_moves_templates[4 ] = "movb   %v1b, %vdb"; // 2 -> 1
    unsigned_moves_templates[5 ] = "movw   %v1w, %vdw"; // 2 -> 2
    unsigned_moves_templates[6 ] = "movzwl %v1w, %vdl"; // 2 -> 3
    unsigned_moves_templates[7 ] = "movzwq %v1w, %vdq"; // 2 -> 4
    unsigned_moves_templates[8 ] = "movb   %v1b, %vdb"; // 3 -> 1
    unsigned_moves_templates[9 ] = "movw   %v1w, %vdw"; // 3 -> 2
    unsigned_moves_templates[10] = "movl   %v1l, %vdl"; // 3 -> 3
    unsigned_moves_templates[11] = "movl   %v1l, %vdl"; // 3 -> 4 Note: special case, see below
    unsigned_moves_templates[12] = "movb   %v1b, %vdb"; // 4 -> 1
    unsigned_moves_templates[13] = "movw   %v1w, %vdw"; // 4 -> 2
    unsigned_moves_templates[14] = "movl   %v1l, %vdl"; // 4 -> 3
    unsigned_moves_templates[15] = "movq   %v1q, %vdq"; // 4 -> 4

    for (int i = 0; i < 16; i++) {
        signed_moves_operations[i] = signed_moves_templates[i][3] == 's' ? X_MOVS : X_MOV;
        unsigned_moves_operations[i] = unsigned_moves_templates[i][3] == 'z' ? X_MOVZ : X_MOV;
    }

    // A mov to write a 32-bit value into a register also zeroes the upper 32 bits of
    // the register by default. The movl may not be removed by a live range coalesce, so
    // it must be treated as a MOVZ, i.e. a zero extension.
    unsigned_moves_operations[11] = X_MOVZ;
}

static void add_move_rules_ri_to_mi(void) {
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

static void add_move_rules_ru_to_mu(void) {
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

static void add_integer_long_double_move_rule(int src, int type, char *extend_template, char *mov_template, char *load_template) {
    Rule *r = add_rule(MLD5, IR_MOVE, src, 0, 6);
    add_allocate_stack_index_in_slot(r, 1, type);               // Allocate stack entry and put it in SV1
    if (extend_template)
        add_op(r, X_MOVC, 0,   SRC1, SV1, extend_template);    // Sign extend register if needed

    add_op(r, X_MOVC, 0,   SRC1, SV1,    mov_template);           // Move register to stack
    add_op(r, X_MOVC, 0,   0,    SV1,    load_template);          // Load floating point from stack
    add_op(r, X_MOVC, DST, 0,    SV1,    "fstpt %vdL");           // Store in dst
}

static void add_long_double_integer_move_rule(int dst, int type, char *conv_template, char *mov_template, char *ext_template) {
    Rule *r = add_rule(dst, IR_MOVE, MLD5, 0, 6);

    // Slot 1: stack index of backup control word
    // Slot 2: register for manipulating control word
    // Slot 3: stack index of temporary control word
    // Slot 4: result of conversion
    int x87_dst_type = type;
    int ru4 = dst == RU4;

    add_allocate_stack_index_in_slot(r, 1, TYPE_SHORT);   // Slot 1: stack index of backup control word
    add_allocate_register_in_slot   (r, 2, TYPE_SHORT);   // Slot 2: register for manipulating control word
    add_allocate_stack_index_in_slot(r, 3, TYPE_SHORT);   // Slot 3: stack index of temporary control word
    add_allocate_stack_index_in_slot(r, 4, x87_dst_type); // Slot 4: result of conversion
    add_allocate_register_in_slot   (r, 5, TYPE_LONG);    // Slot 5: Sign of unsigned long

    add_op(r, X_MOVC, 0, SRC1, 0, "fldt %v1L");         // Load input long double

    if (ru4) {
        // Special case for unsigned longs. If the value is > 2^63, subtract it
        // then xor the high bit of the integer at the end

        add_op(r, X_MOVC, 0,   0, 0, "flds .LDTORU4(%%rip)");     // Load 2^63                                    stack = (2^63, v)
        add_op(r, X_MOVC, 0,   0, 0, "fucomi %%st(1), %%st");     // Compare with 2^63                            stack = (2^63, v)
        add_op(r, X_SETA, SV5, 0, 0, "setbe %vdb");               // Store 1 if the value > 2^63
        add_op(r, X_MOVC, 0,   0, 0, "fldz");                     // Load zero                                    stack = (0, 2^63, v)
        add_op(r, X_MOVC, 0,   0, 0, "fxch %%st(1)");             // Exchange top two stack entries               stack = (2^63, 0, v)
        add_op(r, X_MOVC, 0,   0, 0, "fcmovnbe %%st(1), %%st");   // Move st1 to st0 if the value > 2^63          stack = (2^63, 0, v) or stack = (0, 0, v)
        add_op(r, X_MOVC, 0,   0, 0, "fstp %%st(1)");             // Delete 2nc stack entry                       stack = (2^63, v)    or stack = (0, v)
        add_op(r, X_MOVC, 0,   0, 0, "fsubrp %%st, %%st(1)");     // Reverse subtract                             stack = (v - 2^63)   or stack = (v)
    }

    add_op(r, X_MOVC, 0,   0,   SV1, "fnstcw %v2");         // Backup control word into allocated stack entry
    add_op(r, X_MOVZ, 0,   SV2, SV1, "movzwl %v2w, %v1l");  // Move control word into register
    add_op(r, X_BOR,  0,   SV2, 0,   "orl $3072, %v1l");    // Set rounding and precision control bits
    add_op(r, X_MOVC, SV3, SV2, SV3, "movw %v1w, %v2w");    // Move control word into allocated stack
    add_op(r, X_MOVC, 0,   0,   SV3, "fldcw %v2w");         // Load control word from allocated stack
    add_op(r, X_MOVC, 0,   0,   SV4, conv_template);        // Do conversion and store it on the stack
    add_op(r, X_MOVC, 0,   0,   SV1, "fldcw %v2w");         // Restore control register from backup
    add_op(r, X_MOVC, DST, 0,   SV4, mov_template);         // Move converted value into the dst register

    if (ext_template)
        add_op(r, X_MOVC, DST, 0, SV4, ext_template);     // Move converted value into the dst register

    if (ru4) {
        // xor the high bit if the value was > 2^53
        add_op(r, X_MOVC, 0,   0, SV5, "movzbq %v2b, %v2q"); // Zero all bits except the first
        add_op(r, X_SHR,  0,   0, SV5, "shlq $63, %v2q");    // Move the first bit to the last bit
        add_op(r, X_XOR,  DST, 0, SV5, "xorq  %v2q, %vdq");  // set the sign bit if the original value was >= 2^63 - 1
    }
}

static void add_sse_constant_to_ld_move_rule(int src, int src_type, char *t1, char *t2, char *t3) {
    Rule *r = add_rule(MLD5, IR_MOVE, src, 0, 1);
    add_allocate_stack_index_in_slot(r, 1, src_type);
    add_op(r, X_MOV,  0,   SRC1, 0, t1);
    add_op(r, X_MOV,  SV1, 0,    0, t2);
    add_op(r, X_MOV,  0,   SV1,  0, t3);
    add_op(r, X_MOV,  DST, 0,    0, "fstpt %vdL");
}

static void add_sse_on_stack_to_int_in_register_move_rule(int dst, int src, int type, char *t1, char *t2) {
    Rule *r = add_rule(dst, IR_MOVE, src, 0, 1);
    add_allocate_register_in_slot(r, 1, type);
    add_op(r, X_MOVC,  SV1, SRC1, 0, t1);
    add_op(r, X_MOVC,  DST, SV1, 0, t2);
    fin_rule(r);
}

static void add_sse_in_register_to_long_double_move_rule(int src, int type, char *t1, char *t2) {
    Rule *r = add_rule(MLD5, IR_MOVE, src, 0, 1);
    add_allocate_stack_index_in_slot(r, 1, type);
    add_op(r, X_MOVC,  SV1, SRC1, 0, t1);
    add_op(r, X_MOVC,  0,   SV1,  0, t2);
    add_op(r, X_MOVC,  DST, 0,    0, "fstpt %vdL");
}

static void add_sse_in_stack_to_long_double_move_rule(int src, char *t1) {
    Rule *r = add_rule(MLD5, IR_MOVE, src, 0, 1);
    add_op(r, X_MOVC,  0,   SRC1, 0, t1);
    add_op(r, X_MOVC,  DST, 0,    0, "fstpt %vdL");
}

static Rule *add_int_to_sse_move_rule(int dst, int src) {
    int cost = (src == RU3 || src == RU4) ? 26  :2;
    return add_rule(dst, IR_MOVE, src, 0, cost);
}

static void add_ulong_to_sse_operations(Rule *r, int dst, int src) {
    // The conversion happens by dividing by two, then converting to SSE, then multiplying by two.
    // The or and and stuff is to ensure the intermediate number is rounded up to the nearest odd.
    // This prevents an double rounding error.

    char *t1, *t2; // Convert & add templates

    if (dst == RS3 && src == RU4) { t1 = "cvtsi2ssq %v1q, %vdF";  t2 = "addss %vdF, %vdF"; }
    if (dst == RS4 && src == RU4) { t1 = "cvtsi2sdq %v1q, %vdD";  t2 = "addsd %vdD, %vdD"; }

    add_allocate_label_in_slot(r, 1);                               // Completion label
    add_allocate_label_in_slot(r, 2);                               // Signed case
    add_allocate_register_in_slot(r, 3, TYPE_LONG);                 // Temporary for unsigned case
    add_op(r, X_TEST,  SRC1, SRC1,  0,    "testq %v1q, %v1q");
    add_op(r, X_JZ,    0,    SV2,   0,    "js %v1");                // Jump if unsigned
    add_op(r, X_MOVC,  DST,  SRC1,  0,    t1);                      // Signed case
    add_op(r, X_JMP,   0,    SV1,   0,    "jmp %v1");
    add_op(r, X_MOVC,  SV3,  SRC1,  0,    ".L2:movq %v1q, %vdq");   // Unsigned case
    add_op(r, X_SHR,   SV3,  SV3,   0,    "shrq %v1q");
    add_op(r, X_BAND,  SRC1, SRC1,  0,    "andl $1, %v1l");
    add_op(r, X_BOR,   SV3,  SV3,   SRC1, "orq %v1q, %v2q");
    add_op(r, X_MOVC,  DST,  SRC1,  0,    t1);
    add_op(r, X_ADD,   DST,  DST,   DST,  t2);
    add_op(r, X_MOVC,  DST,  DST,   0,    ".L1:");                  // Done
}

static void add_int_to_sse_operations(Rule *r, int dst, int src) {
    // Add operations that convert an in a register to a SSE in a register
    char *t1 = 0;
    char *t2 = 0;

         if (dst == RS3 && src == RI1) { t1 = "movsbl %v1b, %vdl"; t2 = "cvtsi2ssl %v1l, %vdF"; }
    else if (dst == RS3 && src == RI2) { t1 = "movswl %v1w, %vdl"; t2 = "cvtsi2ssl %v1l, %vdF"; }
    else if (dst == RS3 && src == RI3) {                           t2 = "cvtsi2ssl %v1l, %vdF"; }
    else if (dst == RS3 && src == RI4) {                           t2 = "cvtsi2ssq %v1q, %vdF"; }
    else if (dst == RS4 && src == RI1) { t1 = "movsbl %v1b, %vdl"; t2 = "cvtsi2sdl %v1l, %vdD"; }
    else if (dst == RS4 && src == RI2) { t1 = "movswl %v1w, %vdl"; t2 = "cvtsi2sdl %v1l, %vdD"; }
    else if (dst == RS4 && src == RI3) {                           t2 = "cvtsi2sdl %v1l, %vdD"; }
    else if (dst == RS4 && src == RI4) {                           t2 = "cvtsi2sdq %v1q, %vdD"; }
    else if (dst == RS3 && src == RU1) { t1 = "movzbl %v1b, %vdl"; t2 = "cvtsi2ssl %v1l, %vdF"; }
    else if (dst == RS3 && src == RU2) { t1 = "movzwl %v1w, %vdl"; t2 = "cvtsi2ssl %v1l, %vdF"; }
    else if (dst == RS3 && src == RU3) { t1 = "movl   %v1l, %vdl"; t2 = "cvtsi2ssq %v1q, %vdF"; }
    else if (dst == RS3 && src == RU4) { add_ulong_to_sse_operations(r, dst, src); return; }
    else if (dst == RS4 && src == RU1) { t1 = "movzbl %v1b, %vdl"; t2 = "cvtsi2sdl %v1l, %vdD"; }
    else if (dst == RS4 && src == RU2) { t1 = "movzwl %v1w, %vdl"; t2 = "cvtsi2sdl %v1l, %vdD"; }
    else if (dst == RS4 && src == RU3) { t1 = "movl   %v1l, %vdl"; t2 = "cvtsi2sdq %v1q, %vdD"; }
    else if (dst == RS4 && src == RU4) { add_ulong_to_sse_operations(r, dst, src); return; }
    else panic ("Unknown dst/src combination");

    if (t1) add_op(r, X_MOVC,  SRC1, SRC1, 0, t1);
            add_op(r, X_MOVC,  DST,  SRC1, 0, t2);

}

static void add_long_double_to_sse_move_rule(int dst, int type, char *t1, char *t2) {
    Rule *r = add_rule(dst, IR_MOVE, MLD5, 0, 10);
    add_allocate_stack_index_in_slot(r, 1, type);
    add_op(r, X_MOV,  0, SRC1, 0, "fldt %v1L");
    add_op(r, X_MOV,  SV1,  0, 0, t1);
    add_op(r, X_MOV,  DST, SV1, 0, t2);
}

static void add_float_and_double_move_rules(void) {
    Rule *r ;

    // Constant -> register
    r = add_rule(RS3, IR_MOVE, CS3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movss %v1F, %vdq"); // Load float constant into float
    r = add_rule(RS3, IR_MOVE, CS4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movss %v1F, %vdq"); // Load double constant into float
    r = add_rule(RS4, IR_MOVE, CS3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movsd %v1D, %vdq"); // Load float constant into double
    r = add_rule(RS4, IR_MOVE, CS4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movsd %v1D, %vdq"); // Load double constant into double

    // Constant -> memory
    r = add_rule(MS3, IR_MOVE, CS3, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movss %v1F, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "movss %%xmm14, %vdq");
    r = add_rule(MS4, IR_MOVE, CS4, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movsd %v1D, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "movsd %%xmm14, %vdq");

    // Register -> register
    r = add_rule(RS3, IR_MOVE, RS3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movss %v1q, %vdq");
    r = add_rule(RS3, IR_MOVE, RS4, 0, 1); add_op(r, X_MOVC, DST, SRC1, 0, "cvtsd2ss %v1q, %vdq");
    r = add_rule(RS4, IR_MOVE, RS3, 0, 1); add_op(r, X_MOVC, DST, SRC1, 0, "cvtss2sd %v1q, %vdq");
    r = add_rule(RS4, IR_MOVE, RS4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movsd %v1q, %vdq");

    // Memory -> register
    r = add_rule(RS3, IR_MOVE, MS3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movss %v1q, %vdq");
    r = add_rule(RS3, IR_MOVE, MS4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movsd %v1q, %vdq"); add_op(r, X_MOVC, DST, DST, 0, "cvtsd2ss %v1q, %vdq");
    r = add_rule(RS4, IR_MOVE, MS3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movss %v1q, %vdq"); add_op(r, X_MOVC, DST, DST, 0, "cvtss2sd %v1q, %vdq");
    r = add_rule(RS4, IR_MOVE, MS4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movsd %v1q, %vdq");

    // Register -> memory
    r = add_rule(MS3, IR_MOVE, RS3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movss %v1q, %vdq");
    r = add_rule(MS4, IR_MOVE, RS4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movsd %v1q, %vdq");

    // SSE Constant -> integer in register
    r = add_rule(XR1, IR_MOVE, CS3, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movss %v1F, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "cvttss2sil %%xmm14, %vdl"); fin_rule(r);
    r = add_rule(XR2, IR_MOVE, CS3, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movss %v1F, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "cvttss2sil %%xmm14, %vdl"); fin_rule(r);
    r = add_rule(XR3, IR_MOVE, CS3, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movss %v1F, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "cvttss2sil %%xmm14, %vdl"); fin_rule(r);
    r = add_rule(XR4, IR_MOVE, CS3, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movss %v1F, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "cvttss2siq %%xmm14, %vdq"); fin_rule(r);

    r = add_rule(XR1, IR_MOVE, CS4, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movsd %v1D, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "cvttsd2sil %%xmm14, %vdl"); fin_rule(r);
    r = add_rule(XR2, IR_MOVE, CS4, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movsd %v1D, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "cvttsd2sil %%xmm14, %vdl"); fin_rule(r);
    r = add_rule(XR3, IR_MOVE, CS4, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movsd %v1D, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "cvttsd2sil %%xmm14, %vdl"); fin_rule(r);
    r = add_rule(XR4, IR_MOVE, CS4, 0, 1); add_op(r, X_MOV,  0, SRC1, 0, "movsd %v1D, %%xmm14"); add_op(r, X_MOV,  DST, 0, 0, "cvttsd2siq %%xmm14, %vdq"); fin_rule(r);

    // SSE Constant -> LD
    add_sse_constant_to_ld_move_rule(CS3, TYPE_FLOAT,  "movss %v1F, %%xmm0", "movss %%xmm0, %vd", "flds %v1");
    add_sse_constant_to_ld_move_rule(CS4, TYPE_DOUBLE, "movsd %v1D, %%xmm0", "movsd %%xmm0, %vd", "fldl %v1");

    // SSE in register -> integer in register
    r = add_rule(XR1, IR_MOVE, RS3, 0, 1); add_op(r, X_MOVC,  DST, SRC1, 0, "cvttss2sil %v1F, %vdl"); fin_rule(r);
    r = add_rule(XR2, IR_MOVE, RS3, 0, 1); add_op(r, X_MOVC,  DST, SRC1, 0, "cvttss2sil %v1F, %vdl"); fin_rule(r);
    r = add_rule(XR3, IR_MOVE, RS3, 0, 1); add_op(r, X_MOVC,  DST, SRC1, 0, "cvttss2sil %v1F, %vdl"); fin_rule(r);
    r = add_rule(XR4, IR_MOVE, RS3, 0, 1); add_op(r, X_MOVC,  DST, SRC1, 0, "cvttss2siq %v1F, %vdq"); fin_rule(r);

    r = add_rule(XR1, IR_MOVE, RS4, 0, 1); add_op(r, X_MOVC,  DST, SRC1, 0, "cvttsd2sil %v1F, %vdl"); fin_rule(r);
    r = add_rule(XR2, IR_MOVE, RS4, 0, 1); add_op(r, X_MOVC,  DST, SRC1, 0, "cvttsd2sil %v1F, %vdl"); fin_rule(r);
    r = add_rule(XR3, IR_MOVE, RS4, 0, 1); add_op(r, X_MOVC,  DST, SRC1, 0, "cvttsd2sil %v1F, %vdl"); fin_rule(r);
    r = add_rule(XR4, IR_MOVE, RS4, 0, 1); add_op(r, X_MOVC,  DST, SRC1, 0, "cvttsd2siq %v1F, %vdq"); fin_rule(r);

    // SSE on stack -> integer in register
    add_sse_on_stack_to_int_in_register_move_rule(XR1, MS3, TYPE_FLOAT, "movss %v1F, %vdF", "cvttss2sil %v1F, %vdl");
    add_sse_on_stack_to_int_in_register_move_rule(XR2, MS3, TYPE_FLOAT, "movss %v1F, %vdF", "cvttss2sil %v1F, %vdl");
    add_sse_on_stack_to_int_in_register_move_rule(XR3, MS3, TYPE_FLOAT, "movss %v1F, %vdF", "cvttss2sil %v1F, %vdl");
    add_sse_on_stack_to_int_in_register_move_rule(XR4, MS3, TYPE_FLOAT, "movss %v1F, %vdF", "cvttss2siq %v1F, %vdq");

    add_sse_on_stack_to_int_in_register_move_rule(XR1, MS4, TYPE_DOUBLE, "movsd %v1D, %vdD", "cvttsd2sil %v1D, %vdl");
    add_sse_on_stack_to_int_in_register_move_rule(XR2, MS4, TYPE_DOUBLE, "movsd %v1D, %vdD", "cvttsd2sil %v1D, %vdl");
    add_sse_on_stack_to_int_in_register_move_rule(XR3, MS4, TYPE_DOUBLE, "movsd %v1D, %vdD", "cvttsd2sil %v1D, %vdl");
    add_sse_on_stack_to_int_in_register_move_rule(XR4, MS4, TYPE_DOUBLE, "movsd %v1D, %vdD", "cvttsd2siq %v1D, %vdq");

    // SSE in register -> long double
    add_sse_in_register_to_long_double_move_rule(RS3, TYPE_FLOAT, "movss %v1F, %vdl", "flds %v1F");
    add_sse_in_register_to_long_double_move_rule(RS4, TYPE_DOUBLE, "movsd %v1D, %vdq", "fldl %v1D");

    // SSE in stack -> long double
    add_sse_in_stack_to_long_double_move_rule(MS3, "flds %v1F");
    add_sse_in_stack_to_long_double_move_rule(MS4, "fldl %v1D");

    // Integer in register -> SSE in register
    for (int dst = RS3; dst <= RS4; dst++)
        for (int src = RI1; src <= RU4; src++) {
            r = add_int_to_sse_move_rule(dst, src);
            add_int_to_sse_operations(r, dst, src);
        }

    // Long double -> SSE
    add_long_double_to_sse_move_rule(RS3, TYPE_FLOAT, "fstps %vdL", "movss %v1F, %vdF");
    add_long_double_to_sse_move_rule(RS4, TYPE_DOUBLE, "fstpl %vdL", "movsd %v1D, %vdD");
}

static void add_long_double_move_rules(void)  {
    Rule *r;

    // Long double -> integer rules
    // ----------------------------------------------------------------------------------

    // Long double constant -> memory
    r = add_rule(MLD5, IR_MOVE, CLD,  0, 3);
    add_op(r, X_MOV,  DST, SRC1, 0, "movabsq %v1L, %%r10"); add_op(r, X_MOV,  DST, SRC1, 0, "movq %%r10, %vdL");
    add_op(r, X_MOV,  DST, SRC1, 0, "movabsq %v1H, %%r10"); add_op(r, X_MOV,  DST, SRC1, 0, "movq %%r10, %vdH");

    // Long double memory -> memory
    r = add_rule(MLD5, IR_MOVE, MLD5,  0, 5);
    add_op(r, X_MOVC,  SRC1, SRC1, 0, "movq %v1L, %%r10");
    add_op(r, X_MOVC,  DST,  DST,  0, "movq %%r10, %vdL");
    add_op(r, X_MOVC,  SRC1, SRC1, 0, "movq %v1H, %%r10");
    add_op(r, X_MOVC,  DST,  DST,  0, "movq %%r10, %vdH");

    // Integer in register -> long double in memory

    // Signed integers
    add_integer_long_double_move_rule(RI1, TYPE_CHAR , "movsbw %v1b, %v1w", "movw %v1w, %v2", "filds %v2L");
    add_integer_long_double_move_rule(RI2, TYPE_SHORT, 0,                   "movw %v1w, %v2", "filds %v2L");
    add_integer_long_double_move_rule(RI3, TYPE_INT  , 0,                   "movl %v1l, %v2", "fildl %v2L");
    add_integer_long_double_move_rule(RI4, TYPE_LONG , 0,                   "movq %v1q, %v2", "fildq %v2L");

    // Unsigned integers
    add_integer_long_double_move_rule(RU1, TYPE_CHAR , "movzbw %v1b, %v1w", "movw %v1w, %v2", "filds %v2L");
    add_integer_long_double_move_rule(RU2, TYPE_SHORT, "movzwl %v1w, %v1l", "movl %v1l, %v2", "fildl %v2L");
    add_integer_long_double_move_rule(RU3, TYPE_INT  , "movl   %v1l, %v1l", "movq %v1q, %v2", "fildq %v2L");

    // RU4 is a special case
    // Inspired by clang, when run with -fPIE
    // Signed constants <0 are converted and then need a huge constant added to them
    r = add_rule(MLD5, IR_MOVE, RU4, 0, 10);
    add_allocate_stack_index_in_slot(r, 1, TYPE_LONG);      // Allocate stack entry and put it in slot 1
    add_op(r, X_MOVC, 0,   SRC1, SV1,  "movq   %v1q, %v2");               // Move register to stack
    add_op(r, X_MOVC, 0,   0,    0,    "movq   $0, %%r10");               // Zero r10
    add_op(r, X_MOVC, 0,   SRC1, 0,    "testq  %v1q, %v1q");              // Test & set flags
    add_op(r, X_MOVC, DST, 0,    0,    "sets   %%r10b");                  // Store sign in r10 (0=unsigned, 1=signed)
    add_op(r, X_MOVC, 0,   SRC2, SV1,  "fildq  %v2L");                    // Load floating point from stack
    add_op(r, X_MOVC, 0,   0,    0,    "leaq   .RU4TOLD(%%rip), %%r11");  // Add constant, 0 for unsigned, something huge for signed
    add_op(r, X_MOVC, 0,   0,    0,    "fadds  (%%r11,%%r10,4)");
    add_op(r, X_MOVC, DST, 0,    0,    "fstpt  %vdL");                    // Store in dst

    // Integer -> long double rules
    // ----------------------------------------------------------------------------------
    add_long_double_integer_move_rule(RI1, TYPE_SHORT, "fistps %v2w", "movzbl %v2b, %vdl", "movsbl %vdb, %vdl");
    add_long_double_integer_move_rule(RU1, TYPE_SHORT, "fistps %v2w", "movzbl %v2b, %vdl", "movzbl %vdb, %vdl");
    add_long_double_integer_move_rule(RI2, TYPE_SHORT, "fistps %v2w", "movzwl %v2w, %vdl", "movswl %vdw, %vdl");
    add_long_double_integer_move_rule(RU2, TYPE_INT,   "fistpl %v2w", "movl   %v2w, %vdl", "movzwl %vdw, %vdl");
    add_long_double_integer_move_rule(RI3, TYPE_INT  , "fistpl %v2l", "movl   %v2l, %vdl", 0);
    add_long_double_integer_move_rule(RU3, TYPE_LONG , "fistpq %v2l", "movq   %v2q, %vdq", "movl   %vdl, %vdl");
    add_long_double_integer_move_rule(RI4, TYPE_LONG , "fistpq %v2l", "movq   %v2q, %vdq", 0);
    add_long_double_integer_move_rule(RU4, TYPE_LONG , "fistpq %v2l", "movq   %v2q, %vdq", 0);
}

static Rule *add_move_to_ptr(int src1, int src2, char *sign_extend_template, char *op_template) {
    Rule *r = add_rule(src1, IR_MOVE_TO_PTR, src1, src2, 3);
    if (sign_extend_template) add_op(r, X_MOVS, SRC2, SRC2, 0, sign_extend_template);
    add_op(r, X_MOV_TO_IND, 0,  SRC1,  SRC2, op_template);
    return r;
}

// Add rules for loads & address of from IR_BSHL + IR_ADD + IR_INDIRECT/IR_ADDRESS_OF
static void add_scaled_rule(int *ntc, int cst, int add_reg, int indirect_reg, int address_of_reg, int op, char *template) {
    Rule *r;

    int ntc1 = ++*ntc;
    int ntc2 = ++*ntc;
    r = add_rule(ntc1, IR_BSHL, RI4, cst, 1); add_save_value(r, 1, 2); // Save index register to slot 2
    r = add_rule(ntc1, IR_BSHL, RU4, cst, 1); add_save_value(r, 1, 2); // Save index register to slot 2
    r = add_rule(ntc2, IR_ADD, add_reg, ntc1, 1); add_save_value(r, 1, 1); // Save address register to slot 1

    if (indirect_reg) r = add_rule(indirect_reg, IR_INDIRECT, ntc2, 0, 1);
    if (address_of_reg) r = add_rule(address_of_reg, IR_ADDRESS_OF, ntc2, 0, 1);
    add_op(r, op, DST, SV1, SV2, template);
}

static void add_offset_rule(int *ntc, int add_reg, int indirect_reg, char *template) {
    Rule *r;

    int ntc1 = ++*ntc;
    r = add_rule(ntc1, IR_ADD, add_reg, CI4, 1);
    add_save_value(r, 1, 1); // Save address register to slot 1
    add_save_value(r, 2, 2); // Save offset register to slot 2
    r = add_rule(indirect_reg, IR_INDIRECT, ntc1, 0, 1);
    add_op(r, X_MOV_FROM_SCALED_IND, DST, SV1, SV2, template);
}

static void add_composite_pointer_rules(int *ntc) {
    // Loads
    add_scaled_rule(ntc, CSTV1, RP2, RI2, 0, X_MOV_FROM_SCALED_IND, "movw   (%v1q,%v2q,2), %vdw"); // from *short to short
    add_scaled_rule(ntc, CSTV1, RP2, RU2, 0, X_MOV_FROM_SCALED_IND, "movw   (%v1q,%v2q,2), %vdw"); // from *ushort to ushort
    add_scaled_rule(ntc, CSTV2, RP3, RI3, 0, X_MOV_FROM_SCALED_IND, "movl   (%v1q,%v2q,4), %vdl"); // from *int to int
    add_scaled_rule(ntc, CSTV2, RP3, RU3, 0, X_MOV_FROM_SCALED_IND, "movl   (%v1q,%v2q,4), %vdl"); // from *uint to uint
    add_scaled_rule(ntc, CSTV3, RP4, RI4, 0, X_MOV_FROM_SCALED_IND, "movq   (%v1q,%v2q,8), %vdq"); // from *long to long
    add_scaled_rule(ntc, CSTV3, RP4, RU4, 0, X_MOV_FROM_SCALED_IND, "movq   (%v1q,%v2q,8), %vdq"); // from *ulong to ulong
    add_scaled_rule(ntc, CSTV3, RP4, RP4, 0, X_MOV_FROM_SCALED_IND, "movq   (%v1q,%v2q,8), %vdq"); // from **

    add_offset_rule(ntc, RP1, RI1, "movb   %v2q(%v1q), %vdb"); // from struct member from *char -> char
    add_offset_rule(ntc, RP1, RU1, "movb   %v2q(%v1q), %vdb"); // from struct member from *uchar -> uchar
    add_offset_rule(ntc, RP2, RU2, "movw   %v2q(%v1q), %vdw"); // from struct member from *ushort -> ushort
    add_offset_rule(ntc, RP2, RI2, "movw   %v2q(%v1q), %vdw"); // from struct member from *short -> short
    add_offset_rule(ntc, RP3, RI3, "movl   %v2q(%v1q), %vdl"); // from struct member from *int -> int
    add_offset_rule(ntc, RP3, RU3, "movl   %v2q(%v1q), %vdl"); // from struct member from *uint -> uint
    add_offset_rule(ntc, RP4, RI4, "movq   %v2q(%v1q), %vdq"); // from struct member from *long -> long
    add_offset_rule(ntc, RP4, RU4, "movq   %v2q(%v1q), %vdq"); // from struct member from *ulong -> ulong
    add_offset_rule(ntc, RP3, RS3, "movss  %v2q(%v1q), %vdF"); // from struct member from *float -> float
    add_offset_rule(ntc, RP4, RS4, "movsd  %v2q(%v1q), %vdF"); // from struct member from *double -> double


    add_offset_rule(ntc, RP4, RP1, "movq   %v2q(%v1q), %vdq"); // from ** to *char
    add_offset_rule(ntc, RP4, RP2, "movq   %v2q(%v1q), %vdq"); // from ** to *short
    add_offset_rule(ntc, RP4, RP3, "movq   %v2q(%v1q), %vdq"); // from ** to *int
    add_offset_rule(ntc, RP4, RP4, "movq   %v2q(%v1q), %vdq"); // from ** to *long

    // Address of
    add_scaled_rule(ntc, CSTV1, RP4, 0, RP4, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,2), %vdq");
    add_scaled_rule(ntc, CSTV2, RP4, 0, RP4, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,4), %vdq");
    add_scaled_rule(ntc, CSTV3, RP4, 0, RP4, X_MOV_FROM_SCALED_IND, "lea    (%v1q,%v2q,8), %vdq");
}

static void add_int_indirect_rule(int dst, int src) {
    char *template;

    switch (src) {
        case RP1: template = "movb %v1o(%v1q), %vdb"; break;
        case RP2: template = "movw %v1o(%v1q), %vdw"; break;
        case RP3: template = "movl %v1o(%v1q), %vdl"; break;
        default:  template = "movq %v1o(%v1q), %vdq";
    }

    Rule *r = add_rule(dst, IR_INDIRECT, src, 0, 2);
    add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, template);
}

static void add_sse_indirect_rule(int dst, int src) {
    char *template;

    switch (src) {
        case RP3:  template = "movss %v1o(%v1q), %vdF"; break;
        case RP4:  template = "movsd %v1o(%v1q), %vdD"; break;
        default:   panic("Unknown src in add_sse_indirect_rule %d", src);
    }

    Rule *r = add_rule(dst, IR_INDIRECT, src, 0, 2);
    add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, template);
}

static void add_indirect_rules(void) {
    Rule *r ;

    // Integers
    for (int dst = 0; dst < 4; dst++) add_int_indirect_rule(RI1 + dst, RP1 + dst);
    for (int dst = 0; dst < 4; dst++) add_int_indirect_rule(RU1 + dst, RP1 + dst);

    // Rules used when loading a struct into a register
    r = add_rule(RU4, IR_INDIRECT, RP1, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movzbq %v1o(%v1q), %vdq");
    r = add_rule(RU4, IR_INDIRECT, RP2, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movzwq %v1o(%v1q), %vdq");
    r = add_rule(RU4, IR_INDIRECT, RP3, 0, 2); add_op(r, X_MOV_FROM_IND, DST, SRC1, 0, "movl %v1o(%v1q), %vdl");

    // SSE
    for (int dst = 0; dst < 2; dst++) add_sse_indirect_rule(RS3 + dst, RP3 + dst);

    // Pointer to pointer
    for (int dst = 0; dst < 4; dst++) add_int_indirect_rule(RP1 + dst, RP4);
    add_int_indirect_rule(RP5, RP4);
    add_int_indirect_rule(RPF, RP4);

    // Pointer to long double in register to pointer in memory
    r = add_rule(MPV, IR_MOVE, RP5, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");

    // Long double indirect from pointer in register
    r = add_rule(MLD5, IR_INDIRECT, RP5, 0, 5);
    add_allocate_register_in_slot(r, 1, TYPE_LONG);                       // SV1: allocate register for value
    add_op(r, X_MOV_FROM_IND, SV1, SRC1, 0,   "movq %v1o(%v1q), %vdq");   // Move low byte
    add_op(r, X_MOV,          DST, SRC1, SV1, "movq %v2q, %vdL");
    add_op(r, X_MOV_FROM_IND, SV1, SRC1, 0,   "movq %v1or+8(%v1q), %vdq"); // Move high byte
    add_op(r, X_MOV,          DST, SRC1, SV1, "movq %v2q, %vdH");

    // Long double indirect from pointer in memory
    r = add_rule(MLD5, IR_INDIRECT, MPV, 0, 5);
    add_allocate_register_in_slot(r, 1, TYPE_LONG);                     // SV1: register for pointer
    add_allocate_register_in_slot(r, 2, TYPE_LONG);                     // SV2: register for value
    add_op(r, X_MOVC,         SV1, SRC1, 0, "movq %v1q, %vdq");         // Load pointer in memory into pointer in register
    add_op(r, X_MOV_FROM_IND, SV2, SV1,  0, "movq %v1o(%v1q), %vdq");   // Move low byte
    add_op(r, X_MOVC,         DST, SV2,  0, "movq %v1q, %vdL");
    add_op(r, X_MOV_FROM_IND, SV2, SV1,  0, "movq %v1or+8(%v1q), %vdq"); // Move high byte
    add_op(r, X_MOVC,         DST, SV2,  0, "movq %v1q, %vdH");

    // Move of a constant to a pointer in a register
    r = add_rule(RP5, IR_MOVE_TO_PTR, RP5, CLD, 5);
    add_allocate_register_in_slot(r, 1, TYPE_LONG);                   // SV1: register for value
    add_op(r, X_MOVC,       SV1, SRC2, 0,   "movq %v1L, %vdq");       // Move low byte
    add_op(r, X_MOV_TO_IND, 0,   SRC1, SV1, "movq %v2q, %v1o(%v1q)"); // Move high byte
    add_op(r, X_MOVC,       SV1, SRC2, 0,   "movq %v1H, %vdq");
    add_op(r, X_MOV_TO_IND, 0,   SRC1, SV1, "movq %v2q, %v1or+8(%v1q)");

    // Move of a long double to a pointer in a register
    r = add_rule(RP5, IR_MOVE_TO_PTR, RP5, MLD5, 5);
    add_allocate_register_in_slot(r, 1, TYPE_LONG);                   // SV1: register for value
    add_op(r, X_MOVC,       SV1, SRC2, 0,   "movq %v1L, %vdq");       // Move low byte
    add_op(r, X_MOV_TO_IND, 0,   SRC1, SV1, "movq %v2q, %v1o(%v1q)"); // Move high byte
    add_op(r, X_MOVC,       SV1, SRC2, 0,   "movq %v1H, %vdq");
    add_op(r, X_MOV_TO_IND, 0,   SRC1, SV1, "movq %v2q, %v1or+8(%v1q)");
}

static void add_address_of_rule(int dst, int src, char *template_args, int finish) {
    Rule *r;

    char *mov_template, *lea_template;
    wasprintf(&lea_template, "leaq %s", template_args);
    wasprintf(&mov_template, "movq %s", template_args);

    r = add_rule(dst, IR_ADDRESS_OF,          src,  0, 2); add_op(r, X_LEA, DST, SRC1, 0, lea_template); if (finish) fin_rule(r);
    r = add_rule(dst, IR_ADDRESS_OF_FROM_GOT, src,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, mov_template); if (finish) fin_rule(r);
}

static void add_pointer_rules(int *ntc) {
    Rule *r;

    add_indirect_rules();
    add_composite_pointer_rules(ntc);

    // Address loads
    // Any ADDRESS_OF a pointer in a register must be lvalues. Therefore, a mov converts them from an lvalue into an rvalue
    r = add_rule(XRP, IR_ADDRESS_OF, RP1,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, IR_ADDRESS_OF, RP2,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, IR_ADDRESS_OF, RP3,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, IR_ADDRESS_OF, RP4,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(RP5, IR_ADDRESS_OF, RP5,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, IR_ADDRESS_OF, RP5,  0, 1); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);

    // Common rules for IR_ADDRESS_OF and IR_ADDRESS_OF_FROM_GOT
    add_address_of_rule(XRP, XMI,  "%v1q, %vdq", 1);
    add_address_of_rule(XRP, XMU,  "%v1q, %vdq", 1);
    add_address_of_rule(XRP, MS3,  "%v1q, %vdq", 1);
    add_address_of_rule(XRP, MS4,  "%v1q, %vdq", 1);
    add_address_of_rule(XRP, MPV,  "%v1q, %vdq", 1);
    add_address_of_rule(RP5, MLD5, "%v1L, %vdq", 0);
    add_address_of_rule(RP1, MSA,  "%v1q, %vdq", 0);
    add_address_of_rule(RP2, MSA,  "%v1q, %vdq", 0);
    add_address_of_rule(RP3, MSA,  "%v1q, %vdq", 0);
    add_address_of_rule(RP4, MSA,  "%v1q, %vdq", 0);
    add_address_of_rule(RP5, MSA,  "%v1q, %vdq", 0);
    add_address_of_rule(RPF, FUN,  "%v1q, %vdq", 0);
    add_address_of_rule(RP4, MPF,  "%v1q, %vdq", 0);

    // Stores of a pointer to a pointer
    for (int dst = RP1; dst <= RP4; dst++)
        for (int src = RP1; src <= RP5; src++)
            add_move_to_ptr(dst, src, 0, "movq %v2q, %v1o(%v1q)");

    // Stores to a pointer
    for (int is_unsigned = 0; is_unsigned < 2; is_unsigned++) {
        add_move_to_ptr(RP1, is_unsigned ? RU1 : RI1, 0, "movb %v2b, %v1o(%v1q)");
        add_move_to_ptr(RP1, is_unsigned ? RU2 : RI2, 0, "movb %v2b, %v1o(%v1q)");
        add_move_to_ptr(RP2, is_unsigned ? RU2 : RI2, 0, "movw %v2w, %v1o(%v1q)");
        add_move_to_ptr(RP1, is_unsigned ? RU3 : RI3, 0, "movb %v2b, %v1o(%v1q)");
        add_move_to_ptr(RP2, is_unsigned ? RU3 : RI3, 0, "movw %v2w, %v1o(%v1q)");
        add_move_to_ptr(RP3, is_unsigned ? RU3 : RI3, 0, "movl %v2l, %v1o(%v1q)");
        add_move_to_ptr(RP1, is_unsigned ? RU4 : RI4, 0, "movb %v2b, %v1o(%v1q)");
        add_move_to_ptr(RP2, is_unsigned ? RU4 : RI4, 0, "movw %v2w, %v1o(%v1q)");
        add_move_to_ptr(RP3, is_unsigned ? RU4 : RI4, 0, "movl %v2l, %v1o(%v1q)");
        add_move_to_ptr(RP4, is_unsigned ? RU4 : RI4, 0, "movq %v2q, %v1o(%v1q)");
    }

    // Move a pointer to a function to a pointer to a pointer to a function
    add_move_to_ptr(RP4, RPF, 0, "movq %v2q, %v1o(%v1q)");

    // Note, CI4 cannot be written to RP4 since there is no instruction for it
    add_move_to_ptr(RP1, CI1, 0, "movb $%v2b, %v1o(%v1q)");
    add_move_to_ptr(RP2, CI2, 0, "movw $%v2w, %v1o(%v1q)");
    add_move_to_ptr(RP3, CI3, 0, "movl $%v2l, %v1o(%v1q)");
    add_move_to_ptr(RP4, CI3, 0, "movq $%v2q, %v1o(%v1q)");

    add_move_to_ptr(RP3, RS3, 0, "movss %v2F, %v1o(%v1q)");
    add_move_to_ptr(RP4, RS4, 0, "movsd %v2D, %v1o(%v1q)");
}

static void add_conditional_zero_jump_rule(int operation, int src1, int src2, int cost, int x86_cmp_operation, char *comparison, char *conditional_jmp, int do_fin_rule) {
    int x86_jmp_operation = operation == IR_JZ ? X_JZ : X_JNZ;

    Rule *r = add_rule(0, operation, src1, src2, cost);
    add_op(r, x86_cmp_operation, 0, SRC1, 0, comparison);
    add_op(r, x86_jmp_operation, 0, SRC2, 0, conditional_jmp);
    if (do_fin_rule) fin_rule(r);
}

// Add integer comparision conditional jump rule
static void add_int_comp_cond_jmp_rule(int *ntc, int src1, int src2, char *template, int op, int x86op1, char *t1, int x86op2, char *t2) {
    Rule *r;

    (*ntc)++;
    r = add_rule(*ntc, op,     src1, src2, 10); add_op(r, X_CMP,  0, SRC1, SRC2, template ); fin_rule(r);
    r = add_rule(0,    IR_JNZ, *ntc, LAB,  1 ); add_op(r, x86op1, 0, SRC2, 0,    t1); fin_rule(r);
    r = add_rule(0,    IR_JZ,  *ntc, LAB,  1 ); add_op(r, x86op2, 0, SRC2, 0,    t2); fin_rule(r);
}

// Add integer comparision conditional jump rules
static void add_int_comp_cond_jmp_rules(int *ntc, int is_unsigned, int src1, int src2, char *template) {
    add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_EQ, X_JE,  "je %v1",  X_JNE, "jne %v1");
    add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_NE, X_JNE, "jne %v1", X_JE,  "je %v1");

    if (is_unsigned) {
        add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_LT, X_JB,  "jb %v1" , X_JAE, "jae %v1" );
        add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_GT, X_JA,  "ja %v1",  X_JBE, "jbe %v1");
        add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_LE, X_JBE, "jbe %v1", X_JA,  "ja %v1");
        add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_GE, X_JAE, "jae %v1", X_JB,  "jb %v1");
    }
    else {
        add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_LT, X_JLT, "jl %v1" , X_JGE, "jge %v1" );
        add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_GT, X_JGT, "jg %v1",  X_JLE, "jle %v1");
        add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_LE, X_JLE, "jle %v1", X_JGT, "jg %v1");
        add_int_comp_cond_jmp_rule(ntc, src1, src2, template, IR_GE, X_JGE, "jge %v1", X_JLT, "jl %v1");
    }
}

static void add_int_comparison_assignment_rule(int src1, int src2, char *cmp_template, int operation, int set_operation, char *set_template) {
    // Comparison operators always return an int
    Rule *r = add_rule(RI3, operation, src1, src2, 12);
    add_op(r, X_CMP,         0,   SRC1, SRC2, cmp_template);
    add_op(r, set_operation, DST, 0,    0,    set_template);
    add_op(r, X_MOVZ, DST, DST, 0, "movzbl %v1b, %v1l");
    fin_rule(r);
}

// Comparison and assignment rules for integers
static void add_int_comp_assignment_rules(int is_unsigned, int src1, int src2, char *template) {
    add_int_comparison_assignment_rule(src1, src2, template, IR_EQ, X_SETE,  "sete %vdb");
    add_int_comparison_assignment_rule(src1, src2, template, IR_NE, X_SETNE, "setne %vdb");

    if (is_unsigned) {
        add_int_comparison_assignment_rule(src1, src2, template, IR_LT, X_SETB,  "setb %vdb");
        add_int_comparison_assignment_rule(src1, src2, template, IR_GT, X_SETA,  "seta %vdb");
        add_int_comparison_assignment_rule(src1, src2, template, IR_LE, X_SETBE, "setbe %vdb");
        add_int_comparison_assignment_rule(src1, src2, template, IR_GE, X_SETAE, "setae %vdb");
    }
    else {
        add_int_comparison_assignment_rule(src1, src2, template, IR_LT, X_SETLT, "setl %vdb");
        add_int_comparison_assignment_rule(src1, src2, template, IR_GT, X_SETGT, "setg %vdb");
        add_int_comparison_assignment_rule(src1, src2, template, IR_LE, X_SETLE, "setle %vdb");
        add_int_comparison_assignment_rule(src1, src2, template, IR_GE, X_SETGE, "setge %vdb");
    }
}

// Add conditional jump and assignment rules for an integer comparison
static void add_int_comparison_rules(int *ntc, int is_unsigned, int src1, int src2, char *template) {
    add_int_comp_cond_jmp_rules(ntc, is_unsigned, src1, src2, template);
    add_int_comp_assignment_rules(is_unsigned, src1, src2, template);
}

static void add_long_double_comparison_instructions(Rule *r, int src1, int src2, char *src1_template, char *src2_template, char *template, int src1_first) {
    if (src1_first) {
        add_op(r, X_MOVC, 0, SRC1, 0, src1_template);
        add_op(r, X_MOVC, 0, SRC2, 0, src2_template);
    }
    else {
        add_op(r, X_MOVC, 0, SRC2, 0, src2_template);
        add_op(r, X_MOVC, 0, SRC1, 0, src1_template);
    }

    add_op(r, X_CMP,  0, 0, 0, template);
    add_op(r, X_MOVC, 0, 0, 0, "fstp %%st(0)");
}

static void add_long_double_comp_assignment_rule(int src1, int src2, char *src1_template, char *src2_template, int operation, int set_operation, char *set_template, int src1_first) {
    // Comparison operators always return an int
    Rule *r = add_rule(RI3, operation, src1, src2, 15);
    add_long_double_comparison_instructions(r, src1, src2, src1_template, src2_template, "fcomip %%st(1), %%st", src1_first);
    add_op(r, set_operation, DST, 0,    0, set_template);
    add_op(r, X_MOVZ,        DST, DST,  0, "movzbl %v1b, %v1l");
}

static void add_long_double_comp_cond_jmp_rule(int *ntc, int src1, int src2, char *src1_template, char *src2_template, int operation, int x86op1, char *t1, int x86op2, char *t2, int src1_first) {
    Rule *r;

    (*ntc)++;
    r = add_rule(*ntc, operation, src1, src2, 14);
    add_long_double_comparison_instructions(r, src1, src2, src1_template, src2_template, "fcomip %%st(1), %%st", src1_first);
    r = add_rule(0, IR_JNZ, *ntc, LAB, 1); add_op(r, x86op1, 0, SRC2, 0, t1); fin_rule(r);
    r = add_rule(0, IR_JZ,  *ntc, LAB, 1); add_op(r, x86op2, 0, SRC2, 0, t2); fin_rule(r);
}

// Comparison and assignment/jump rules for floating point numbers
static void add_long_double_comp_rules(int *ntc, int src1, int src2, char *src1_template, char *src2_template) {
    add_long_double_comp_assignment_rule(src1, src2, src1_template, src2_template, IR_LT, X_SETA,  "seta %vdb", 1);
    add_long_double_comp_assignment_rule(src1, src2, src1_template, src2_template, IR_GT, X_SETA,  "seta %vdb", 0);
    add_long_double_comp_assignment_rule(src1, src2, src1_template, src2_template, IR_LE, X_SETAE, "setae %vdb", 1);
    add_long_double_comp_assignment_rule(src1, src2, src1_template, src2_template, IR_GE, X_SETAE, "setae %vdb", 0);

    // == and != comparison assignments
    // No rules are added for conditional jumps, for simplicty.
    for (int i = 0; i < 2; i++) {
        // Inspired by the code that gcc generates
        Rule *r = add_rule(RI3, i == 0 ? IR_EQ : IR_NE, src1, src2, 15);
        add_long_double_comparison_instructions(r, src1, src2, src1_template, src2_template, "fucomip %%st(1), %%st", 1);
        add_op(r, X_CMP, DST, 0, 0, i == 0 ? "setnp %vdb" : "setp %vdb");
        add_op(r, X_LD_EQ_CMP, 0, 0, 0,  i == 0 ? "movl $0, %%edx" : "movl $1, %%edx");
        add_long_double_comparison_instructions(r, src1, src2, src1_template, src2_template, "fucomip %%st(1), %%st", 1);
        add_op(r, X_MOVC, DST, 0, 0, "cmovne %%edx, %vdl");
        add_op(r, X_MOVZ, DST, 0, 0, "movzbl %vdb, %vdl");
    }

    add_long_double_comp_cond_jmp_rule(ntc, src1, src2, src1_template, src2_template, IR_LT, X_JA,  "ja %v1" , X_JAE, "jbe %v1", 1);
    add_long_double_comp_cond_jmp_rule(ntc, src1, src2, src1_template, src2_template, IR_GT, X_JA,  "ja %v1",  X_JBE, "jbe %v1", 0);
    add_long_double_comp_cond_jmp_rule(ntc, src1, src2, src1_template, src2_template, IR_LE, X_JAE, "jae %v1", X_JA,  "jb %v1", 1);
    add_long_double_comp_cond_jmp_rule(ntc, src1, src2, src1_template, src2_template, IR_GE, X_JAE, "jae %v1", X_JB,  "jb %v1", 0);
}

static void add_sse_comp_assignment_operations(Rule *r, int src1, int src2, int operation, char *set_template) {
    add_op(r, X_SETA, DST, 0,   0, set_template);
    add_op(r, X_MOVZ, DST, DST, 0, "movzbl %v1b, %vdl");
}

static void add_sse_comp_rule(int *ntc, int src1, int src2, int operation, char *cmp_template, char *set_template, int x86op1, char *t1, int x86op2, char *t2) {
    // Add assignment rule
    Rule *r = add_rule(RI3, operation, src1, src2, 15);
    add_op(r, X_COMIS, 0, SRC1, SRC2, cmp_template);
    add_sse_comp_assignment_operations(r, SRC1, SRC2, operation, set_template);

    // Add conditional jump rule
    (*ntc)++;
    r = add_rule(*ntc, operation, src1, src2, 14);
    add_op(r, X_COMIS, 0, SRC1, SRC2, cmp_template);
    r = add_rule(0, IR_JNZ, *ntc, LAB, 1); add_op(r, x86op1, 0, SRC2, 0, t1);
    r = add_rule(0, IR_JZ,  *ntc, LAB, 1); add_op(r, x86op2, 0, SRC2, 0, t2);
}

static void add_sse_comp_assignment_memory_rule(int src1, int src2, int type, int operation, char *mov_template, char *cmp_template, char *set_template) {
    Rule *r = add_rule(RI3, operation, src1, src2, 20);

    // Load the second operand into a register in slot 2
    add_allocate_register_in_slot(r, 2, type);
    add_op(r, X_MOVC,  SV2, SRC2, 0, mov_template);
    add_op(r, X_COMIS, 0, SV2, SRC2, cmp_template);
    add_sse_comp_assignment_operations(r, SRC1, SV2, operation, set_template);
}

static void add_sse_eq_ne_assignment_operations(Rule *eq_rule, Rule *ne_rule, int src1, int src2, char *cmp_template) {
    add_allocate_register_in_slot(eq_rule, 1, TYPE_CHAR);
    add_op(eq_rule, X_COMIS, 0,   src1, src2, cmp_template);
    add_op(eq_rule, X_SETE,  DST, 0,    0,    "sete %vdb");
    add_op(eq_rule, X_SETNP, SV1, 0,    0,    "setnp %vdb");
    add_op(eq_rule, X_BAND,  DST, SV1,  DST,  "andb %v1b, %vdb");
    add_op(eq_rule, X_BAND,  DST, 0,    DST,  "andb $1, %vdb");
    add_op(eq_rule, X_MOVZ,  DST, DST,  0,    "movzbl %v1b, %vdl");

    add_allocate_register_in_slot(ne_rule, 1, TYPE_CHAR);
    add_op(ne_rule, X_COMIS, 0,   src1, src2, cmp_template);
    add_op(ne_rule, X_SETNE, DST, 0,    0,    "setne %vdb");
    add_op(ne_rule, X_SETP,  SV1, 0,    0,    "setp %vdb");
    add_op(ne_rule, X_BOR,   DST, SV1,  DST,  "orb %v1b, %vdb");
    add_op(ne_rule, X_BAND,  DST, 0,    DST,  "andb $1, %vdb");
    add_op(ne_rule, X_MOVZ,  DST, DST,  0,    "movzbl %v1b, %vdl");
}

static void add_sse_eq_ne_assignment_rule(int src1, int src2, char *cmp_template) {
    Rule *eq_rule = add_rule(RI3, IR_EQ, src1, src2, 20);
    Rule *ne_rule = add_rule(RI3, IR_NE, src1, src2, 20);
    add_sse_eq_ne_assignment_operations(eq_rule, ne_rule, SRC1, SRC2, cmp_template);
}

static void add_sse_eq_ne_assignment_memory_rule(int src1, int src2, char *mov_template, char *cmp_template) {
    Rule *eq_rule = add_rule(RI3, IR_EQ, src1, src2, 20);
    Rule *ne_rule = add_rule(RI3, IR_NE, src1, src2, 20);

    // Load the second operand into a register in slot 2
    add_allocate_register_in_slot(eq_rule, 2, TYPE_FLOAT);
    add_op(eq_rule, X_MOVC, SV2, SRC2, 0, mov_template);

    add_allocate_register_in_slot(ne_rule, 2, TYPE_DOUBLE);
    add_op(ne_rule, X_MOVC, SV2, SRC2, 0, mov_template);

    add_sse_eq_ne_assignment_operations(eq_rule, ne_rule, SRC1, SV2, cmp_template);
}

static void add_sse_eq_ne_assignment_memory_rules(int dst, int src, char size1, char size2, int type) {
    char *ucomiss, *comiss, *mov;
    wasprintf(&ucomiss, "ucomis%c %%v1%c, %%v2%c", size2, size1, size1);
    wasprintf(&comiss, "comis%c %%v1%c, %%v2%c", size2, size1, size1);
    wasprintf(&mov, "movs%c %%v1%c, %%vd%c", size2, size1, size1);

    // Note: G[TE] rules are missing & covered by leaf register loads
    add_sse_eq_ne_assignment_memory_rule(dst, src,        mov, ucomiss);
    add_sse_comp_assignment_memory_rule (dst, src, type, IR_LT, mov, comiss, "seta %vdb");
    add_sse_comp_assignment_memory_rule (dst, src, type, IR_LE, mov, comiss, "setnb %vdb");
}

static void add_sse_comp_assignment_rules(int *ntc) {
    // To be compatible with gcc, only setnb and seta are allowed. This is necessary
    // for nan handling to work correctly. Some of the memory/memory rules are missing
    // since they require both operands to be loaded into a register. They are
    // loaded at the leaves and covered by the register/register rules.

    // Register - register
    add_sse_eq_ne_assignment_rule(RS3, RS3, "ucomiss %v1F, %v2F");
    add_sse_eq_ne_assignment_rule(RS4, RS4, "ucomisd %v1D, %v2D");
    add_sse_comp_rule(ntc, RS3, RS3, IR_LT, "comiss %v1F, %v2F", "seta %vdb",  X_JA,  "ja %v1" , X_JAE, "jna %v1");
    add_sse_comp_rule(ntc, RS3, RS3, IR_GT, "comiss %v2F, %v1F", "seta %vdb",  X_JA,  "ja %v1",  X_JBE, "jna %v1");
    add_sse_comp_rule(ntc, RS3, RS3, IR_LE, "comiss %v1F, %v2F", "setnb %vdb", X_JAE, "jnb %v1", X_JA,  "jb %v1");
    add_sse_comp_rule(ntc, RS3, RS3, IR_GE, "comiss %v2F, %v1F", "setnb %vdb", X_JAE, "jnb %v1", X_JA,  "jb %v1");
    add_sse_comp_rule(ntc, RS4, RS4, IR_LT, "comisd %v1D, %v2D", "seta %vdb",  X_JA,  "ja %v1" , X_JAE, "jna %v1");
    add_sse_comp_rule(ntc, RS4, RS4, IR_GT, "comisd %v2D, %v1D", "seta %vdb",  X_JA,  "ja %v1",  X_JBE, "jna %v1");
    add_sse_comp_rule(ntc, RS4, RS4, IR_LE, "comisd %v1D, %v2D", "setnb %vdb", X_JAE, "jnb %v1", X_JA,  "jb %v1");
    add_sse_comp_rule(ntc, RS4, RS4, IR_GE, "comisd %v2D, %v1D", "setnb %vdb", X_JAE, "jnb %v1", X_JA,  "jb %v1");

    // Constant - register
    // Note: G[TE] rules are missing & covered by leaf register loads
    add_sse_eq_ne_assignment_rule(CS3, RS3, "ucomiss %v1F, %v2F");
    add_sse_eq_ne_assignment_rule(CS4, RS4, "ucomisd %v1D, %v2D");
    add_sse_comp_rule(ntc, CS3, RS3, IR_LT, "comiss %v1F, %v2F", "seta %vdb",  X_JA,  "ja %v1" , X_JAE, "jna %v1");
    add_sse_comp_rule(ntc, CS3, RS3, IR_LE, "comiss %v1F, %v2F", "setnb %vdb", X_JAE, "jnb %v1", X_JA,  "jb %v1");
    add_sse_comp_rule(ntc, CS4, RS4, IR_LT, "comisd %v1D, %v2D", "seta %vdb",  X_JA,  "ja %v1" , X_JAE, "jna %v1");
    add_sse_comp_rule(ntc, CS4, RS4, IR_LE, "comisd %v1D, %v2D", "setnb %vdb", X_JAE, "jnb %v1", X_JA,  "jb %v1");

    // Register - constant
    // Note: L[TE] rules are missing & covered by leaf register loads
    add_sse_eq_ne_assignment_rule(RS3, CS3, "ucomiss %v2F, %v1F");
    add_sse_eq_ne_assignment_rule(RS4, CS4, "ucomisd %v2D, %v1D");
    add_sse_comp_rule(ntc, RS3, CS3, IR_GT, "comiss %v2F, %v1F", "seta %vdb",  X_JA,  "ja %v1" , X_JAE, "jna %v1");
    add_sse_comp_rule(ntc, RS3, CS3, IR_GE, "comiss %v2F, %v1F", "setnb %vdb", X_JAE, "jnb %v1", X_JA,  "jb %v1");
    add_sse_comp_rule(ntc, RS4, CS4, IR_GT, "comisd %v2D, %v1D", "seta %vdb",  X_JA,  "ja %v1" , X_JAE, "jna %v1");
    add_sse_comp_rule(ntc, RS4, CS4, IR_GE, "comisd %v2D, %v1D", "setnb %vdb", X_JAE, "jnb %v1", X_JA,  "jb %v1");

    // Memory - memory
    add_sse_eq_ne_assignment_memory_rules(CS3, MS3, 'F', 's', TYPE_FLOAT);
    add_sse_eq_ne_assignment_memory_rules(CS4, MS4, 'D', 'd', TYPE_DOUBLE);
    add_sse_eq_ne_assignment_memory_rules(MS3, CS3, 'F', 's', TYPE_FLOAT);
    add_sse_eq_ne_assignment_memory_rules(MS4, CS4, 'D', 'd', TYPE_DOUBLE);
    add_sse_eq_ne_assignment_memory_rules(MS3, MS3, 'F', 's', TYPE_FLOAT);
    add_sse_eq_ne_assignment_memory_rules(MS4, MS4, 'D', 'd', TYPE_DOUBLE);
}

static void add_commutative_operation_rule(int operation, int x86_mov_operation, int x86_operation, int dst, int src1, int src2, int cost, char *mov_template, char *op_template) {
    Rule *r = add_rule(dst, operation, src1, src2, cost);
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

    wasprintf(&op_vv, "%s %%v1, %%v2",  x86_operand); // Perform operation on two registers or memory
    wasprintf(&op_cv, "%s $%%v1, %%v2", x86_operand); // Perform operation on a constant and a register

    // Add rules for byte, word and quads
    for (int i = 0; i < 3; i++) {
        add_commutative_operation_rule(                operation, X_MOV, x86_operation, RI1 + i, XRI, XRI, cost,     "mov%s %v1, %vd",  op_vv);
        add_commutative_operation_rule(                operation, X_MOV, x86_operation, RU1 + i, XRU, XRU, cost,     "mov%s %v1, %vd",  op_vv);
        add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RI1 + i, XRI, XMI, cost + 1, "mov%s %v1, %vd",  op_vv);
        add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RU1 + i, XRU, XMU, cost + 1, "mov%s %v1, %vd",  op_vv);

        // 64 bit immediates aren't possible, but 32 bit constants can't be used on quads either.
        for (int j = 0; j < 3; j++) {
            add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RI1 + i, CI1 + j, XRI,     cost,     "mov%s %v1, %vd",  op_cv);
            add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RI1 + i, CU1 + j, XRI,     cost,     "mov%s %v1, %vd",  op_cv);
            add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RU1 + i, CU1 + j, XRU,     cost,     "mov%s %v1, %vd",  op_cv);
            add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RU1 + i, CI1 + j, XRU,     cost,     "mov%s %v1, %vd",  op_cv);
            add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RI1 + i, XMI,     CI1 + j, cost + 1, "mov%s $%v1, %vd", op_vv);
            add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RU1 + i, XMU,     CU1 + j, cost + 1, "mov%s $%v1, %vd", op_vv);
        }
    }

    // Register moves on quads
    add_commutative_operation_rule(                operation, X_MOV, x86_operation, RI4, XRI, XRI, cost,     "mov%s %v1, %vd",  op_vv);
    add_commutative_operation_rule(                operation, X_MOV, x86_operation, RU4, XRU, XRU, cost,     "mov%s %v1, %vd",  op_vv);
    add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RI4, XRI, XMI, cost + 1, "mov%s %v1, %vd",  op_vv);
    add_bi_directional_commutative_operation_rules(operation, X_MOV, x86_operation, RU4, XRU, XMU, cost + 1, "mov%s %v1, %vd",  op_vv);

    // Operations between a register/memory and a constant with a quad result aren't straightforward
    // 1. There isn't an immediate 64 bit operand
    // 2. It's not possible to use CI1, CI2, CI3, CU1, CU2, CU3 on a quad without taking
    //    care, since e.g. andq $ffffffff could end up getting translated to andq $-1 which is incorrect.
    // For simplicity, no rules are present for quads & constants, forcing the constant into a register.
}

static void add_pointer_plus_int_rule(int dst, int src, int cost, int x86_operation, char *sign_extend_template) {
    Rule *r = add_rule(dst, IR_ADD, dst, src, cost);
    add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    if (sign_extend_template) add_op(r, X_MOVS, SRC2, SRC2, 0, sign_extend_template);
    add_op(r, x86_operation, DST,  SRC2, DST, "addq %v1q, %v2q");
}

static void add_pointer_add_rules(void) {
    for (int i = RP1; i <= RP5; i++) {
        add_pointer_plus_int_rule(i, RI1, 11, X_ADD, "movsbq %v1b, %vdq");
        add_pointer_plus_int_rule(i, RI2, 11, X_ADD, "movswq %v1w, %vdq");
        add_pointer_plus_int_rule(i, RI3, 11, X_ADD, "movslq %v1l, %vdq");
        add_pointer_plus_int_rule(i, RI4, 10, X_ADD, 0);

        add_pointer_plus_int_rule(i, RU1, 11, X_ADD, "movzbq %v1b, %vdq");
        add_pointer_plus_int_rule(i, RU2, 11, X_ADD, "movzwq %v1w, %vdq");
        add_pointer_plus_int_rule(i, RU3, 11, X_ADD, "movl %v1l, %vdl");
        add_pointer_plus_int_rule(i, RU4, 10, X_ADD, 0);
    }

    for (int i = CI1; i <= CI4; i++)
        add_bi_directional_commutative_operation_rules(IR_ADD, X_MOV, X_ADD, XRP, i, XRP, 10, "movq %v1q, %vdq", "addq $%v1q, %v2q");
}

static void add_sub_rule(int dst, int src1, int src2, int cost, char *mov_template, char *sign_extend_template, char *sub_template) {
    Rule *r = add_rule(dst, IR_SUB, src1, src2, cost);
    add_op(r, X_MOV, DST, SRC1, 0,   mov_template);
    if (sign_extend_template) add_op(r, X_MOVS,        SRC2, SRC2, 0,   sign_extend_template);
    add_op(r, X_SUB, DST, SRC2, DST, sub_template);
    fin_rule(r);
}

static void add_sub_rules(void) {
    // All of these subtractions can only done with 32 bit constants
    // CU3s need to be ruled out since they overflow if >= 0x80000000
    for (int i = 0; i < 3; i++) {
                    add_sub_rule(XRI, CI1 + i, XRI,     10, "mov%s $%v1, %vd", 0, "sub%s %v1, %vd");
        if (i != 2) add_sub_rule(XRU, CU1 + i, XRU,     10, "mov%s $%v1, %vd", 0, "sub%s %v1, %vd");
                    add_sub_rule(XRI, XRI,     CI1 + i, 10, "mov%s %v1, %vd",  0, "sub%s $%v1, %vd");
        if (i != 2) add_sub_rule(XRU, XRU,     CU1 + i, 10, "mov%s %v1, %vd",  0, "sub%s $%v1, %vd");
                    add_sub_rule(XRP, XRP,     CI1 + i, 10, "movq %v1q, %vdq", 0, "subq $%v1q, %vdq");
                    add_sub_rule(RI4, XRP,     CI1 + i, 10, "movq %v1q, %vdq", 0, "subq $%v1q, %vdq"); // A pointer difference is a signed int
        if (i != 2) add_sub_rule(XRP, XRP,     CU1 + i, 10, "movq %v1q, %vdq", 0, "subq $%v1q, %vdq");
                    add_sub_rule(RP5, RP5,     CI1 + i, 10, "movq %v1q, %vdq", 0, "subq $%v1q, %vdq");
        if (i != 2) add_sub_rule(RP5, RP5,     CU1 + i, 10, "movq %v1q, %vdq", 0, "subq $%v1q, %vdq");
                    add_sub_rule(XRI, CI1 + i, XMI,     11, "mov%s $%v1, %vd", 0, "sub%s %v1, %vd");
        if (i != 2) add_sub_rule(XRU, CU1 + i, XMU,     11, "mov%s $%v1, %vd", 0, "sub%s %v1, %vd");
                    add_sub_rule(XRI, XMI,     CI1 + i, 11, "mov%s %v1, %vd",  0, "sub%s $%v1, %vd");
                    add_sub_rule(XRU, XMU,     CI1 + i, 11, "mov%s %v1, %vd",  0, "sub%s $%v1, %vd");
    }

    add_sub_rule(XR, XR, XR,  10, "mov%s %v1, %vd",  0, "sub%s %v1, %vd");
    add_sub_rule(XR, XR, XM,  11, "mov%s %v1, %vd",  0, "sub%s %v1, %vd");
    add_sub_rule(XR, XM, XR,  11, "mov%s %v1, %vd",  0, "sub%s %v1, %vd");

    // Pointer - int subtraction
    for (int i = RP1; i <= RP5; i++) {
        add_sub_rule(i, i, RI1, 11, "movq %v1q, %vdq", "movsbq %v1b, %v1q", "subq %v1q, %vdq");
        add_sub_rule(i, i, RI2, 11, "movq %v1q, %vdq", "movswq %v1w, %v1q", "subq %v1q, %vdq");
        add_sub_rule(i, i, RI3, 11, "movq %v1q, %vdq", "movslq %v1l, %v1q", "subq %v1q, %vdq");
        add_sub_rule(i, i, RI4, 10, "movq %v1q, %vdq", 0,                   "subq %v1q, %vdq");

        add_sub_rule(i, i, RU1, 11, "movq %v1q, %vdq", "movzbq %v1b, %v1q", "subq %v1q, %vdq");
        add_sub_rule(i, i, RU2, 11, "movq %v1q, %vdq", "movzwq %v1w, %v1q", "subq %v1q, %vdq");
        add_sub_rule(i, i, RU3, 11, "movq %v1q, %vdq", "movl   %v1l, %v1l", "subq %v1q, %vdq");
        add_sub_rule(i, i, RU4, 10, "movq %v1q, %vdq", 0,                   "subq %v1q, %vdq");
    }

    // The result of a pointer-pointer subtraction is always a signed long: RI4.
    for (int i = RP1; i <= RP5; i++)
        for (int j = RP1; j <= RP5; j++)
            add_sub_rule(RI4, i, j,  10, "movq %v1q, %vdq", 0, "subq %v1q, %vdq");
}

static void add_div_rule(int dst, int src1, int src2, int cost, char *t1, char *t2, char *t3, char *tdiv, char *tmod) {
    Rule *r;

    r = add_rule(dst, IR_DIV, src1,  src2,  cost); add_op(r, X_MOV,  0,    SRC1, 0,    t1);
                                                   add_op(r, X_CLTD, 0,    0,    0,    t2);
                                                   add_op(r, X_IDIV, 0,    SRC2, 0,    t3);
                                                   add_op(r, X_MOV,  DST,  0,    0,    tdiv);
                                                   fin_rule(r);
    r = add_rule(dst, IR_MOD, src1,  src2,  cost); add_op(r, X_MOV,  0,    SRC1, 0,    t1);
                                                   add_op(r, X_CLTD, 0,    0,    0,    t2);
                                                   add_op(r, X_IDIV, 0,    SRC2, 0,    t3);
                                                   add_op(r, X_MOV,  DST,  0,    0,    tmod);
                                                   fin_rule(r);
}

static void add_div_rules(void) {
    add_div_rule(RI3, RI3, RI3, 40, "movl %v1l, %%eax", "cltd", "idivl %v1l", "movl %%eax, %vdl", "movl %%edx, %vdl");
    add_div_rule(RI4, RI4, RI4, 50, "movq %v1q, %%rax", "cqto", "idivq %v1q", "movq %%rax, %vdq", "movq %%rdx, %vdq");

    add_div_rule(RU3, RU3, RU3, 40, "movl %v1l, %%eax", "movl $0, %%edx", "divl %v1l", "movl %%eax, %vdl", "movl %%edx, %vdl");
    add_div_rule(RU4, RU4, RU4, 50, "movq %v1q, %%rax", "movq $0, %%rdx", "divq %v1q", "movq %%rax, %vdq", "movq %%rdx, %vdq");
}

static void add_bnot_rule(int dst, int src) {
    Rule *r = add_rule(dst, IR_BNOT, src, 0, 3); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd");
                                                 add_op(r, X_BNOT, DST, DST,  0, "not%s %vd");
                                                 fin_rule(r);
}

static void add_bnot_rules(void) {
    add_bnot_rule(XR, XR);
    add_bnot_rule(XR, XM);
}

static void add_binary_constant_shift_rule(int dst, int src1, int src2, char *template) {
    Rule *r;

   r = add_rule(dst, IR_BSHL, src1, src2, 3); add_op(r, X_MOV, DST, SRC1, 0,   "mov%s %v1, %vd");
                                              add_op(r, X_SHC, DST, SRC2, DST, "shl%s $%v1, %vd");
                                              fin_rule(r);
   r = add_rule(dst, IR_BSHR, src1, src2, 3); add_op(r, X_MOV, DST, SRC1, 0,   "mov%s %v1, %vd");   // Binary
                                              add_op(r, X_SHC, DST, SRC2, DST, "shr%s $%v1, %vd");
                                              fin_rule(r);
   r = add_rule(dst, IR_ASHR, src1, src2, 3); add_op(r, X_MOV, DST, SRC1, 0,   "mov%s %v1, %vd");   // Arithmetic
                                              add_op(r, X_SHC, DST, SRC2, DST, "sar%s $%v1, %vd");
                                              fin_rule(r);
}

static void add_binary_register_shift_rule(int src1, int src2, char *template) {
    Rule *r;

    r = add_rule(src1, IR_BSHL, src1, src2, 4); add_op(r, X_MOVC, 0,   SRC2, 0,   template);
                                                add_op(r, X_MOV,  DST, SRC1, 0,   "mov%s %v1, %vd");
                                                add_op(r, X_SHR,  DST, 0,    DST, "shl%s %%cl, %vd");
                                                fin_rule(r);

    r = add_rule(src1, IR_BSHR, src1, src2, 4); add_op(r, X_MOVC, 0,   SRC2, 0,   template);
                                                add_op(r, X_MOV,  DST, SRC1, 0,   "mov%s %v1, %vd");    // Binary
                                                add_op(r, X_SHR,  DST, 0,    DST, "shr%s %%cl, %vd");
                                                fin_rule(r);
    r = add_rule(src1, IR_ASHR, src1, src2, 4); add_op(r, X_MOVC, 0,   SRC2, 0,   template);
                                                add_op(r, X_MOV,  DST, SRC1, 0,   "mov%s %v1, %vd");    // Arithmetic
                                                add_op(r, X_SHR,  DST, 0,    DST, "sar%s %%cl, %vd");
                                                fin_rule(r);
}

static void add_binary_shift_rules(void) {
    // All combinations of src/dst signed/unsigned
    for (int dst = 0; dst < 2; dst++) {
        int dstx = dst ? EXP_SIGNED : EXP_UNSIGNED;
        for (int src = 0; src < 2; src++) {
            int srcx = src ? EXP_SIGNED : EXP_UNSIGNED;

            add_binary_constant_shift_rule(EXP_R + EXP_SIZE + dstx, EXP_R + EXP_SIZE + dstx, EXP_C + EXP_SIZE + srcx, "shl%s $%v1, %vd");
            add_binary_constant_shift_rule(EXP_R + EXP_SIZE + dstx, EXP_M + EXP_SIZE + dstx, EXP_C + EXP_SIZE + srcx, "shl%s $%v1, %vd");

            add_binary_register_shift_rule(EXP_R + EXP_SIZE + dstx, src ? RI1 : RU1, "movb %v1b, %%cl");
            add_binary_register_shift_rule(EXP_R + EXP_SIZE + dstx, src ? RI2 : RU2, "movw %v1w, %%cx");
            add_binary_register_shift_rule(EXP_R + EXP_SIZE + dstx, src ? RI3 : RU3, "movl %v1l, %%ecx");
            add_binary_register_shift_rule(EXP_R + EXP_SIZE + dstx, src ? RI4 : RU4, "movq %v1q, %%rcx");
        }
    }
}

static void add_long_double_operation_rule(int operation, int x86_operation, int cost, int dst, int src1, int src2, char *src1_template, char *src2_template, char *op_template, char *dst_template) {
    Rule *r;

    r = add_rule(dst, operation, src1, src2, cost);
    add_op(r, X_MOVC,        0,    SRC2, 0, src2_template);
    add_op(r, X_MOVC,        0,    SRC1, 0, src1_template);
    add_op(r, x86_operation, 0,    0,    0, op_template);
    add_op(r, X_MOVC,        DST,  0,    0, dst_template);
}

static void add_long_double_commutative_operation_rule(int operation, int x86_operation, int cost, int dst, int src1, int src2, char *src1_template, char *src2_template, char *op_template, char *dst_template) {
    add_long_double_operation_rule(operation, x86_operation, cost, dst, src1, src2, src1_template, src2_template, op_template, dst_template);
    add_long_double_operation_rule(operation, x86_operation, cost, dst, src2, src1, src2_template, src1_template, op_template, dst_template);
}

static void add_long_double_operation_rules(void) {
    char *ll = "fldt %v1L";
    char *lc = "fldt %v1C";
    char *fadd = "faddp %%st, %%st(1)";
    char *fsub = "fsubp %%st, %%st(1)";
    char *fmul = "fmulp %%st, %%st(1)";
    char *fdiv = "fdivp %%st, %%st(1)";
    char *fstore = "fstpt %vdL";

    add_long_double_operation_rule(            IR_ADD, X_FADD, 15, MLD5, MLD5, MLD5, ll, ll, fadd, fstore); // Add
    add_long_double_commutative_operation_rule(IR_ADD, X_FADD, 15, MLD5, MLD5, CLD,  ll, lc, fadd, fstore);

    add_long_double_operation_rule(            IR_SUB, X_FSUB, 15, MLD5, MLD5, MLD5, ll, ll, fsub, fstore); // Subtract
    add_long_double_operation_rule(            IR_SUB, X_FSUB, 15, MLD5, MLD5, CLD, ll, lc, fsub, fstore);
    add_long_double_operation_rule(            IR_SUB, X_FSUB, 15, MLD5, CLD,  MLD5, lc, ll, fsub, fstore);

    add_long_double_operation_rule(            IR_MUL, X_FMUL, 15, MLD5, MLD5, MLD5, ll, ll, fmul, fstore); // Multiply
    add_long_double_commutative_operation_rule(IR_MUL, X_FMUL, 15, MLD5, MLD5, CLD,  ll, lc, fmul, fstore);

    add_long_double_operation_rule(            IR_DIV, X_FDIV, 40, MLD5, MLD5, MLD5, ll, ll, fdiv, fstore); // Divide
    add_long_double_operation_rule(            IR_DIV, X_FDIV, 40, MLD5, MLD5, CLD,  ll, lc, fdiv, fstore);
    add_long_double_operation_rule(            IR_DIV, X_FDIV, 40, MLD5, CLD,  MLD5, lc, ll, fdiv, fstore);
}

static void add_sse_operation_rule(int operation, int x86_operation, int cost, int dst, int src1, int src2, char *mov_template, char *op_template) {
    Rule *r;

    r = add_rule(dst, operation, src1, src2, cost);
    add_op(r, X_MOV,         DST, SRC1, 0,   mov_template);
    add_op(r, x86_operation, DST, SRC2, DST, op_template);
}

// Add combinations for different storages (constant, register, memory) of SSE operands
static void add_sse_operation_combination_rules(int operation, int x86_operation, int cost, int type, char *mov_template, char *op_template) {
    add_sse_operation_rule(operation, x86_operation, cost, RS3 + type, RS3 + type, RS3 + type, mov_template, op_template);
    add_sse_operation_rule(operation, x86_operation, cost, RS3 + type, RS3 + type, CS3 + type, mov_template, op_template);
    add_sse_operation_rule(operation, x86_operation, cost, RS3 + type, CS3 + type, RS3 + type, mov_template, op_template);
    add_sse_operation_rule(operation, x86_operation, cost, RS3 + type, MS3 + type, MS3 + type, mov_template, op_template);
    add_sse_operation_rule(operation, x86_operation, cost, RS3 + type, MS3 + type, CS3 + type, mov_template, op_template);
    add_sse_operation_rule(operation, x86_operation, cost, RS3 + type, CS3 + type, MS3 + type, mov_template, op_template);
    add_sse_operation_rule(operation, x86_operation, cost, RS3 + type, MS3 + type, RS3 + type, mov_template, op_template);
    add_sse_operation_rule(operation, x86_operation, cost, RS3 + type, RS3 + type, MS3 + type, mov_template, op_template);
}

static void add_sse_operation_rules(void) {
    add_sse_operation_combination_rules(IR_ADD, X_FADD, 15, 0, "movss %v1F, %vdF", "addss %v1F, %vdF");
    add_sse_operation_combination_rules(IR_ADD, X_FADD, 15, 1, "movsd %v1D, %vdD", "addsd %v1D, %vdD");
    add_sse_operation_combination_rules(IR_SUB, X_FSUB, 15, 0, "movss %v1F, %vdF", "subss %v1F, %vdF");
    add_sse_operation_combination_rules(IR_SUB, X_FSUB, 15, 1, "movsd %v1D, %vdD", "subsd %v1D, %vdD");
    add_sse_operation_combination_rules(IR_MUL, X_FMUL, 15, 0, "movss %v1F, %vdF", "mulss %v1F, %vdF");
    add_sse_operation_combination_rules(IR_MUL, X_FMUL, 15, 1, "movsd %v1D, %vdD", "mulsd %v1D, %vdD");
    add_sse_operation_combination_rules(IR_DIV, X_FDIV, 40, 0, "movss %v1F, %vdF", "divss %v1F, %vdF");
    add_sse_operation_combination_rules(IR_DIV, X_FDIV, 40, 1, "movsd %v1D, %vdD", "divsd %v1D, %vdD");
}

static X86Operation *add_int_function_call_arg_op(Rule *r) {
    add_op(r, X_ARG, 0, SRC1, SRC2, "pushq %v2q");
}

static X86Operation *add_sse_function_call_arg_op(Rule *r, char *template) {
    add_allocate_register_in_slot(r, 1, TYPE_LONG);
    add_op(r, X_MOVC, SV1, SRC2, 0, template);
    add_op(r, X_ARG, 0, SRC1, SV1, "pushq %v2q");
}

void init_instruction_selection_rules(void) {
    Rule *r;

    instr_rule_count = 0;
    disable_merge_constants = 0;
    rule_coverage_file = 0;

    instr_rules = wcalloc(MAX_RULE_COUNT, sizeof(Rule));

    init_moves_templates();

    int ntc = AUTO_NON_TERMINAL_START;

    // Identity rules, for matching leaf nodes in the instruction tree
    r = add_rule(XC,    0, XC,    0, 0); fin_rule(r);
    r = add_rule(CLD,   0, CLD,   0, 0);
    r = add_rule(CS3,   0, CS3,   0, 0); fin_rule(r);
    r = add_rule(CS4,   0, CS4,   0, 0); fin_rule(r);
    r = add_rule(XR,    0, XR,    0, 0); fin_rule(r);
    r = add_rule(CSTV1, 0, CSTV1, 0, 0);
    r = add_rule(CSTV2, 0, CSTV2, 0, 0);
    r = add_rule(CSTV3, 0, CSTV3, 0, 0);
    r = add_rule(XM,    0, XM,    0, 0); fin_rule(r);
    r = add_rule(MLD5,  0, MLD5,  0, 0);
    r = add_rule(XRP,   0, XRP,   0, 0); fin_rule(r);
    r = add_rule(RP5,   0, RP5,   0, 0); fin_rule(r);
    r = add_rule(MPV,   0, MPV,   0, 0); fin_rule(r);
    r = add_rule(RS3,   0, RS3,   0, 0); fin_rule(r);
    r = add_rule(RS4,   0, RS4,   0, 0); fin_rule(r);
    r = add_rule(MS3,   0, MS3,   0, 0); fin_rule(r);
    r = add_rule(MS4,   0, MS4,   0, 0); fin_rule(r);
    r = add_rule(STL,   0, STL,   0, 0);
    r = add_rule(LAB,   0, LAB,   0, 0);
    r = add_rule(FUN,   0, FUN,   0, 0);
    r = add_rule(RPF,   0, RPF,   0, 0);
    r = add_rule(MPF,   0, MPF,   0, 0);
    r = add_rule(MSA,   0, MSA,   0, 0);

    r = add_rule(XR1,  0, XC1, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movb $%v1b, %vdb"); fin_rule(r);
    r = add_rule(XR2,  0, XC2, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movw $%v1w, %vdw"); fin_rule(r);
    r = add_rule(XR3,  0, XC3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movl $%v1l, %vdl"); fin_rule(r);
    r = add_rule(XR4,  0, XC4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq"); fin_rule(r);
    r = add_rule(RP5,  0, MPV, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRI,  0, XMI, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"); fin_rule(r);
    r = add_rule(XRU,  0, XMU, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"); fin_rule(r);

    r = add_rule(XRP, 0,  MI4,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, 0,  MU4,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, 0,  MPV,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);

    r = add_rule(RS3, 0,  CS3,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movss %v1F, %vdF");
    r = add_rule(RS4, 0,  CS4,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movsd %v1D, %vdD");
    r = add_rule(RS3, 0,  MS3,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movss %v1F, %vdF");
    r = add_rule(RS4, 0,  MS4,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movsd %v1D, %vdD");

    r = add_rule(RP5, 0,  MI4,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(RP1, 0,  STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(RP3, 0,  STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); // For wchar_t
    r = add_rule(RPF, 0,  MPF,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");

    // Register -> register move rules
    for (int dst = RI1; dst <= RI4; dst++) for (int src = RI1; src <= RI4; src++) add_mov_rule(dst, src, 0, 0);
    for (int dst = RU1; dst <= RU4; dst++) for (int src = RI1; src <= RI4; src++) add_mov_rule(dst, src, 0, 0);
    for (int dst = RI1; dst <= RI4; dst++) for (int src = RU1; src <= RU4; src++) add_mov_rule(dst, src, 0, 0);
    for (int dst = RU1; dst <= RU4; dst++) for (int src = RU1; src <= RU4; src++) add_mov_rule(dst, src, 0, 0);

    for (int i = 0; i < 2; i++) {
        r = add_rule(XRI, IR_MOVE, CI1 + i, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
        r = add_rule(XRI, IR_MOVE, CU1 + i, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
        r = add_rule(XRU, IR_MOVE, CI1 + i, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
        r = add_rule(XRU, IR_MOVE, CU1 + i, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
        r = add_rule(XMI, IR_MOVE, CI1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
        r = add_rule(XMI, IR_MOVE, CU1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
        r = add_rule(MPV, IR_MOVE, CI1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vd" ); fin_rule(r);
        r = add_rule(MPV, IR_MOVE, CU1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vd" ); fin_rule(r);
        r = add_rule(XMU, IR_MOVE, CI1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
        r = add_rule(XMU, IR_MOVE, CU1 + i, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
    }

    // CI3 constants are ok, but CU3 constants can overflow, so must be loaded into
    // memory through a register, since there is no instruction to load an imm64 into
    // memory.
    r = add_rule(XRI, IR_MOVE, CI3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
    r = add_rule(XRU, IR_MOVE, CI3, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd");  fin_rule(r);
    r = add_rule(XMI, IR_MOVE, CI3, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);
    r = add_rule(MPV, IR_MOVE, CI3, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vd" );  fin_rule(r);
    r = add_rule(XMU, IR_MOVE, CI3, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s $%v1, %vd" ); fin_rule(r);

    // CI4 and CU4 constants must be loaded into memory through a register, since there is no instruction to
    // load an imm64 into memory
    r = add_rule(XMI, IR_MOVE, RU4, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"); fin_rule(r);
    r = add_rule(XMU, IR_MOVE, RI4, 0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"); fin_rule(r);

    // Memory -> register move rules
    r = add_rule(XRI, IR_MOVE, XMI,  0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"); fin_rule(r);
    r = add_rule(XRU, IR_MOVE, XMU,  0, 2); add_op(r, X_MOV,  DST, SRC1, 0, "mov%s %v1, %vd"); fin_rule(r);

    // Register -> memory move rules
    add_move_rules_ri_to_mi();
    add_move_rules_ru_to_mu();

    add_float_and_double_move_rules();
    add_long_double_move_rules();

    // Physical register class moves
    r = add_rule(MS4, IR_MOVE_PREG_CLASS, RI4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(RI4, IR_MOVE_PREG_CLASS, RS4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);

    // Pointer move rules
    r = add_rule(RP1, IR_MOVE, STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(RP3, IR_MOVE, STL,  0, 1); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq"); // For wchar_t

    r = add_rule(XRP, IR_MOVE, CI4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, IR_MOVE, CU4, 0, 1); add_op(r, X_MOV,  DST, SRC1, 0, "movq $%v1q, %vdq"); fin_rule(r);

    for (int dst = RP1; dst <= RP5; dst++) for (int src = RP1; src <= RP5; src++) add_mov_rule(dst, src, 0, 0);
    for (int dst = RI1; dst <= RI4; dst++) for (int src = RP1; src <= RP5; src++) add_mov_rule(dst, src, 0, 0);
    for (int dst = RU1; dst <= RU4; dst++) for (int src = RP1; src <= RP5; src++) add_mov_rule(dst, src, 0, 0);
    for (int dst = RP1; dst <= RP5; dst++) for (int src = RI1; src <= RI4; src++) add_mov_rule(dst, src, 0, 0);
    for (int dst = RP1; dst <= RP5; dst++) for (int src = RU1; src <= RU4; src++) add_mov_rule(dst, src, 0, 0);

    r = add_rule(RP4, IR_MOVE, MI4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(MI4, IR_MOVE, XRP, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(MU4, IR_MOVE, XRP, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(XRP, IR_MOVE, MPV, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(MPV, IR_MOVE, XRP, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(MPV, IR_MOVE, XRI, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(MPV, IR_MOVE, XRU, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);

    add_pointer_rules(&ntc);

    // Position Independent Code (PIC)
    r = add_rule(XRP, IR_LOAD_FROM_GOT, XRP,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(RP5, IR_LOAD_FROM_GOT, RP5,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);
    r = add_rule(RPF, IR_LOAD_FROM_GOT, RPF,  0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq"); fin_rule(r);

    // Direct function calls
    r = add_rule(XRI,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRU,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRP,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(RPF,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(RS3,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(RS4,  IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(MLD5, IR_CALL, FUN, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); add_op(r, X_MOVC, DST, DST, 0, "fstpt %v1L");

    // Function calls through pointer in register
    r = add_rule(XRI,  IR_CALL, RPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRI,  IR_CALL, MPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRU,  IR_CALL, RPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRU,  IR_CALL, MPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRP,  IR_CALL, RPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRP,  IR_CALL, MPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(RS3,  IR_CALL, RPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(RS3,  IR_CALL, MPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(RS4,  IR_CALL, RPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(RS4,  IR_CALL, MPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(MLD5, IR_CALL, RPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); add_op(r, X_MOVC, DST, DST, 0, "fstpt %v1L");
    r = add_rule(MLD5, IR_CALL, MPF, 0, 5); add_op(r, X_CALL, DST, SRC1, 0, 0); add_op(r, X_MOVC, DST, DST, 0, "fstpt %v1L");

    // Constants in pushed argument. pushq immediate can only be 32 bit
    r = add_rule(0, IR_ARG, CI4, XC1, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq $%v2q"); fin_rule(r);
    r = add_rule(0, IR_ARG, CI4, XC2, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq $%v2q"); fin_rule(r);
    r = add_rule(0, IR_ARG, CI4, XC3, 2); add_op(r, X_ARG, 0, SRC1, SRC2, "pushq $%v2q"); fin_rule(r);

    // Stack management
    r = add_rule(0, IR_ALLOCATE_STACK, CI4, 0, 2); add_op(r, X_ALLOCATE_STACK, 0, SRC1, SRC2, "subq $%v1q, %%rsp"); fin_rule(r);
    r = add_rule(RI4, IR_MOVE_STACK_PTR, 0, 0, 2); add_op(r, X_MOVE_STACK_PTR, DST, 0, 0, "movq %%rsp, %vdq"); fin_rule(r);

    // imm64 needs to be loaded into a register first
    r = add_rule(0, IR_ARG, CI4, XC4, 2);
    add_allocate_register_in_slot (r, 1, TYPE_LONG);   // Allocate quad register in slot 1
    add_op(r, X_MOVC, SV1, SRC2, 0, "movabsq $%v1q, %vdq");
    add_op(r, X_ARG,  SV1, 0,    0, "pushq %vdq");
    fin_rule(r);

    // Long double constant arg
    r = add_rule(0, IR_ARG, CI4, CLD, 2);
    add_op(r, X_MOV, SRC2,  SRC2, 0,    "movabsq %v1H, %%r10");
    add_op(r, X_ARG, 0,     SRC1, SRC1, "pushq %%r10");
    add_op(r, X_MOV, SRC2,  SRC2, 0,    "movabsq %v1L, %%r10");
    add_op(r, X_ARG, 0,     SRC1, SRC1, "pushq %%r10");

    // Long double memory arg
    r = add_rule(0, IR_ARG, CI4, MLD5, 2);
    add_op(r, X_ARG, 0,    SRC1, SRC2, "pushq %v2H");
    add_op(r, X_ARG, 0,    SRC1, SRC2, "pushq %v2L");

    // SSE constant arg
    r = add_rule(0, IR_ARG, CI4, CS3, 2); add_sse_function_call_arg_op(r, "movabsq %v1f, %vdq");
    r = add_rule(0, IR_ARG, CI4, CS4, 2); add_sse_function_call_arg_op(r, "movabsq %v1d, %vdq");

    // SSE register arg
    r = add_rule(0, IR_ARG, CI4, RS3, 2); add_op(r, X_ARG, 0, 0, 0, "subq    $8, %%rsp"); add_op(r, X_ARG, 0, SRC1, SRC2, "movq %v2F, (%%rsp)");
    r = add_rule(0, IR_ARG, CI4, RS4, 2); add_op(r, X_MOV, 0, 0, 0, "subq    $8, %%rsp"); add_op(r, X_ARG, 0, SRC1, SRC2, "movq %v2D, (%%rsp)");

    // Add rules for sign/zero extention of an arg, but at a high cost, to encourage other rules to take precedence
    r = add_rule(0, IR_ARG, CI4, RI1, 10); add_op(r, X_MOVS, SRC2, SRC2, 0 , "movsbq %v1b, %v1q"); add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RI2, 10); add_op(r, X_MOVS, SRC2, SRC2, 0 , "movswq %v1w, %v1q"); add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RI3, 10); add_op(r, X_MOVS, SRC2, SRC2, 0 , "movslq %v1l, %v1q"); add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RI4, 2);                                                          add_int_function_call_arg_op(r);

    r = add_rule(0, IR_ARG, CI4, RU1, 10); add_op(r, X_MOVZ, SRC2, SRC2, 0 , "movzbq %v1b, %v1q"); add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RU2, 10); add_op(r, X_MOVZ, SRC2, SRC2, 0 , "movzwq %v1w, %v1q"); add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RU3, 10); add_op(r, X_MOVZ, SRC2, SRC2, 0 , "movl   %v1l, %v1l"); add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RU4, 2);                                                          add_int_function_call_arg_op(r);

    r = add_rule(0, IR_ARG, CI4, RP1, 2);                                                          add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RP2, 2);                                                          add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RP3, 2);                                                          add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RP4, 2);                                                          add_int_function_call_arg_op(r);
    r = add_rule(0, IR_ARG, CI4, RPF, 2);                                                          add_int_function_call_arg_op(r);

    // Long double return rules
    r = add_rule(0, IR_LOAD_LONG_DOUBLE, CLD,  0, 1); add_op(r, X_MOVC, DST, SRC1, 0, "fldt %v1C");
    r = add_rule(0, IR_LOAD_LONG_DOUBLE, MLD5, 0, 1); add_op(r, X_MOVC, DST, SRC1, 0, "fldt %v1L");

    // Function pointers
    r = add_rule(XRI, IR_MOVE, RPF, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRU, IR_MOVE, RPF, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(XRP, IR_MOVE, RPF, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, 0); fin_rule(r);
    r = add_rule(RPF, IR_MOVE, RPF, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(RPF, IR_MOVE, RP1, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(RPF, IR_MOVE, RP2, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(RPF, IR_MOVE, RP3, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(RPF, IR_MOVE, RP4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(MPF, IR_MOVE, RPF, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(RPF, IR_MOVE, MPF, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq %v1q, %vdq");
    r = add_rule(RPF, IR_MOVE, FUN, 0, 2); add_op(r, X_LEA, DST, SRC1, 0, "leaq %v1q, %vdq");
    r = add_rule(RPF, IR_MOVE, CI4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(RPF, IR_MOVE, CU4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(MPF, IR_MOVE, CI4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq $%v1q, %vdq");
    r = add_rule(MPF, IR_MOVE, CU4, 0, 2); add_op(r, X_MOV, DST, SRC1, 0, "movq $%v1q, %vdq");

    // Jump rules
    r = add_rule(0, IR_JMP, LAB, 0,1);  add_op(r, X_JMP, 0, SRC1, 0, "jmp %v1"); fin_rule(r);

    add_conditional_zero_jump_rule(IR_JZ,  XR,  LAB, 3, X_TEST, "test%s %v1, %v1",  "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  XRP, LAB, 3, X_TEST, "testq %v1q, %v1q", "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  XMI, LAB, 3, X_CMPZ, "cmp $0, %v1",      "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JZ,  RPF, LAB, 3, X_TEST, "testq %v1q, %v1q", "jz %v1",   1);
    add_conditional_zero_jump_rule(IR_JNZ, XR,  LAB, 3, X_TEST, "test%s %v1, %v1",  "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, XRP, LAB, 3, X_TEST, "testq %v1q, %v1q", "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, XMI, LAB, 3, X_CMPZ, "cmp $0, %v1",      "jnz %v1",  1);
    add_conditional_zero_jump_rule(IR_JNZ, RPF, LAB, 3, X_TEST, "testq %v1q, %v1q", "jnz %v1",  1);

    // Integer comparisons
    char *cmp_vv = "cmp%s %v2, %v1";
    char *cmpq_vv = "cmpq %v2q, %v1q";
    char *cmpq_vc = "cmpq $%v2q, %v1q";

    // Floating point loads
    char *ll = "fldt %v1L";
    char *lc = "fldt %v1C";

    // immediate comparisons can only be done on 32 bit integers
    // https://www.felixcloutier.com/x86/cmp

    // Comparision + conditional jump rules
    // Pointer comparisons are unsigned
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            add_int_comparison_rules(&ntc, 1, RP1 + i, RP1 + j, cmpq_vv);

    add_int_comparison_rules(&ntc, 1, RPF, RPF, "cmpq %v2q, %v1q");

    // Note the RU4, CI3 oddball. cmpq can only be done on imm32, which has to be signed. If CU4 were allowed, then 0x80000000 and higher would
    // be produced, which is illegal, since that would become an imm64 in the assembler.
    add_int_comparison_rules(&ntc, 0, XRI, XRI, cmp_vv);             add_int_comparison_rules(&ntc, 1, XRU, XRU, cmp_vv);
    add_int_comparison_rules(&ntc, 0, XRI, XMI, cmp_vv);             add_int_comparison_rules(&ntc, 1, XRU, XMU, cmp_vv);
    add_int_comparison_rules(&ntc, 0, XMI, XRI, cmp_vv);             add_int_comparison_rules(&ntc, 1, XMU, XRU, cmp_vv);
    add_int_comparison_rules(&ntc, 1, RI4, XRP, cmpq_vv);            add_int_comparison_rules(&ntc, 1, RU4, XRP, cmpq_vv);
    add_int_comparison_rules(&ntc, 1, XRP, RI4, cmpq_vv);            add_int_comparison_rules(&ntc, 1, XRP, RU4, cmpq_vv);
    add_int_comparison_rules(&ntc, 1, XRP, XCI, cmpq_vc);            add_int_comparison_rules(&ntc, 1, XRP, XCU, cmpq_vc);
    add_int_comparison_rules(&ntc, 1, RPF, XCI, cmpq_vc);            add_int_comparison_rules(&ntc, 1, RPF, XCU, cmpq_vc);
    add_int_comparison_rules(&ntc, 1, MPF, XCI, cmpq_vc);            add_int_comparison_rules(&ntc, 1, MPF, XCU, cmpq_vc);
    add_int_comparison_rules(&ntc, 1, XRP, XMI, cmpq_vv);            add_int_comparison_rules(&ntc, 1, XRP, XMU, cmpq_vv);
    add_int_comparison_rules(&ntc, 1, RI4, MPV, cmpq_vv);            add_int_comparison_rules(&ntc, 1, RU4, MPV, cmpq_vv);
    add_int_comparison_rules(&ntc, 1, XMI, XRP, cmpq_vv);            add_int_comparison_rules(&ntc, 1, XMU, XRP, cmpq_vv);
    add_int_comparison_rules(&ntc, 0, RI1, CI1, "cmpb $%v2b, %v1b"); add_int_comparison_rules(&ntc, 1, RU1, CU1, "cmpb $%v2b, %v1b");
    add_int_comparison_rules(&ntc, 0, RI2, CI2, "cmpw $%v2w, %v1w"); add_int_comparison_rules(&ntc, 1, RU2, CU2, "cmpw $%v2w, %v1w");
    add_int_comparison_rules(&ntc, 0, RI3, CI3, "cmpl $%v2l, %v1l"); add_int_comparison_rules(&ntc, 1, RU3, CU3, "cmpl $%v2l, %v1l");
    add_int_comparison_rules(&ntc, 0, RI4, CI3, "cmpq $%v2q, %v1q"); add_int_comparison_rules(&ntc, 1, RU4, CI3, "cmpq $%v2q, %v1q");
    add_int_comparison_rules(&ntc, 0, MI1, CI1, "cmpb $%v2b, %v1b"); add_int_comparison_rules(&ntc, 1, MU1, CU1, "cmpb $%v2b, %v1b");
    add_int_comparison_rules(&ntc, 0, MI2, CI2, "cmpw $%v2w, %v1w"); add_int_comparison_rules(&ntc, 1, MU2, CU2, "cmpw $%v2w, %v1w");
    add_int_comparison_rules(&ntc, 0, MI3, CI3, "cmpl $%v2l, %v1l"); add_int_comparison_rules(&ntc, 1, MU3, CU3, "cmpl $%v2l, %v1l");
    add_int_comparison_rules(&ntc, 0, MI4, CI3, "cmpq $%v2q, %v1q"); add_int_comparison_rules(&ntc, 1, MU4, CU3, "cmpq $%v2q, %v1q");
    add_int_comparison_rules(&ntc, 0, MPV, CI3, "cmpq $%v2q, %v1q"); add_int_comparison_rules(&ntc, 0, MPV, CU3, "cmpq $%v2q, %v1q");

    add_long_double_comp_rules(&ntc, MLD5, MLD5, ll, ll);
    add_long_double_comp_rules(&ntc, MLD5, CLD,  ll, lc);
    add_long_double_comp_rules(&ntc, CLD,  MLD5, lc, ll);

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

    add_long_double_operation_rules();
    add_sse_operation_rules();
    add_sse_comp_assignment_rules(&ntc);

    if (ntc >= AUTO_NON_TERMINAL_END)
        panic("terminal rules exceeded: %d > %d\n", ntc, AUTO_NON_TERMINAL_END);

    if (!disable_check_for_duplicate_rules) check_for_duplicate_rules();

    rule_coverage = new_set(instr_rule_count - 1);
}
