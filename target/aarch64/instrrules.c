#include "wcc.h"
#include "aarch64.h"

#define MAX_TARGET_OPS_PER_ROLE 32

int match_constant_type_in_instrsel = 1;

// Take a template and convert %v* placeholders and add b, w, l, q to them.
char *add_size_to_template(char *template, int size) {
    if (!template) return NULL; // Some magic operations have no templates but are implemented in codegen.

    char aarch64_size = size_to_aarch64_size(size);
    char *result = wcalloc(1, 128);
    char *dst = result;

    char *c = template;
    while (*c) {
        if (c[0] == '%' && c[1] == 'v' && (c[3] != 'w' && c[3] != 'x' )) {
            *dst++ = '%';
            *dst++ = 'v';
            *dst++ = c[2];
            *dst++ = aarch64_size;
            c += 2;
        }
        else
            *dst++ = *c;

        c++;
    }

    return result;
}

// Used to match a value to a leaf node (operation = 0) src rule
// There are a couple of possible matches for a constant.
int match_value_to_rule_src(Value *v, int src) {
    if (v->is_constant) {
        int vtt = v->type->type;
        int is_unsigned = v->type->is_unsigned;

        if (vtt == TYPE_LONG_DOUBLE)
            return src == CLD;
        else if (vtt == TYPE_FLOAT)
            return src == CS3;
        else if (vtt == TYPE_DOUBLE)
            return src == CS4;
        else {
            // Integer constant

                 if (vtt == TYPE_CHAR  && !is_unsigned && src == CI1) return 1;
            else if (vtt == TYPE_SHORT && !is_unsigned && src == CI2) return 1;
            else if (vtt == TYPE_INT   && !is_unsigned && src == CI3) return 1;
            else if (vtt == TYPE_LONG  && !is_unsigned && src == CI4) return 1;
            else if (vtt == TYPE_CHAR  &&  is_unsigned && src == CU1) return 1;
            else if (vtt == TYPE_SHORT &&  is_unsigned && src == CU2) return 1;
            else if (vtt == TYPE_INT   &&  is_unsigned && src == CU3) return 1;
            else if (vtt == TYPE_LONG  &&  is_unsigned && src == CU4) return 1;

            // Check if the constant can be encoded in the add/sub instructions family.
            // It must be a 12-bit immediate, optionally shifted left by 12 bits.
            // Either the constant is < 4096 or it is a multiple of 4096 (1<<12) and ((constant >> 12) <= 4095).
            else if (src == CADDSUB && ((v->int_value < 0x1000) || ((v->int_value & 0xfff) == 0) && ((v->int_value >> 12) < 0x1000)))
                return 1;

            else
                return 0;
        }
    }
    else
        return non_terminal_for_value(v) == src;
}


// Add an TargetOperation template to a rule's linked list, making a copy
static TargetOperation *add_aarch64_op_to_rule(Rule *r, TargetOperation *target_op) {
    if (!r->target_operation_count)
        r->target_operations = wmalloc(MAX_TARGET_OPS_PER_ROLE * sizeof(TargetOperation));

    if (r->target_operation_count == MAX_TARGET_OPS_PER_ROLE) panic("Exceeded MAX_TARGET_OPS_PER_ROLE");

    int index = r->target_operation_count++;
    r->target_operations[index] = *target_op;
    return &r->target_operations[index];
}

// Add an target operation template to a rule
static TargetOperation *add_op(Rule *r, int operation, int dst, int v1, int v2, char *template) {
    if (operation < TARGET_OPS_START) panic("Operation %s is not a target operation", operation_string(operation));

    TargetOperation *target_op = wcalloc(1, sizeof(TargetOperation));
    target_op->operation.id = operation;

    target_op->operation.is_move = (operation == AARCH64_OP_MOV);
    target_op->operation.is_call = (operation == AARCH64_OP_CALL);

    target_op->operation.is_conditional_jump = (
        operation == AARCH64_OP_BEQ ||
        operation == AARCH64_OP_BNE
    );

    target_op->operation.is_unconditional_jump = (operation == AARCH64_OP_B);

    // TODO aarch64, other tags, e.g.

    target_op->dst = dst;
    target_op->v1 = v1;
    target_op->v2 = v2;

    target_op->template = template;
    target_op->save_value_in_slot = 0;
    target_op->allocate_stack_index_in_slot = 0;
    target_op->allocate_register_in_slot = 0;
    target_op->allocate_label_in_slot = 0;
    target_op->allocated_type = 0;
    target_op->arg = 0;

    target_op = add_aarch64_op_to_rule(r, target_op);

    return target_op;
}

