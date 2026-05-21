#include "wcc.h"
#include "aarch64.h"


Set *allocate_return_value_live_ranges(void) {
    return new_set(LIVE_RANGE_PREG_REG_R28);
}

void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type) {} // TODO aarch64

int *make_original_stack_indexes(Function *function) {
    int *result = wcalloc(function->vreg_count + 1, sizeof(int *));
    return result;
} // TODO aarch64

// Move the result value of a function call to a vreg
// Convert v1 = IR_CALL... to:
// 1. v2 = IR_CALL...
// 2. v1 = v2
// Live range coalescing prevents the v1-v2 live range from getting merged with
// a special check for IR_CALL.
static void add_function_call_result_moves(Function *function) {
    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation.id != IR_CALL || !ir->dst) continue;

        fprintf(stderr, "TODO aarch64 add_function_call_result_moves() is untested\n");

        if (!is_integer_type(ir->dst->type))
            panic("TODO aarch64: add_function_call_result_moves() function return value for non-integer types");

        Value *value = dup_value(ir->dst);
        value->vreg = ++function->vreg_count;
        Tac *tac = new_instruction(IR_MOVE);
        tac->dst = ir->dst;

        tac->src1 = value;
        tac->src1->live_range_preg = LIVE_RANGE_PREG_REG_R00;
        add_to_set(ir->src1->return_value_live_ranges, tac->src1->live_range_preg);

        ir->dst = value;
        insert_tac_before(ir->next, tac, 1);
    }
}

static void add_function_return_moves(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation.id == IR_RETURN && !ir->src1) || ir->operation.id != IR_RETURN) continue;

        if (!is_integer_type(ir->src1->type))
            panic("TODO aarch64: add_function_return_moves() function return value for non-integer types");

        int live_range_preg = LIVE_RANGE_PREG_REG_R00;

        ir->src1->preferred_live_range_preg_index = live_range_preg;

        ir->dst = new_value();
        ir->dst->type = dup_type(function->type->target);
        if (ir->dst->type->type == TYPE_ENUM) ir->dst->type = new_type(TYPE_INT);
        ir->dst->vreg = ++function->vreg_count;
        ir->dst->live_range_preg = live_range_preg;

        new_tac_before(ir, IR_MOVE, ir->dst, ir->src1, 0, 1);

        ir->dst = 0;
        ir->src1 = 0;
        ir->src2 = 0;
    }
}

void process_target_functions(Function *function) {
    // TODO aarch64 lots more to do here
    add_function_return_moves(function);
    add_function_call_result_moves(function);
}

void add_function_call_clobbers(char *ig, int vreg_count, LongSet *livenow, Tac *tac) {} // TODO aarch64

