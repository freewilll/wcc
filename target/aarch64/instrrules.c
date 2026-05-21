#include "wcc.h"
#include "aarch64.h"

#define MAX_TARGET_OPS_PER_ROLE 32

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

    // Register register move
    r = add_rule(RI3,  IR_MOVE, RI3, 0, 1); add_op(r, AARCH64_OP_MOV,  DST, SRC1, 0, "mov %vdw, %v1w");

    // Add two RI3s
    r = add_rule(RI3,  IR_ADD, RI3, RI3, 1); add_op(r, AARCH64_OP_ADD,  DST, SRC1, SRC2, "add %vdw, %v1w, %v2w");

    if (ntc >= AUTO_NON_TERMINAL_END)
    panic("terminal rules exceeded: %d > %d\n", ntc, AUTO_NON_TERMINAL_END);
}
