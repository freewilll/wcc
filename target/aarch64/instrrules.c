#include "wcc.h"
#include "aarch64.h"

#define MAX_TARGET_OPS_PER_ROLE 32

// Take a template and convert %v* and %s placeholders and add b, w, l, q to them.
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

    TargetOperation *target_op = wmalloc(sizeof(TargetOperation));
    target_op->operation.id = operation;

    target_op->operation.is_move = (operation == AARCH64_OP_MOV);
    target_op->operation.is_call = (operation == AARCH64_OP_CALL);
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

    // Add two RI3s
    r = add_rule(RI3,  IR_ADD, RI3, RI3, 1); add_op(r, AARCH64_OP_ADD,  DST, SRC1, SRC2, "add %vdw, %v1w, %v2w");

    //  Multiply two RI3s
    r = add_rule(RI3,  IR_MUL, RI3, RI3, 1); add_op(r, AARCH64_OP_MUL,  DST, SRC1, SRC2, "mul %vdw, %v1w, %v2w");

    // Function calls
    r = add_rule(0,    IR_CALL, FUN, 0, 5); add_op(r, AARCH64_OP_CALL, DST, SRC1, 0, 0);
    r = add_rule(RI3,  IR_CALL, FUN, 0, 5); add_op(r, AARCH64_OP_CALL, DST, SRC1, 0, 0);

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

    // Integer Conversions
    r = add_rule(RU1, IR_MOVE, RI3, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "and %vdw, %v1w, 255");
    r = add_rule(RU2, IR_MOVE, RI3, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "and %vdw, %v1w, 65535");
    r = add_rule(RU3, IR_MOVE, RI3, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RU3, IR_MOVE, RI4, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "mov %vdx, %v1x");
    r = add_rule(RU4, IR_MOVE, RI4, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "mov %vdx, %v1x");
    r = add_rule(RI3, IR_MOVE, RI1, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "sxtb %vdw, %v1w");
    r = add_rule(RI3, IR_MOVE, RI2, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "sxth %vdw, %v1w");
    r = add_rule(RI3, IR_MOVE, RI4, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RI3, IR_MOVE, RU1, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "and %vdw, %v1w, 255");
    r = add_rule(RI3, IR_MOVE, RU2, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "and %vdw, %v1w, 65535");
    r = add_rule(RI3, IR_MOVE, RU3, 0, 1); add_op(r, AARCH64_OP_AND, DST, SRC1, 0, "mov %vdw, %v1w");

    // Register register move
    r = add_rule(RI1,  IR_MOVE, RI1, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RI2,  IR_MOVE, RI2, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RI3,  IR_MOVE, RI3, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RU3,  IR_MOVE, RU3, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RI4,  IR_MOVE, RI4, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdx, %v1x");
    r = add_rule(RI4,  IR_MOVE, RU4, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdx, %v1x");
    r = add_rule(RU2,  IR_MOVE, RU2, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RU3,  IR_MOVE, RU3, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RU4,  IR_MOVE, RU4, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdx, %v1x");
    r = add_rule(RP4,  IR_MOVE, RP4, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdx, %v1x");
}

void define_rules(void) {
    Rule *r;

    instr_rule_count = 0;
    disable_merge_constants = 0;
    rule_coverage_file = 0;

    instr_rules = wcalloc(MAX_RULE_COUNT, sizeof(Rule));

    int ntc = AUTO_NON_TERMINAL_START;

    // Identity rules, for matching leaf nodes in the instruction tree
    r = add_rule(XC,  0, XC,  0, 0); fin_rule(r);
    r = add_rule(XR,  0, XR,  0, 0); fin_rule(r);
    r = add_rule(XM,  0, XM,  0, 0); fin_rule(r);
    r = add_rule(XRP, 0, XRP, 0, 0); fin_rule(r);
    r = add_rule(STL, 0, STL, 0, 0);
    r = add_rule(FUN, 0, FUN, 0, 0);

    // Load integer constants into registers
    r = add_rule(XR1, 0, XC1, 0, 1); add_op(r, AARCH64_OP_MOV_INT_CST, DST, SRC1, 0, NULL); fin_rule(r);
    r = add_rule(XR2, 0, XC2, 0, 1); add_op(r, AARCH64_OP_MOV_INT_CST, DST, SRC1, 0, NULL); fin_rule(r);
    r = add_rule(XR3, 0, XC3, 0, 1); add_op(r, AARCH64_OP_MOV_INT_CST, DST, SRC1, 0, NULL); fin_rule(r);
    r = add_rule(XR4, 0, XC4, 0, 2); add_op(r, AARCH64_OP_MOV_INT_CST, DST, SRC1, 0, NULL); fin_rule(r); // The cost is 2 to encourage loading into a 32-bit register if possible

    // Load memory into registers
    r = add_rule(XRI, 0, XMI, 0, 2); add_op(r, AARCH64_OP_MOV, DST, SRC1, 0, "mov %vd, %v1"); fin_rule(r);
    r = add_rule(XRU, 0, XMU, 0, 2); add_op(r, AARCH64_OP_MOV, DST, SRC1, 0, "mov %vd, %v1"); fin_rule(r);
    r = add_rule(XRP, 0, MI4, 0, 2); add_op(r, AARCH64_OP_MOV, DST, SRC1, 0, "mov %vd, %v1"); fin_rule(r);
    r = add_rule(XRP, 0, MU4, 0, 2); add_op(r, AARCH64_OP_MOV, DST, SRC1, 0, "mov %vd, %v1"); fin_rule(r);

    add_early_testing_rules();

    if (ntc >= AUTO_NON_TERMINAL_END)
    panic("terminal rules exceeded: %d > %d\n", ntc, AUTO_NON_TERMINAL_END);
}
