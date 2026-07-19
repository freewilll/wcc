#include "wcc.h"
#include "aarch64.h"


Set *allocate_return_value_live_ranges(void) {
    return new_set(LIVE_RANGE_PREG_V07 + 1);
}

int prepend_function_params(Function *function) {
    return 0;
}

int add_struct_or_union_param_move(Function *function, Tac *ir, Type *type, FunctionParamLocations *pl, RegisterSet *register_set) {
    panic("TODO aarch64 add_struct_or_union_param_move");
}

void add_function_vararg_param_moves(Function *function, FunctionParamAllocation *fpa) {
    fprintf(stderr, "TODO aarch64 add_function_vararg_param_moves\n");
}

// Using the state of already allocated registers & stack entries in fpa, determine the location for a type and set it in fpl.
static void add_type_to_allocation(FunctionParamAllocation *fpa, FunctionParamLocation *fpl, Type *type, int force_stack) {
    fpl->int_register = -1;
    fpl->fp_register = -1;
    fpl->stack_offset = -1;
    fpl->stack_padding = -1;

    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
    if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

    if (get_preg_class_for_scalar_type(type) == PC_FP)
        panic("TODO aarch64 floating point params");

    int in_stack = 0;
    int is_single_int_register = 1;
    int is_single_fp_register = 0;

    int alignment = get_type_alignment(type);

    // TODO aarch64
    if (fpa->single_int_register_arg_count >= 8)
        panic("TODO aarch64 params in stack");

    fpl->int_register = fpa->single_int_register_arg_count < 6 ? fpa->single_int_register_arg_count : -1;

    if (debug_function_param_allocation && !in_stack && (is_single_int_register || is_single_fp_register)) {
        if (fpl->int_register != -1)
            printf("  arg %2d with alignment %2d     int reg %5d\n", fpa->param_locations->length, alignment, fpl->int_register);
        else
            printf("  arg %2d with alignment %2d     sse reg %5d\n", fpa->param_locations->length, alignment, fpl->fp_register);
    }

    fpa->single_int_register_arg_count += is_single_int_register;
    if (!in_stack && is_single_fp_register) fpa->single_fp_register_arg_count++;
}

static void add_single_stack_function_param_location(FunctionParamAllocation *fpa, Type *type) {
    FunctionParamLocations *fpl = wcalloc(1, sizeof(FunctionParamLocations));
    append_to_list(fpa->param_locations, fpl);
    fpl->locations = wmalloc(sizeof(FunctionParamLocation));
    fpl->count = 1;
    add_type_to_allocation(fpa, &(fpl->locations[0]), type, 0);
}

void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type) {
    if (type->type == TYPE_STRUCT_OR_UNION)
        panic("TODO aarch64 add_function_param_to_allocation for structs/unions");

    add_single_stack_function_param_location(fpa, type);
}

int *make_original_stack_indexes(Function *function) {
    int *result = wcalloc(function->vreg_count + 1, sizeof(int *));
    return result;
} // TODO aarch64

int make_struct_or_union_arg_move_instructions(Function *function, Tac *ir, Value *param, int preg_class, int register_index, FunctionParamLocation *location, RegisterSet *register_set) {
    panic("TODO aarch64: make_struct_or_union_arg_move_instructions");
}

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

        if (ir->dst && ir->dst->type->type == TYPE_STRUCT_OR_UNION || is_floating_point_type(ir->dst->type))
            panic("TODO aarch64: add_function_call_result_moves() function return value in callee for non-integer types");

        Value *value = dup_value(ir->dst);
        value->vreg = ++function->vreg_count;
        Tac *tac = new_instruction(IR_MOVE);
        tac->dst = ir->dst;

        tac->src1 = value;
        tac->src1->live_range_preg = LIVE_RANGE_PREG_R00;
        add_to_set(ir->src1->return_value_live_ranges, tac->src1->live_range_preg);

        ir->dst = value;
        insert_tac_before(ir->next, tac, 1);
    }
}

static void add_function_return_moves(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation.id == IR_RETURN && !ir->src1) || ir->operation.id != IR_RETURN) continue;

        if (ir->dst && (ir->dst->type->type == TYPE_STRUCT_OR_UNION || is_floating_point_type(ir->dst->type)))
            panic("TODO aarch64: add_function_return_moves() function return value in caller for non-integer types");

        int live_range_preg = LIVE_RANGE_PREG_R00;

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
    // Callee
    initialize_function_return_value_fpa(function->type);
    add_function_param_moves(function);
    add_function_return_moves(function);
    // process_function_varargs(function); // TODO aarch64

    // Caller
    process_function_call_arg_allocations(function);
    reverse_function_call_args_order(function);
    add_function_call_result_moves(function);
    add_function_call_arg_moves(function);
}

void add_function_call_clobbers(char *ig, int vreg_count, LongSet *livenow, Tac *tac) {
    // Clobber all temporaries
    for (int i = 0; i < clobbered_registers_in_function_call_count; i++)
        clobber_livenow(ig, vreg_count, livenow, tac, clobbered_registers_in_function_call[i]);

    // Integer arguments are clobbered, except for r0 and r1 which may be used for results.
    for (int i = 2; i < 8; i++)
        clobber_livenow(ig, vreg_count, livenow, tac, int_arg_registers[i]);

    // Unless the function returns something in r00, clobber r00
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_R00))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_R00);

    // Unless the function returns something in r01, clobber r01
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_R01))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_R01);

    // FP arguments are clobbered, except for v0 and v1 which may be used for results.
    for (int i = 2; i < 8; i++)
        clobber_livenow(ig, vreg_count, livenow, tac, fp_arg_registers[i]);

    // Unless the function returns something in v00, clobber v00
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_V00))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_V00);

    // Unless the function returns something in v01, clobber v01
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_V01))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_V01);
}