// Add rules in the early aarch64 development phase that make it possible for e2e tests to run
// TODO aarch64 remove
void add_early_testing_rules(void) {
    Rule *r;

    r = add_rule(RP1,  IR_MOVE, RP1, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdx, %v1x");

    // Function calls
    r = add_rule(0,    IR_CALL, FUN, 0, 5); add_op(r, AARCH64_OP_CALL, DST, SRC1, 0, 0);
    r = add_rule(RI3,  IR_CALL, FUN, 0, 5); add_op(r, AARCH64_OP_CALL, DST, SRC1, 0, 0);
    r = add_rule(RI4,  IR_CALL, FUN, 0, 5); add_op(r, AARCH64_OP_CALL, DST, SRC1, 0, 0);

    // Load pointer to string literal into register
    r = add_rule(RP1, IR_MOVE,  STL,  0, 1);
    add_op(r, AARCH64_OP_ADRP,     DST, SRC1, 0,    "adrp %vdx, %v1");
    add_op(r, AARCH64_OP_ADD_LO12, DST, DST,  SRC1, "add %vdx, %vdx, :lo12:%v2");

    // Address of
    r = add_rule(RP3, IR_ADDRESS_OF,  MI3,  0, 1);
    add_op(r, AARCH64_OP_ADRP,     DST, SRC1, 0,    "adrp %vdx, %v1");
    add_op(r, AARCH64_OP_ADD_LO12, DST, DST,  SRC1, "add %vdx, %vdx, :lo12:%v2");

    // Move to pointer
    r = add_rule(RP3, IR_MOVE_TO_PTR, RP3, RI3, 1);
    add_op(r, AARCH64_OP_ADRP, 0, SRC1, SRC2, "str %v2w, [%v1x]");

    // Pointer to pointer move
    r = add_rule(RP4,  IR_MOVE, RP4, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdx, %v1x");
}

static TargetOperation *add_convert_move_op(Rule *r, int operation, int dst, int v1, int v2, char *template) {
    TargetOperation *op = add_op(r, operation, dst, v1, v2, template);
    op->operation.is_convert_move = 1;
    return op;
}

static void add_int_register_move_rules(void) {
    Rule *r;

    for (int dst = RI1; dst <= RU4; dst++) {
        for (int src = RI1; src <= RU4; src++) {
            int src_size = make_target_size_from_non_terminal(src);
            int dst_size = make_target_size_from_non_terminal(dst);

            // If the size is staying the same or going down, then a straight move will do.
            // The upper bits may contain garbage, but that's ok.
            if (dst_size <= src_size) {
                int size = src_size >= dst_size ? src_size : dst_size;
                char *template = size < 4 ? "mov %vdw, %v1w" : "mov %vdx, %v1x";
                r = add_rule(dst,  IR_MOVE, src, 0, 1);
                add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, template);
                continue;
            }

            // Implicit else: dst_size > src_size
            int src_is_unsigned = src >= RU1;
            int dst_is_unsigned = dst >= RU1;

            int dst_is_32_bit = dst_size < 4;

            // If both arguments are signed, then the move must be sign extended
            if (!dst_is_unsigned && !src_is_unsigned) {
                if (src_size == 1) {
                    r = add_rule(dst, IR_MOVE, src, 0, 1);
                    add_convert_move_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, dst_is_32_bit ? "sxtb %vdw, %v1w" : "sxtb %vdx, %v1w");
                }
                else if (src_size == 2) {
                    r = add_rule(dst, IR_MOVE, src, 0, 1);
                    add_convert_move_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, dst_is_32_bit ? "sxth %vdw, %v1w" : "sxth %vdx, %v1w");
                }
                else if (src_size == 3) {
                    r = add_rule(dst, IR_MOVE, src, 0, 1);
                    add_convert_move_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, dst_is_32_bit ? "sxtw %vdw, %v1w" : "sxtw %vdx, %v1w");

                }
                else if (src_size == 4)
                    panic("Bug in add_int_register_move_rules()");
            }

            // Either of the arguments is unsigned. Truncate the destination to the src size
            else {
                if (src_size == 1) {
                    r = add_rule(dst, IR_MOVE, src, 0, 1);
                    add_convert_move_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, dst_is_32_bit ? "and %vdw, %v1w, 255" : "and %vdx, %v1x, 255");
                }
                else if (src_size == 2) {
                    r = add_rule(dst, IR_MOVE, src, 0, 1);
                    add_convert_move_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, dst_is_32_bit ? "and %vdw, %v1w, 65535" : "and %vdx, %v1x, 65535");
                }
                else if (src_size == 3) {
                    r = add_rule(dst, IR_MOVE, src, 0, 1);
                    add_convert_move_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, dst_is_32_bit ? "mov %vdw, %v1w" : "mov %vdw, %v1w");

                }
                else if (src_size == 4)
                    panic("Bug in add_int_register_move_rules()");
            }
        }
    }
}

