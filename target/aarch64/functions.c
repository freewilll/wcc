#include "wcc.h"
#include "aarch64.h"

Set *allocate_return_value_live_ranges(void) {
    return new_set(LIVE_RANGE_PREG_V07 + 1);
}

int prepend_function_params(Function *function, Tac *ir) {
    return 0;
}

int add_struct_or_union_param_move(Function *function, Tac *ir, Type *type, CallValueLocations *pl, RegisterSet *register_set) {
    panic("TODO aarch64 add_struct_or_union_param_move");
}

void add_function_vararg_param_moves(Function *function, CallValueAllocation *cva, Tac *ir) {
    fprintf(stderr, "TODO aarch64 add_function_vararg_param_moves\n");
}

// Using the state of already allocated registers & stack entries in cva, determine the location for a type and set it in cvl.
void add_type_to_cvl(CallValueAllocation *cva, CallValueLocation *cvl, Type *type, int force_stack) {
    cvl->int_register = -1;
    cvl->fp_register = -1;
    cvl->stack_offset = -1;
    cvl->stack_padding = -1;

    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
    if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

    int is_single_int_register = type_fits_in_single_int_register(type);
    int is_single_fp_register = is_floating_point_type(type);
    int in_stack =
        force_stack ||
        (is_single_int_register && cva->single_int_register_arg_count >= 8) || (is_single_fp_register && cva->single_fp_register_arg_count >= 8);

    int alignment = get_type_alignment(type);
    if (alignment < 8) alignment = 8;

    if (!in_stack && is_single_int_register)
        cvl->int_register = cva->single_int_register_arg_count < 8 ? cva->single_int_register_arg_count : -1;

    else if (!in_stack && is_single_fp_register)
        cvl->fp_register = cva->single_fp_register_arg_count < 8 ? cva->single_fp_register_arg_count : -1;

    else {
        // It's on the stack
        add_type_to_cvl_in_stack(cva, cvl, type, alignment);

        if (debug_call_value_allocation)
            printf("  arg %2d with alignment %2d     offset 0x%04x with padding 0x%04x\n", cva->locations->length, alignment, cvl->stack_offset, cvl->stack_padding);
    }

    if (debug_call_value_allocation && !in_stack && (is_single_int_register || is_single_fp_register)) {
        if (cvl->int_register != -1)
            printf("  arg %2d with alignment %2d     int reg %5d\n", cva->locations->length, alignment, cvl->int_register);
        else
            printf("  arg %2d with alignment %2d     fp  reg %5d\n", cva->locations->length, alignment, cvl->fp_register);
    }

    cva->single_int_register_arg_count += is_single_int_register;
    if (!in_stack && is_single_fp_register) cva->single_fp_register_arg_count++;
}

// Add a type to a call value allocation
void add_type_to_cva(CallValueAllocation *cva, Type *type) {
    if (type->type == TYPE_INT128) {
        add_int128_call_value_locations(cva, type);
    }

    else if (type->type != TYPE_STRUCT_OR_UNION) {
        // Create a single location for the arg
        add_single_call_value_location(cva, type);
    }
    else {
        panic("TODO aarch64 add_type_to_cva for structs/unions");
    }
}

int *make_original_stack_indexes(Function *function) {
    int *result = wcalloc(function->vreg_count + 1, sizeof(int *));
    return result;
} // TODO aarch64

int make_struct_or_union_arg_move_instructions(Function *function, Tac *ir, Value *param, int preg_class, int register_index, CallValueLocation *location, RegisterSet *register_set) {
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

        if (ir->dst && ir->dst->type->type == TYPE_STRUCT_OR_UNION)
            panic("TODO aarch64: add_function_call_result_moves() function return value in callee for composite types");

        Value *value = dup_value(ir->dst);
        value->vreg = ++function->vreg_count;
        Tac *tac = new_instruction(IR_MOVE);
        tac->dst = ir->dst;

        int is_fp = is_floating_point_type(ir->dst->type);
        tac->src1 = value;
        tac->src1->live_range_preg = is_fp ? LIVE_RANGE_PREG_V00 : LIVE_RANGE_PREG_R00;
        add_to_set(ir->src1->return_value_live_ranges, tac->src1->live_range_preg);

        ir->dst = value;
        insert_tac_before(ir->next, tac, 1);
    }
}

