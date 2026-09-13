#include "wcc.h"
#include "aarch64.h"

Set *allocate_return_value_live_ranges(void) {
    return new_set(LIVE_RANGE_PREG_V07 + 1);
}

int prepend_function_params(Function *function, Tac *ir) {
    return 0;
}

// Create a return value CVA for functions returning a struct or union.
void init_target_call_value_allocaton(Type *function_type, CallValueAllocation *cva) {
    if (function_type->target->type == TYPE_STRUCT_OR_UNION)
        initialize_function_return_value_cva(function_type);
}

// This implements the reverse of make_struct_or_union_to_abi_hfa_registers_move_instructions
// This is also used for moving struct/unions into function return value registers.
static int make_hfa_struct_or_union_move_from_registers_to_stack_instructions(
        Function *function, Tac *ir, CallValueLocation* cvl,
        int register_index, int stack_index, RegisterSet *register_set, int param_register_vreg) {

    if (debug_function_param_mapping)
        printf("Adding param move from int register %d size %d to stack_index %d, offset %d\n", register_index, cvl->stru_size, stack_index, cvl->stru_offset);

    if (!param_register_vreg) param_register_vreg = ++function->vreg_count;

    // Make param register value
    Value *param_register = new_value();
    param_register->vreg = param_register_vreg;
    int live_range_preg = register_set->fp_registers[register_index];
    param_register->live_range_preg = live_range_preg;

    int type = cvl->stru_size == 4 ? TYPE_FLOAT : TYPE_DOUBLE;
    param_register->type = new_type(type);
    Value *dst = new_value_in_stack(type, stack_index, cvl->stru_offset);
    new_tac_before(ir, IR_MOVE, dst, param_register, 0, 0);

    return live_range_preg;
}

// Add instructions to move struct/union data from a param register to a struct on the stack
// live_ranges
int add_struct_or_union_param_move(Function *function, Tac *ir, Type *type, CallValueLocations *cvl, RegisterSet *register_set) {
    Value *v = allocate_stack_space_for_type(function, ir, type);

    for (int loc = 0; loc < cvl->count; loc++) {
        CallValueLocation *location = &(cvl->locations[loc]);

        if (location->int_register != -1)
            make_int_struct_or_union_move_from_register_to_stack_instructions(function, ir, location, location->int_register, v->stack.index, register_set, 0);
        else if (location->fp_register != -1)
            make_hfa_struct_or_union_move_from_registers_to_stack_instructions(function, ir, location, location->fp_register, v->stack.index, register_set, 0);
        else
            panic("Got unexpected stack offset in add_struct_or_union_param_move()");
    }

    return v->stack.index;
}

void add_function_vararg_param_moves(Function *function, CallValueAllocation *cva, Tac *ir) {
    fprintf(stderr, "TODO aarch64 add_function_vararg_param_moves\n");
}

// Using the state of already allocated registers & stack entries in cva, determine the location for a non-composite type and set it in cvl.
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