static void add_one_operand_rules(char *target_operand, int base1, int base2, int operation, int target_operation, int cost) {
    Rule *r;

    char *op_w, *op_x;
    wasprintf(&op_w, "%s %%vdw, %%v1w", target_operand);
    wasprintf(&op_x, "%s %%vdx, %%v1x", target_operand);

    r = add_rule(base1 + 0, operation, base2 + 0, 0, cost); add_op(r, target_operation, DST, SRC1, 0, op_w);
    r = add_rule(base1 + 1, operation, base2 + 1, 0, cost); add_op(r, target_operation, DST, SRC1, 0, op_w);
    r = add_rule(base1 + 2, operation, base2 + 2, 0, cost); add_op(r, target_operation, DST, SRC1, 0, op_w);
    r = add_rule(base1 + 3, operation, base2 + 3, 0, cost); add_op(r, target_operation, DST, SRC1, 0, op_x);
}

static void add_two_operand_rules(char *target_operand, int base1, int base2, int base3, int operation, int target_operation, int cost) {
    Rule *r;

    char *op_w, *op_x;
    wasprintf(&op_w, "%s %%vdw, %%v1w, %%v2w", target_operand);
    wasprintf(&op_x, "%s %%vdx, %%v1x, %%v2x", target_operand);

    int keep_base3 = (base3== XC || base3 == XR || base3 == CADDSUB);
    r = add_rule(base1 + 0, operation, base2 + 0, keep_base3 ? base3 : base3 + 0, cost); add_op(r, target_operation, DST, SRC1, SRC2, op_w); fin_rule(r);
    r = add_rule(base1 + 1, operation, base2 + 1, keep_base3 ? base3 : base3 + 1, cost); add_op(r, target_operation, DST, SRC1, SRC2, op_w); fin_rule(r);
    r = add_rule(base1 + 2, operation, base2 + 2, keep_base3 ? base3 : base3 + 2, cost); add_op(r, target_operation, DST, SRC1, SRC2, op_w); fin_rule(r);
    r = add_rule(base1 + 3, operation, base2 + 3, keep_base3 ? base3 : base3 + 3, cost); add_op(r, target_operation, DST, SRC1, SRC2, op_x); fin_rule(r);
}

static void add_int_mod_rules(void) {
    for (int is_signed = 0; is_signed < 2; is_signed++) {
        int base = is_signed ? RI1 : RU1;
        for (int i = 0; i < 4; i++) {
            char *div_template = is_signed
                ? add_size_to_template("sdiv %vdw, %v1, %v2", i + 1)
                : add_size_to_template("udiv %vdw, %v1, %v2", i + 1);

            Rule *r = add_rule(base + i, IR_MOD, base + i, base + i, 50);
            add_allocate_register_in_slot(r, 1, TYPE_CHAR + i);
            add_allocate_register_in_slot(r, 2, TYPE_CHAR + i);
            add_op(r, AARCH64_OP_DIV, SV1, SRC1, SRC2, div_template);
            add_op(r, AARCH64_OP_MUL, SV2, SV1,  SRC2, add_size_to_template("mul %vd, %v1, %v2", i + 1));
            add_op(r, AARCH64_OP_SUB, DST, SRC1, SV2,  add_size_to_template("sub %vd, %v1, %v2", i + 1));
        }
    }
}

static void add_int_bitshift_rules(void) {
    // IR_BSHL, applies to both signed and unsigned integers
    add_two_operand_rules("lsl", RI1, RI1, XC, IR_BSHL, AARCH64_OP_LSL, 3); // signed r << c
    add_two_operand_rules("lsl", RI1, RI1, XR, IR_BSHL, AARCH64_OP_LSL, 3); // signed r << r
    add_two_operand_rules("lsl", RU1, RU1, XC, IR_BSHL, AARCH64_OP_LSL, 3); // unsigned r << c
    add_two_operand_rules("lsl", RU1, RU1, XR, IR_BSHL, AARCH64_OP_LSL, 3); // unsigned r << r

    // IR_ASHR, arithmetic left shift, for signed integers
    add_two_operand_rules("asr", RI1, RI1, XC, IR_ASHR, AARCH64_OP_ASR, 3); // signed r >> c
    add_two_operand_rules("asr", RI1, RI1, XR, IR_ASHR, AARCH64_OP_ASR, 3); // signed r >> r

    // IR_BSHR, binary left shift, for unsigned integers
    add_two_operand_rules("lsr", RU1, RU1, XC, IR_BSHR, AARCH64_OP_LSR, 3); // unsigned r >> c
    add_two_operand_rules("lsr", RU1, RU1, XR, IR_BSHR, AARCH64_OP_LSR, 3); // unsigned r >> r
}