static void add_function_return_moves(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation.id == IR_RETURN && !ir->src1) || ir->operation.id != IR_RETURN) continue;

        if (ir->dst && (ir->dst->type->type == TYPE_STRUCT_OR_UNION))
            panic("TODO aarch64: add_function_return_moves() function return value in caller for composite types");

        int is_fp = is_floating_point_type(function->type->target);
        int live_range_preg = is_fp ? LIVE_RANGE_PREG_V00 : LIVE_RANGE_PREG_R00;

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

// Add instructions to copy an int128 from a register/stack to the stack
static void add_function_call_arg_move_for_int128_to_stack(Function *function, Tac *tac) {
    if (tac->src2->stack.index)
        panic("Got unexpected stack index %d in add_function_call_arg_move_for_int128_to_stack", tac->src2->stack.index);

    Value *arg = tac->src1;
    CallValueLocations *cvl = arg->function_call.function_call_arg_locations;
    if (cvl->count != 1) panic("Unexpected int128 to stack move with locations->count != 1");
    int stack_offset = cvl->locations[0].stack_offset;

    SplitVreg *split_vreg = function->int128_register_mappings[tac->src2->vreg];
    if (!split_vreg) panic("NULL pointer when fetching split vreg for int128 for vreg %d", tac->src2->vreg);

    if (debug_function_arg_mapping)
        printf("Adding copy from vregs for int128 vreg=%d, split %d / %d\n", tac->src2->vreg, split_vreg->low, split_vreg->high);

    Value *src2_low = dup_value(tac->src2);
    src2_low->type->type =TYPE_LONG;
    src2_low->vreg = split_vreg->low;

    Value *src2_high = dup_value(tac->src2);
    src2_high->type->type =TYPE_LONG;
    src2_high->vreg = split_vreg->high;

    Value *dst_low = new_value();
    dst_low->type = dup_type(tac->src2->type);
    dst_low->type->type = TYPE_LONG;
    dst_low->stack.index = stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1
    dst_low->stack.area = SA_FUNCTION_ARGS;

    Value *dst_high = dup_value(dst_low);
    dst_high->stack.index++;

    tac->operation.id = IR_MOVE;
    tac->dst = dst_low;
    tac->src1 = src2_low;
    tac->src2 = 0;

    new_tac_after(tac, IR_MOVE, dst_high, src2_high, NULL);
}

// Move a single register to the arg stack
static void add_single_register_arg_move_to_stack(Function *function, Tac *tac) {
    Value *arg = tac->src1;
    CallValueLocations *cvl = arg->function_call.function_call_arg_locations;
    if (cvl->count != 1) panic("Unexpected int128 to stack move with locations->count != 1");
    int stack_offset = cvl->locations[0].stack_offset;

    Value *dst = new_value();
    dst->type = dup_type(tac->src2->type);
    dst->stack.index = stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1
    dst->stack.area = SA_FUNCTION_ARGS;

    tac->operation.id = IR_MOVE;
    tac->dst = dst;
    tac->src1 = tac->src2;
    tac->src2 = 0;
}

// Arguments in function calls that go to the stack are placed in a reserved stack
// SA_FUNCTION_ARGS area. Codegen needs this to distinguish params on the stack
// from args on the stack.
void convert_target_arg_move_to_stack_instructions(Function *function, Tac *tac) {
    if (tac->src2->type->type == TYPE_INT128) {
        // Add memory copies for int128
        add_function_call_arg_move_for_int128_to_stack(function, tac);
    }
    else if (tac->src2->type->type == TYPE_STRUCT_OR_UNION) {
        // Add memory copies for struct and unions
        panic("TODO aarch64: arg move for struct or union");
    }
    else {
        // Move a single register to the arg stack
        add_single_register_arg_move_to_stack(function, tac);
    }
}

void process_target_functions(Function *function) {
    // Callee
    initialize_function_return_value_cva(function->type);
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