static void add_struct_or_union_call_value_location(CallValueAllocation *cva, Type *type) {
    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
    if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

    StructOrUnionScalars *scalars = wmalloc(sizeof(StructOrUnionScalars));
    scalars->scalars = wmalloc(sizeof(StructOrUnionScalar *) * MAX_STRUCT_OR_UNION_SCALARS);
    scalars->count = 0;
    flatten_type(type, scalars, 0);

    int seen_floats = 0;
    int seen_doubles = 0;
    int seen_others = 0;

    // Check if the struct is a Homogeneous Floating-point Aggregate (HFA)
    for (int i = 0; i < scalars->count; i++) {
        StructOrUnionScalar *scalar = scalars->scalars[i];

        if (scalar->type->type == TYPE_FLOAT)
            seen_floats++;
        else if (scalar->type->type == TYPE_DOUBLE)
            seen_doubles++;
        else
            seen_others++;
    }

    int is_hfa = (
        !seen_others &&
        ((seen_floats <= 4 && !seen_doubles) || (seen_doubles <= 4 && !seen_floats))
    );
    int hfa_fits = (is_hfa && cva->single_fp_register_arg_count + scalars->count <= 8);

    int size = get_type_size(type);
    if (size > 16 && !hfa_fits) {
        // The entire thing is on the stack.
        // This may be an int or fp struct
        add_single_call_value_location(cva, type); // TODO aarch64
    }

    // aapcs64 C.2
    // If it's an HFA and there are enough FP registers left, use them
    else if (is_hfa && cva->single_fp_register_arg_count + scalars->count <= 8) {
        int member_size = seen_floats ? 4 : 8;

        CallValueLocations *cvl = wcalloc(1, sizeof(CallValueLocations));
        cvl->locations = wmalloc(sizeof(CallValueLocation) * scalars->count);
        cvl->count = scalars->count;

        for (int i = 0; i < scalars->count; i++) {
            cvl->locations[i].fp_register = cva->single_fp_register_arg_count + i;
            cvl->locations[i].stru_size = member_size;
            cvl->locations[i].stru_offset = i * member_size;
            add_type_to_cvl(cva, &(cvl->locations[i]), new_type(seen_floats ? TYPE_FLOAT : TYPE_DOUBLE), 0);
        }

        append_to_list(cva->locations, cvl);

        cva->single_fp_register_arg_count += scalars->count;
    }

    // aapcs64 C.3
    // Set all FP registers to allocated, round size up to 8 bytes,
    // and allocate stack space
    else if (is_hfa) {
        cva->single_fp_register_arg_count = 8;
        size = (size + 7) & ~7;
        add_single_call_value_location(cva, type);
    }
    else {
        // The struct goes either into int registers, of if there aren't enough,
        // on the stack.

        // aapcs64 C.12
        int needed_int_registers = (size + 7) / 8;

        if (cva->single_int_register_arg_count + needed_int_registers <= 8) {
            // The struct fits in integer registers

            CallValueLocations *cvl = wcalloc(1, sizeof(CallValueLocations));
            cvl->locations = wcalloc(needed_int_registers, sizeof(CallValueLocation));
            cvl->count = needed_int_registers;

            for (int i = 0; i < needed_int_registers; i++) {
                cvl->locations[i].stru_size = size > 8 ? 8 : size;
                size -= 8;
                cvl->locations[i].stru_offset = i * 8;
                add_type_to_cvl(cva, &(cvl->locations[i]), new_type(TYPE_LONG), 0);
            }

            append_to_list(cva->locations, cvl);

            cva->single_int_register_arg_count += needed_int_registers;
        }
        else {
            // The struct goes into the stack
            cva->single_int_register_arg_count = 8;
            add_single_call_value_location(cva, type);
        }
    }

    for (int i = 0; i < scalars->count; i++) wfree(scalars->scalars[i]);
    wfree(scalars->scalars);
    wfree(scalars);
}

// Add a type to a call value allocation
void add_type_to_cva(CallValueAllocation *cva, Type *type) {
    if (type->type == TYPE_INT128) {
        add_int128_call_value_locations(cva, type);
    }

    else if (type->type == TYPE_STRUCT_OR_UNION) {
        add_struct_or_union_call_value_location(cva, type);
    }

    else {
        // Create a single location for the arg
        add_single_call_value_location(cva, type);
    }
}

int *make_original_stack_indexes(Function *function) {
    int *result = wcalloc(function->vreg_count + 1, sizeof(int *));
    return result;
} // TODO aarch64

// Move floats/doubles from ABI encoded HFA in registers to the struct members in the stack
static int make_struct_or_union_to_abi_hfa_registers_move_instructions(Function *function, Tac *ir, Value *arg, int preg_class, int register_index,
    CallValueLocation *cvl, RegisterSet *register_set) {

    if (debug_function_arg_mapping) printf("Adding arg move from struct to SSE register_index=%d register size=%d\n", register_index, cvl->stru_size);

    if (cvl->stru_size != 4 && cvl->stru_size != 8)
        panic("Expected 4 or 8 stru size in make_struct_or_union_to_abi_hfa_registers_move_instructions()");

    int type = cvl->stru_size == 4 ? TYPE_FLOAT : TYPE_DOUBLE;
    Value *temp = load_struct_scalar_into_new_vreg(function, ir, arg, cvl, new_type(type));
    return add_arg_move_to_register(function, ir, new_type(type), temp, preg_class, register_index, register_set);
}

// Load an ABI register from a part of a struct or union
int make_struct_or_union_to_abi_move_instructions(
    Function *function, Tac *ir, Value *arg, int preg_class, int register_index,
    CallValueLocation *cvl, RegisterSet *register_set) {

    if (debug_function_arg_mapping)
        printf("Adding arg move from struct preg_class=%d register_index=%d register size=%d\n", preg_class, register_index, cvl->stru_size);

    if (preg_class == PC_INT) {
        return make_struct_or_union_to_abi_int_registers_move_instructions(function, ir, arg, preg_class, register_index, cvl, register_set);
    }
    else {
        return make_struct_or_union_to_abi_hfa_registers_move_instructions(function, ir, arg, preg_class, register_index, cvl, register_set);
    }
}