// Add rules for dst={RI*, RU*}, SRC={RI1, RU1, RI2, RU2, ...} for all 6 comparison types
static void add_int_comparison_rules(void) {
    Rule *r;

    int operations[6] = {IR_EQ, IR_NE, IR_LT, IR_GT, IR_LE, IR_GE};

    for (int src_is_unsigned = 0; src_is_unsigned < 2; src_is_unsigned++) {
        for (int src_size = 1; src_size <= 4; src_size++) {
            int src = src_is_unsigned ? RU1 + src_size - 1: RI1 + src_size - 1;

            char *subs_template = add_size_to_template("subs %vd, %v1, %v2", src_size);

            char *templates[2][6] = {
                add_size_to_template("cset %vd, eq", src_size),
                add_size_to_template("cset %vd, ne", src_size),
                add_size_to_template("cset %vd, lt", src_size),
                add_size_to_template("cset %vd, gt", src_size),
                add_size_to_template("cset %vd, le", src_size),
                add_size_to_template("cset %vd, ge", src_size),

                add_size_to_template("cset %vd, eq", src_size),
                add_size_to_template("cset %vd, ne", src_size),
                add_size_to_template("cset %vd, lo", src_size),
                add_size_to_template("cset %vd, hi", src_size),
                add_size_to_template("cset %vd, ls", src_size),
                add_size_to_template("cset %vd, hs", src_size),
            };

            for (int operation = 0; operation < 6; operation++) {
                for (int dst_is_unsigned = 0; dst_is_unsigned < 2; dst_is_unsigned++) {
                    for (int dst_size = 1; dst_size <= 4; dst_size++) {
                        int dst = dst_is_unsigned ? RU1 + dst_size - 1: RI1 + dst_size - 1;

                        r = add_rule(dst, operations[operation], src, src, 12);
                        add_allocate_register_in_slot(r, 1, TYPE_CHAR + src_size - 1);
                        add_op(r, AARCH64_OP_SUB,  SV1, SRC1, SRC2, subs_template);
                        add_op(r, AARCH64_OP_CSET, DST, 0,    0,    templates[src_is_unsigned][operation]);
                    }
                }
            }
        }
    }
}

static void add_conditional_zero_jump_rule(int operation, int src1, int src2, int cost, int target_operation, char *comparison, char *conditional_jmp) {
    Rule *r = add_rule(0, operation, src1, src2, cost);
    add_op(r, AARCH64_OP_CMP, 0, SRC1, SRC2, comparison);
    add_op(r, target_operation, 0, SRC2, 0, conditional_jmp);
    fin_rule(r);
}

