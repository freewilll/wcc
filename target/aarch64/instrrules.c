#include "wcc.h"
#include "aarch64.h"

#define MAX86_OP_AARCH64_OPERATION_PER_RULE 32

// Add an TargetOperation template to a rule's linked list, making a copy
static TargetOperation *add_aarch64_op_to_rule(Rule *r, TargetOperation *x86op) {
    if (!r->target_operation_count)
        r->target_operations = wmalloc(MAX86_OP_AARCH64_OPERATION_PER_RULE * sizeof(TargetOperation));

    if (r->target_operation_count == MAX86_OP_AARCH64_OPERATION_PER_RULE) panic("Exceeded MAX86_OP_AARCH64_OPERATION_PER_RULE");

    int index = r->target_operation_count++;
    r->target_operations[index] = *x86op;
    return &r->target_operations[index];
}

// Add an x86 operation template to a rule
static TargetOperation *add_op(Rule *r, int operation, int dst, int v1, int v2, char *template) {
    if (operation < TARGET_OPS_START) panic("Operation %s is not a target operation", operation_string(operation));

    TargetOperation *x86op = wmalloc(sizeof(TargetOperation));
    x86op->operation.id = operation;

    x86op->operation.is_move = (operation == AARCH64_OP_MOV);

    x86op->dst = dst;
    x86op->v1 = v1;
    x86op->v2 = v2;

    x86op->template = template;
    x86op->save_value_in_slot = 0;
    x86op->allocate_stack_index_in_slot = 0;
    x86op->allocate_register_in_slot = 0;
    x86op->allocate_label_in_slot = 0;
    x86op->allocated_type = 0;
    x86op->arg = 0;

    x86op = add_aarch64_op_to_rule(r, x86op);

    return x86op;
}

void define_rules(void) {
    Rule *r;

    instr_rule_count = 0;
    disable_merge_constants = 0;
    rule_coverage_file = 0;

    instr_rules = wcalloc(MAX_RULE_COUNT, sizeof(Rule));

    int ntc = AUTO_NON_TERMINAL_START;

    // TODO aarch64 lots, lots and lots more

    // Identity rules, for matching leaf nodes in the instruction tree
    r = add_rule(RI3,    0, RI3,    0, 0);
    r = add_rule(CI3,    0, CI3,    0, 0);

    // Load constant into register
    r = add_rule(RI3,  IR_MOVE, CI3, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");
    r = add_rule(RI3,  IR_MOVE, CI4, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");

    // Add two RI3s
    r = add_rule(RI3,  IR_ADD, RI3, RI3, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, SRC2, "add %vdw, %v1w, %v2w");

    if (ntc >= AUTO_NON_TERMINAL_END)
    panic("terminal rules exceeded: %d > %d\n", ntc, AUTO_NON_TERMINAL_END);
}