// Move struct/union data from registers or memory to the destination of a function call
static void add_function_call_result_moves_for_struct_or_union(Function *function, Tac *ir) {
    if (ir->dst->stack.index == 0) panic("Expected a stack index for a function call returning a struct/union");

    int stack_index = ir->dst->stack.index;
    Type *function_type = ir->src1->type;
    Value *function_value = ir->src1;

    CallValueAllocation *cva = function_type->function->return_value_cva;
    if (!cva) panic("In add_function_call_result_moves_for_struct_or_union() got an empty RV cva");
    CallValueLocations *cvl = cva->locations->elements[0];

    if (cvl->locations[0].stack_offset == -1) {
        // Move registers to a struct/union on the stack

        ir->dst = NULL;
        ir = ir->next;

        // Allocate result vregs and create a live range or them
        int *live_range_pregs = allocate_vregs_for_cvl(function, ir, cvl);

        for (int loc = 0; loc < cvl->count; loc++) {
            CallValueLocation *location = &(cvl->locations[loc]);

            int live_range_preg;
            int param_register_vreg = live_range_pregs[loc];

            if (location->int_register != -1)
                live_range_preg = make_int_struct_or_union_move_from_register_to_stack_instructions(
                    function, ir, location, location->int_register, stack_index,
                    &function_return_value_register_set, param_register_vreg);
            else if (location->fp_register != -1) {
                live_range_preg = make_hfa_struct_or_union_move_from_registers_to_stack_instructions(
                    function, ir, location, location->fp_register, stack_index,
                    &function_return_value_register_set, param_register_vreg);
            }
            else
                panic("Got unexpected stack offset in add_function_call_result_moves_for_struct_or_union");

            add_to_set(function_value->return_value_live_ranges, live_range_preg);
        }

        wfree(live_range_pregs);
    }
    else {
        panic("TODO add_function_call_result_moves_for_struct_or_union in stack"); // TODO aarch64
    }
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

        if (ir->dst && ir->dst->type->type == TYPE_INT128)
            add_function_call_result_moves_for_int128(function, ir, LIVE_RANGE_PREG_R00, LIVE_RANGE_PREG_R01);

        else if (ir->dst && ir->dst->type->type == TYPE_STRUCT_OR_UNION) {
            add_function_call_result_moves_for_struct_or_union(function, ir);
        }

        else {
            // Add move for integer, pointer, or FP
            int is_fp = is_floating_point_type(ir->dst->type);
            int live_range_preg = is_fp ? LIVE_RANGE_PREG_V00 : LIVE_RANGE_PREG_R00;
            add_function_result_moves_for_scalar(function, ir, live_range_preg);
        }
    }
}

// Move struct or union of size <= 16 into ABI registers
static void add_function_return_moves_for_struct_or_union(Function *function, Tac *ir) {
    // Determine registers
    CallValueAllocation *cva = init_call_value_allocaton(function->identifier);
    add_type_to_cva(cva, ir->src1->type);
    CallValueLocations *cvl = cva->locations->elements[0];

    if (cvl->locations[0].stack_offset != -1) {
        printf("TODO add_function_return_moves_for_struct_or_union in stack\n"); // TODO aarch64
    }
    else
        // Move the data into registers
        add_function_return_moves_for_struct_or_union_to_abi_register(function, ir);
}


static void add_function_return_moves(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation.id == IR_RETURN && !ir->src1) || ir->operation.id != IR_RETURN) continue;

        else if (ir->src1->type->type == TYPE_INT128)
            add_function_return_moves_for_int128(function, ir, LIVE_RANGE_PREG_R00, LIVE_RANGE_PREG_R01);

        else if (ir->src1->type->type == TYPE_STRUCT_OR_UNION)
            add_function_return_moves_for_struct_or_union(function, ir);

        else {
            int is_fp = is_floating_point_type(function->type->target);
            int live_range_preg = is_fp ? LIVE_RANGE_PREG_V00 : LIVE_RANGE_PREG_R00;
            add_function_return_moves_for_scalar(function, ir, live_range_preg);
        }
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

// Add instructions to copy a struct to the stack.
// TODO aarch64 this is now working for small structs, but not for large ones
static void add_function_call_arg_move_for_struct_or_union_to_stack(Function *function, Tac *tac) {
    int size = get_type_size(tac->src2->type);
    int rounded_up_size = (size + 7) & ~7;

    Value *arg = tac->src1;
    CallValueLocations *cvl = arg->function_call.function_call_arg_locations;
    if (cvl->count != 1) panic("Unexpected struct to stack move with locations->count != 1");
    int stack_offset = cvl->locations[0].stack_offset;

    Value *dst = new_value();
    dst->stack.index = stack_offset / 8 + 1; // Conventionally the first stack_index entry starts at 1
    dst->stack.area = SA_FUNCTION_ARGS;

    dst->type = make_pointer_to_void();
    dst->is_lvalue = 1;

    // Convert src to be a pointer to void
    Value *src = dup_value(tac->src2);
    src->type = make_pointer_to_void();

    if (debug_function_arg_mapping)
        printf("Adding memory copy for struct/union SI=%d rounded-up-size=%d\n", src->stack.index, rounded_up_size);

    add_memory_copy(function, tac, dst, src, size);

    make_instruction_a_nop(tac);
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
        add_function_call_arg_move_for_struct_or_union_to_stack(function, tac);
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