void define_rules(void) {
    Rule *r;

    instr_rule_count = 0;
    disable_merge_constants = 0;
    rule_coverage_file = 0;

    instr_rules = wcalloc(MAX_RULE_COUNT, sizeof(Rule));

    int ntc = AUTO_NON_TERMINAL_START;

    // Identity rules, for matching leaf nodes in the instruction tree
    r = add_rule(XC,       0, XC,       0, 0); fin_rule(r);
    r = add_rule(CADDSUB,  0, CADDSUB,  0, 0);
    r = add_rule(XR,       0, XR,       0, 0); fin_rule(r);
    r = add_rule(XM,       0, XM,       0, 0); fin_rule(r);
    r = add_rule(XRP,      0, XRP,      0, 0); fin_rule(r);
    r = add_rule(STL,      0, STL,      0, 0);
    r = add_rule(FUN,      0, FUN,      0, 0);
    r = add_rule(LAB,      0, LAB,      0, 0);

    // Load integer constants into registers
    r = add_rule(XR1, 0, XC1, 0, 1); add_op(r, AARCH64_OP_MOV_INT_CST, DST, SRC1, 0, NULL); fin_rule(r);
    r = add_rule(XR2, 0, XC2, 0, 1); add_op(r, AARCH64_OP_MOV_INT_CST, DST, SRC1, 0, NULL); fin_rule(r);
    r = add_rule(XR3, 0, XC3, 0, 1); add_op(r, AARCH64_OP_MOV_INT_CST, DST, SRC1, 0, NULL); fin_rule(r);
    r = add_rule(XR4, 0, XC4, 0, 2); add_op(r, AARCH64_OP_MOV_INT_CST, DST, SRC1, 0, NULL); fin_rule(r); // The cost is 2 to encourage loading into a 32-bit register if possible.

    // Load memory into registers
    r = add_rule(XRI, 0, XMI, 0, 2); add_op(r, AARCH64_OP_MOV, DST, SRC1, 0, "mov %vd, %v1"); fin_rule(r);
    r = add_rule(XRU, 0, XMU, 0, 2); add_op(r, AARCH64_OP_MOV, DST, SRC1, 0, "mov %vd, %v1"); fin_rule(r);
    r = add_rule(XRP, 0, MI4, 0, 2); add_op(r, AARCH64_OP_MOV, DST, SRC1, 0, "mov %vd, %v1"); fin_rule(r);
    r = add_rule(XRP, 0, MU4, 0, 2); add_op(r, AARCH64_OP_MOV, DST, SRC1, 0, "mov %vd, %v1"); fin_rule(r);

    // Register -> register move rules
    add_int_register_move_rules();

    // Operations
    // r + r and r - r
    add_two_operand_rules("add",  RI1, RI1, RI1, IR_ADD,  AARCH64_OP_ADD, 10);
    add_two_operand_rules("add",  RU1, RU1, RU1, IR_ADD,  AARCH64_OP_ADD, 10);
    add_two_operand_rules("sub",  RI1, RI1, RI1, IR_SUB,  AARCH64_OP_SUB, 10);
    add_two_operand_rules("sub",  RU1, RU1, RU1, IR_SUB,  AARCH64_OP_SUB, 10);

    // r + c and r - c where c can be encoded in the instruction
    add_two_operand_rules("add",  RI1, RI1, CADDSUB, IR_ADD, AARCH64_OP_ADD, 10);
    add_two_operand_rules("add",  RU1, RU1, CADDSUB, IR_ADD, AARCH64_OP_ADD, 10);
    add_two_operand_rules("sub",  RI1, RI1, CADDSUB, IR_SUB, AARCH64_OP_SUB, 10);
    add_two_operand_rules("sub",  RU1, RU1, CADDSUB, IR_SUB, AARCH64_OP_SUB, 10);

    add_two_operand_rules("mul",  RI1, RI1, RI1, IR_MUL,  AARCH64_OP_MUL, 30);
    add_two_operand_rules("mul",  RU1, RU1, RU1, IR_MUL,  AARCH64_OP_MUL, 30);
    add_two_operand_rules("sdiv", RI1, RI1, RI1, IR_DIV,  AARCH64_OP_DIV, 40);
    add_two_operand_rules("udiv", RU1, RU1, RU1, IR_DIV,  AARCH64_OP_DIV, 40);
    add_two_operand_rules("orr",  RI1, RI1, RI1, IR_BOR,  AARCH64_OP_BOR,  3);
    add_two_operand_rules("orr",  RU1, RU1, RU1, IR_BOR,  AARCH64_OP_BOR,  3);
    add_two_operand_rules("and",  RI1, RI1, RI1, IR_BAND, AARCH64_OP_BAND, 3);
    add_two_operand_rules("and",  RU1, RU1, RU1, IR_BAND, AARCH64_OP_BAND, 3);
    add_two_operand_rules("eor",  RI1, RI1, RI1, IR_XOR,  AARCH64_OP_XOR,  3);
    add_two_operand_rules("eor",  RU1, RU1, RU1, IR_XOR,  AARCH64_OP_XOR,  3);
    add_one_operand_rules("mvn", RI1, RI1, IR_BNOT, AARCH64_OP_BNOT, 3);
    add_one_operand_rules("mvn", RU1, RU1, IR_BNOT, AARCH64_OP_BNOT, 3);
    add_int_mod_rules();
    add_int_bitshift_rules();
    add_int_comparison_rules();

    // Jump rules
    r = add_rule(0, IR_JMP, LAB, 0,1);  add_op(r, AARCH64_OP_B, 0, SRC1, 0, "b %v1"); fin_rule(r);

    add_conditional_zero_jump_rule(IR_JZ,  XR, LAB, 3, AARCH64_OP_BEQ, "cmp %v1, 0",  "beq %v1");
    add_conditional_zero_jump_rule(IR_JNZ, XR, LAB, 3, AARCH64_OP_BNE, "cmp %v1, 0",  "bne %v1");

    add_early_testing_rules();

    if (ntc >= AUTO_NON_TERMINAL_END)
    panic("terminal rules exceeded: %d > %d\n", ntc, AUTO_NON_TERMINAL_END);
}
