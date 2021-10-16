#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

// Details about a single scalar in a struct/union
typedef struct struct_or_union_scalar {
    Type *type;
    int offset;
} StructOrUnionScalar;

// A list of StructOrUnionScalar
typedef struct struct_or_union_scalars {
    StructOrUnionScalar **scalars;
    int count;
} StructOrUnionScalars;

static int make_struct_or_union_arg_move_instructions(
        Function *function, Tac *ir, Value *param, int preg_class, int register_index,
        FunctionParamLocation *location, RegisterSet *register_set);

static Value *make_param_dst_on_stack(int type, int stack_index, int offset) {
    Value *dst = new_value();

    dst->type = new_type(type);
    dst->is_lvalue = 1;
    dst->stack_index = stack_index;
    dst->offset = offset;
    return dst;
}

// This implements the reverse of make_int_struct_or_union_arg_move_instructions
// This is also used for moving struct/unions into function return value registers
static int make_int_struct_or_union_move_from_register_to_stack_instructions(
        Function *function, Tac *ir, Type *type, FunctionParamLocation* pl,
        int register_index, int stack_index, RegisterSet *register_set, int param_register_vreg) {

    if (debug_function_param_mapping)
        printf("Adding param move from int register %d size %d to stack_index %d, offset %d\n", register_index, pl->stru_size, stack_index, pl->stru_offset);

    if (!param_register_vreg) param_register_vreg = ++function->vreg_count;

    // Make shift register
    Value *shift_register = new_value();
    shift_register->vreg = param_register_vreg;
    int live_range_preg = register_set->int_registers[register_index];
    shift_register->live_range_preg = live_range_preg;

    int size = pl->stru_size;
    int offset = 0;

    for (int i = 3; i >= 0; i--) {
        int size_unit = 1 << i;
        if (size_unit > size) continue;

        // Make dst on stack
        Value *dst = new_value();
        dst->type = new_type(TYPE_CHAR + i);
        dst->type->is_unsigned = 1;
        dst->is_lvalue = 1;
        dst->stack_index = stack_index;
        dst->offset = pl->stru_offset + offset;

        shift_register = dup_value(shift_register);
        shift_register->type = new_type(TYPE_CHAR + i);
        shift_register->type->is_unsigned = 1;

        insert_instruction_from_operation(ir, IR_MOVE, dst, shift_register, 0, 1);

        size -= size_unit;
        offset += size_unit;
        if (size == 0) break;

        // Shift the register over size_unit * 8 bytes
        shift_register = dup_value(shift_register);
        shift_register->type = new_type(TYPE_LONG);

        Value *new_shift_register = dup_value(shift_register);
        new_shift_register->live_range_preg = 0;

        insert_instruction_from_operation(ir, IR_BSHR, new_shift_register, shift_register, new_integral_constant(TYPE_LONG, size_unit * 8), 1);

        shift_register = new_shift_register;
    }

    return live_range_preg;
}

// This implements the reverse of make_sse_struct_or_union_arg_move_instructions
static int make_sse_struct_or_union_move_from_register_to_stack_instructions(
        Function *function, Tac *ir, Type *type, FunctionParamLocation* pl,
        int register_index, int stack_index, RegisterSet *register_set, int param_register_vreg) {

    if (debug_function_param_mapping)
        printf("Adding param move from sse register %d size %d to stack_index %d, offset %d\n", register_index, pl->stru_size, stack_index, pl->stru_offset);

    if (!param_register_vreg) param_register_vreg = ++function->vreg_count;

    // Make param register value
    Value *param_register = new_value();
    param_register->vreg = param_register_vreg;
    int live_range_preg = register_set->sse_registers[register_index];
    param_register->live_range_preg = live_range_preg;

    if (pl->stru_size == 4) {
        // Move a single float
        param_register->type = new_type(TYPE_FLOAT);
        Value *dst = make_param_dst_on_stack(TYPE_FLOAT, stack_index, pl->stru_offset);
        insert_instruction_from_operation(ir, IR_MOVE, dst, param_register, 0, 1);
    }

    else if (pl->stru_size == 8 && pl->stru_member_count == 1) {
        // Move a single double
        param_register->type = new_type(TYPE_DOUBLE);
        Value *dst = make_param_dst_on_stack(TYPE_DOUBLE, stack_index, pl->stru_offset);
        insert_instruction_from_operation(ir, IR_MOVE, dst, param_register, 0, 1);
    }

    else {
        // Move two floats. It must first be copied into an integer register and then
        // copied to the stack.
        param_register->type = new_type(TYPE_DOUBLE);
        Value *temp_int = new_value();
        temp_int->type = new_type(TYPE_LONG);
        temp_int->vreg = ++function->vreg_count;
        insert_instruction_from_operation(ir, IR_MOVE_PREG_CLASS, temp_int, param_register, 0, 1);

        Value *dst = make_param_dst_on_stack(TYPE_LONG, stack_index, pl->stru_offset);
        insert_instruction_from_operation(ir, IR_MOVE, dst, temp_int, 0, 1);
    }

    return live_range_preg;
}

// Move struct/union data from registers or memory to the destination of a function call
static void add_function_call_result_moves_for_struct_or_union(Function *function, Tac *ir) {
    if (ir->dst->stack_index == 0) panic("Expected a stack index for a function call returning a struct/union");

    int stack_index = ir->dst->stack_index;
    Type *type = ir->dst->type;

    Type *function_type = ir->src1->function_symbol->type;

    FunctionParamAllocation *fpa = function_type->function->return_value_fpa;
    FunctionParamLocations *fpl = &(fpa->params[0]);

    Value *function_value = ir->src1;

    if (fpl->locations[0].stack_offset == -1) {
        // Move registers to a struct/union on the stack

        ir->dst = 0;
        ir = ir->next;

        // Allocate result vregs and create a live range or them
        int *live_range_pregs = malloc(sizeof(int) * 8);
        for (int loc = 0; loc < fpl->count; loc++) {
            FunctionParamLocation *location = &(fpl->locations[loc]);
            live_range_pregs[loc] = ++function->vreg_count;
            Value *dst = new_value();
            dst->vreg = live_range_pregs[loc];
            dst->type = location->int_register != -1 ? new_type(TYPE_LONG) : new_type(TYPE_DOUBLE);
            insert_instruction_from_operation(ir, IR_CALL_ARG_REG, dst, 0, 0, 1);
        }

        for (int loc = 0; loc < fpl->count; loc++) {
            FunctionParamLocation *location = &(fpl->locations[loc]);

            int live_range_preg;
            int param_register_vreg = live_range_pregs[loc];
            if (location->int_register != -1)
                live_range_preg = make_int_struct_or_union_move_from_register_to_stack_instructions(
                    function, ir, type, location, location->int_register, stack_index,
                    &function_return_value_register_set, param_register_vreg);
            else if (location->sse_register != -1)
                live_range_preg = make_sse_struct_or_union_move_from_register_to_stack_instructions(
                    function, ir, type, location, location->sse_register, stack_index,
                    &function_return_value_register_set, param_register_vreg);
            else
                panic("Got unexpected stack offset in add_struct_or_union_param_move");

            add_to_set(function_value->return_value_live_ranges, live_range_preg);
        }
    }
    else {
        // Move registers to a struct/union in memory. rax contains the address

        // Ensure RAX doesn't get clobbered by the function call
        add_to_set(function_value->return_value_live_ranges, LIVE_RANGE_PREG_RAX_INDEX);

        // Take address of dst struct/union on the stack
        Value *address_value = new_value();
        address_value->vreg = ++function->vreg_count;
        address_value->type = make_pointer_to_void();
        insert_instruction_from_operation(ir, IR_ADDRESS_OF, address_value, ir->dst, 0, 1);

        // Move struct/union target address into rdi
        Value *rdi_value = new_value();
        rdi_value->vreg = ++function->vreg_count;
        rdi_value->type = make_pointer_to_void();
        rdi_value->live_range_preg = LIVE_RANGE_PREG_RDI_INDEX;
        insert_instruction_from_operation(ir, IR_MOVE, rdi_value, address_value, 0, 1);

        // Setup dst for rax
        Value *struct_dst = ir->dst;

        ir->dst = new_value();
        ir->dst->vreg = ++function->vreg_count;
        ir->dst->type = make_pointer_to_void();
        ir->dst->live_range_preg = LIVE_RANGE_PREG_RAX_INDEX;

        // Prepare src1 to also be rax, but as an lvalue
        Value *src1 = dup_value(ir->dst);
        src1->is_lvalue = 1;

        // Add the memory copy
        int size = get_type_size(type);
        ir = add_memory_copy(function, ir, struct_dst, src1, size);
    }
}

// Convert v1 = IR_CALL... to:
// 1. v2 = IR_CALL...
// 2. v1 = v2
// Live range coalescing prevents the v1-v2 live range from getting merged with
// a special check for IR_CALL.
void add_function_call_result_moves(Function *function) {
    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation != IR_CALL || !ir->dst) continue;

        if (ir->dst->type->type == TYPE_STRUCT_OR_UNION)
            add_function_call_result_moves_for_struct_or_union(function, ir);

        else if (ir->dst->type->type != TYPE_LONG_DOUBLE) {
            // Add move for integer, pointer, or SSE
            Value *value = dup_value(ir->dst);
            value->vreg = ++function->vreg_count;
            Tac *tac = new_instruction(IR_MOVE);
            tac->dst = ir->dst;

            int is_sse = is_sse_floating_point_type(ir->dst->type);
            tac->src1 = value;
            tac->src1->live_range_preg = is_sse ? LIVE_RANGE_PREG_XMM00_INDEX : LIVE_RANGE_PREG_RAX_INDEX;
            add_to_set(ir->src1->return_value_live_ranges, tac->src1->live_range_preg);

            ir->dst = value;
            insert_instruction(ir->next, tac, 1);
        }
    }
}

// Add IR_CALL_ARG_REG instructions that don't do anything, but ensure
// that the interference graph and register selection code create
// a live range for the interval between the function register assignment and
// the function call. Without this, there is a chance that function call
// registers are used as temporaries during the code instructions
// emitted above.
static void add_ir_call_reg_instructions(Tac *ir, Value **function_call_values, int count) {
    for (int i = 0; i < count; i++)
        if (function_call_values[i])
            insert_instruction_from_operation(ir, IR_CALL_ARG_REG, 0, function_call_values[i], 0, 1);
}

// Move struct or union of size <= 32 into rax/rdx or xmm0/xmm1
static void add_function_return_moves_for_struct_or_union(Function *function, Tac *ir, char *identifier) {
    // Determine registers
    FunctionParamAllocation *fpa = init_function_param_allocaton(identifier);
    add_function_param_to_allocation(fpa, ir->src1->type);
    FunctionParamLocations *fpl = &(fpa->params[0]);

    Value **function_call_values = malloc(sizeof(Value *) * 2);
    memset(function_call_values, 0, sizeof(Value *) * 2);

    if (fpl->locations[0].stack_offset != -1) {
        // Move data into memory

        ir->operation = IR_NOP;

        // Convert src1 to be a pointer to void
        Value *src1 = dup_value(ir->src1);
        src1->type = make_pointer_to_void();
        Value *dst = dup_value(function->return_value_pointer);
        dst->is_lvalue = 1;

        int size = get_type_size(ir->src1->type);
        ir = add_memory_copy(function, ir, dst, src1, size);

        // Add move of the target pointer to rax
        dst = new_value();

        dst->type = make_pointer_to_void();
        dst->live_range_preg = LIVE_RANGE_PREG_RAX_INDEX;
        dst->type = make_pointer_to_void();
        dst->vreg = ++function->vreg_count;

        ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, function->return_value_pointer, 0);
        ir = insert_instruction_after_from_operation(ir, IR_RETURN, 0, 0, 0);
    }
    else {
        // Move the data into registers
        for (int loc = fpl->count - 1; loc >= 0; loc--) {
            FunctionParamLocation *location = &(fpl->locations[loc]);
            int preg_class = (location->int_register != -1) ? PC_INT : PC_SSE;
            int register_index = (preg_class == PC_INT) ? location->int_register : location->sse_register;
            Value *param = ir->src1;
            int vreg = make_struct_or_union_arg_move_instructions(function, ir, param, preg_class, register_index, location, &function_return_value_register_set);
            function_call_values[loc] = new_value();
            function_call_values[loc]->type = preg_class == PC_INT ? new_type(TYPE_LONG) : new_type(TYPE_DOUBLE);
            function_call_values[loc]->vreg = vreg;
        }

        // Add instructions to ensure the values stay in the registers
        add_ir_call_reg_instructions(ir, function_call_values, 2);
    }
}

// Add a move for a function return value. If it's a long double, a load can be done,
// otherwise, either rax or xmm0 must hold the result.
void add_function_return_moves(Function *function, char *identifier) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation == IR_RETURN && !ir->src1) || ir->operation != IR_RETURN) continue;

        // Implicit else, operation == IR_RETURN & ir-src1 has a value
        if (ir->src1->type->type == TYPE_LONG_DOUBLE) {
            insert_instruction_from_operation(ir, IR_LOAD_LONG_DOUBLE, 0, ir->src1, 0, 1);

            ir->operation = IR_RETURN;
            ir->dst = 0;
            ir->src1 = 0;
            ir->src2 = 0;
        }

        else if (ir->src1->type->type == TYPE_STRUCT_OR_UNION)
            add_function_return_moves_for_struct_or_union(function, ir, identifier);

        else {
            int is_sse = is_sse_floating_point_type(function->return_type);
            int live_range_preg = is_sse ? LIVE_RANGE_PREG_XMM00_INDEX : LIVE_RANGE_PREG_RAX_INDEX;

            ir->src1->preferred_live_range_preg_index = live_range_preg;

            ir->dst = new_value();
            ir->dst->type = dup_type(function->return_type);
            ir->dst->vreg = ++function->vreg_count;
            ir->dst->live_range_preg = live_range_preg;
            ir->src1->preferred_live_range_preg_index = live_range_preg;

            insert_instruction_from_operation(ir, IR_MOVE, ir->dst, ir->src1, 0, 1);

            ir->operation = IR_RETURN;
            ir->dst = 0;
            ir->src1 = 0;
            ir->src2 = 0;
        }
    }
}

// Add a IR_MOVE instruction from a value to a function call register
// function_call_*_register_arg_index ensures that dst will become the actual
// x86_64 physical register rdi, rsi, etc
static int add_arg_move_to_register(Function *function, Tac *ir, Type *type, Value *param, int preg_class, int register_index, RegisterSet *register_set) {
    int *arg_registers = preg_class == PC_INT ? int_arg_registers : sse_arg_registers;

    Tac *tac = new_instruction(IR_MOVE);

    // dst
    tac->dst = new_value();
    tac->dst->type = dup_type(type);
    tac->dst->vreg = ++function->vreg_count;
    tac->dst->live_range_preg = preg_class == PC_INT ? register_set->int_registers[register_index] : register_set->sse_registers[register_index];

    // src
    tac->src1 = param;
    tac->src1->preferred_live_range_preg_index = arg_registers[register_index];

    if (debug_function_arg_mapping) printf("Adding arg move from register for preg-class=%d register_index=%d\n", preg_class, register_index);

    insert_instruction(ir, tac, 1);

    return tac->dst->vreg;
}

// Load a scalar in a struct into a register. The scalar can be either a local, global, or lvalue in register
// If it's an lvalue in a register, it is indirected, otherwise moved.
static void load_struct_scalar_into_temp(Function *function, Tac *ir, Value *param, FunctionParamLocation *pl, Type *type, Value *temp, int offset) {
    int lvalue_in_register = param->is_lvalue && param->vreg;
    Value *src1 = dup_value(param);
    src1->type = type;
    src1->offset += pl->stru_offset + offset;

    insert_instruction_from_operation(ir, lvalue_in_register ? IR_INDIRECT : IR_MOVE, temp, src1, 0, 1);
}

// Load a scalar in a struct into a register. The scalar can be either a local, global, or lvalue in register
// If it's an lvalue in a register, it is indirected, otherwise moved.
static Value *load_struct_scalar(Function *function, Tac *ir, Value *param, FunctionParamLocation *pl, Type *type) {
    Value *temp = new_value();
    temp->type = type;
    temp->vreg = ++function->vreg_count;

    load_struct_scalar_into_temp(function, ir, param, pl, temp->type, temp, 0);

    return temp;
}

static Value *make_long_temp(Function *function) {
    Value *result = new_value();

    result->type = new_type(TYPE_LONG);
    result->type->is_unsigned = 1;
    result->vreg = ++function->vreg_count;

    return result;
}

// Load an 8-byte into an integer register. In the best case, a single move instruction
// is produced. In the worst case, 3 load instructions with 3 bit shifts & 3 bitwise ors.
// Try sizes in order of 8, 4, 2, 1.
// The code produced:
// size = 1     load char
// size = 2     load short
// size = 3     load short, load char, shift char, or char
// size = 4     load int
// size = 5     load int, load char, shift char, or char
// size = 6     load int, load short, shift short, or short
// size = 7     load int, load short, shift short, or short, load char, shift char, or char
// size = 8     load long
static int make_int_struct_or_union_arg_move_instructions(
    Function *function, Tac *ir, Value *param, int preg_class, int register_index,
    FunctionParamLocation *pl, RegisterSet *register_set) {

    // Make the shift register
    Value *result_register = new_value();
    result_register->type = new_type(TYPE_LONG);
    result_register->type->is_unsigned = 1;
    result_register->vreg = ++function->vreg_count;

    if (debug_function_arg_mapping) printf("Adding arg move from struct to integer register_index=%d register size=%d\n", register_index, pl->stru_size);

    int lvalue_in_register = param->is_lvalue && param->vreg;
    int temp_loaded = 0;
    int size = pl->stru_size;
    int offset = 0;

    for (int i = 3; i >= 0; i--) {
        int size_unit = 1 << i;
        if (size_unit > size) continue;

        if (!temp_loaded) {
            Type *type = new_type(TYPE_CHAR + i);
            type->is_unsigned = 1;
            load_struct_scalar_into_temp(function, ir, param, pl, type, result_register, offset);
            temp_loaded = 1;
        }
        else {
            // Load value
            Value *loaded_value = make_long_temp(function);
            Value *temp2 = dup_value(param);
            temp2->type = new_type(TYPE_CHAR + i);
            temp2->type->is_unsigned = 1;
            temp2->offset += pl->stru_offset + offset;
            insert_instruction_from_operation(ir, lvalue_in_register ? IR_INDIRECT : IR_MOVE, loaded_value, temp2, 0, 1);

            // Shift loaded value
            Value *shifted_value;
            if (offset) {
                shifted_value = make_long_temp(function);
                insert_instruction_from_operation(ir, IR_BSHL, shifted_value, loaded_value, new_integral_constant(TYPE_LONG, offset * 8), 1);
            }
            else
                shifted_value = loaded_value;

            // Bitwise or shifted_value and put result in result_register
            Value *orred_value = make_long_temp(function);
            insert_instruction_from_operation(ir, IR_BOR, orred_value, shifted_value, result_register, 1);
            result_register = orred_value;
        }

        size -= size_unit;
        offset += size_unit;
        if (size == 0) break;
    }

    return add_arg_move_to_register(function, ir, new_type(TYPE_LONG), result_register, preg_class, register_index, register_set);
}

// Load an 8-byte of a struct/union that exclusively have floats and doubles in it into a register.
// The struct/union already has an alignment of either 4 or 8, so it can be loaded with simple instructions.
static int make_sse_struct_or_union_arg_move_instructions(
    Function *function, Tac *ir, Value *param, int preg_class, int register_index,
    FunctionParamLocation *pl, RegisterSet *register_set) {

    if (debug_function_arg_mapping) printf("Adding arg move from struct to SSE register_index=%d register size=%d\n", register_index, pl->stru_size);

    if (pl->stru_size == 4) {
        // Move a single float
        Value *temp = load_struct_scalar(function, ir, param, pl, new_type(TYPE_FLOAT));
        return add_arg_move_to_register(function, ir, new_type(TYPE_FLOAT), temp, preg_class, register_index, register_set);
    }
    else if (pl->stru_size == 8 && pl->stru_member_count == 1) {
        // Move a single double
        Value *temp = load_struct_scalar(function, ir, param, pl, new_type(TYPE_DOUBLE));
        return add_arg_move_to_register(function, ir, new_type(TYPE_DOUBLE), temp, preg_class, register_index, register_set);
    }
    else {
        // Move two floats. It must first be loaded into an integer register and then
        // copied to an SSE register.
        Value *temp_int = load_struct_scalar(function, ir, param, pl, new_type(TYPE_LONG));
        Value *temp_sse = new_value();
        temp_sse->type = new_type(TYPE_DOUBLE);
        temp_sse->vreg = ++function->vreg_count;
        insert_instruction_from_operation(ir, IR_MOVE_PREG_CLASS, temp_sse, temp_int, 0, 1);
        return add_arg_move_to_register(function, ir, new_type(TYPE_DOUBLE), temp_sse, preg_class, register_index, register_set);
    }
}

// Lookup corresponding location for preg_class/register
static FunctionParamLocation *lookup_location(int preg_class, int register_index, FunctionParamLocations *pl) {
    // For the location that matches register_index
    for (int loc = 0; loc < pl->count; loc++) {
        FunctionParamLocation *location = &(pl->locations[loc]);
        int function_call_register_arg_index = preg_class == PC_INT
            ? location->int_register
            : location->sse_register;

        if (function_call_register_arg_index == register_index) return location;
    }

    panic("Unhandled struct/union arg move into a register");
}

// Load a function parameter register from an struct or union 8-byte
static int make_struct_or_union_arg_move_instructions(
        Function *function, Tac *ir, Value *param, int preg_class, int register_index,
        FunctionParamLocation *location, RegisterSet *register_set) {

    if (preg_class == PC_INT)
        return make_int_struct_or_union_arg_move_instructions(function, ir, param, preg_class, register_index, location, register_set);
    else
        return make_sse_struct_or_union_arg_move_instructions(function, ir, param, preg_class, register_index, location, register_set);
}

// Insert IR_MOVE instructions before IR_ARG instructions for
// - the first 6 single-register args.
// - the first 8 floating point args.
// The dst of the move will be constrained so that rdi, rsi, xmm0, xmm1 etc are allocated to it.
void add_function_call_arg_moves_for_preg_class(Function *function, int preg_class) {
    int function_call_count = make_function_call_count(function);

    int register_count = preg_class == PC_INT ? 6 : 8;

    // Values of the passed argument, i.e. by the caller
    Value **arg_values = malloc(sizeof(Value *) * function_call_count * register_count);
    memset(arg_values, 0, sizeof(Value *) * function_call_count * register_count);

    // param_indexes maps the register indexes to a parameter index, e.g.
    // foo(int i, long double ld, int j) will produce
    // param_indexes[0] = 0
    // param_indexes[1] = 2
    int *param_indexes = malloc(sizeof(int) * function_call_count * register_count);
    memset(param_indexes, -1, sizeof(int) * function_call_count * register_count);

    FunctionParamLocations **param_locations = malloc(sizeof(FunctionParamLocations *) * function_call_count * register_count);
    memset(param_locations, -1, sizeof(FunctionParamLocations *) * function_call_count * register_count);

    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_ARG) {
            FunctionParamLocations *pl = ir->src1->function_call_arg_locations;

            for (int loc = 0; loc < pl->count; loc++) {
                int function_call_register_arg_index = preg_class == PC_INT
                    ? pl->locations[loc].int_register
                    : pl->locations[loc].sse_register;

                if (function_call_register_arg_index >= 0) {
                    int i = ir->src1->int_value * register_count + function_call_register_arg_index;
                    arg_values[i] = ir->src2;
                    param_indexes[i] = ir->src1->function_call_arg_index;
                    param_locations[i] = pl;
                }
            }
        }

        if (ir->operation == IR_CALL) {
            Value **call_arg = &(arg_values[ir->src1->int_value * register_count]);
            int *param_index = &(param_indexes[ir->src1->int_value * register_count]);
            FunctionParamLocations **pls = &(param_locations[ir->src1->int_value * register_count]);
            Function *called_function = ir->src1->function_symbol->type->function;

            // Allocated registers that hold the argument value
            Value **function_call_values = malloc(sizeof(Value *) * register_count);
            memset(function_call_values, 0, sizeof(Value *) * register_count);

            // Add the moves backwards so that arg 0 (rsi) is last.
            int i = 0;
            while (i < register_count && *call_arg) {
                call_arg++;
                param_index++;
                pls++;
                i++;
            }

            call_arg--;
            param_index--;
            pls--;
            i--;

            for (; i >= 0; i--) {
                Type *type;
                int pi = *param_index;
                if (pi >= 0 && pi < called_function->param_count) {
                    type = called_function->param_types[pi];
                }
                else
                    type = (*call_arg)->type;

                int function_call_vreg;
                Type *function_call_vreg_type;
                if (type->type == TYPE_STRUCT_OR_UNION) {
                    FunctionParamLocation *location = lookup_location(preg_class, i, *pls);
                    function_call_vreg = make_struct_or_union_arg_move_instructions(function, ir, *call_arg, preg_class, i, location, &arg_register_set);
                    function_call_vreg_type = preg_class == PC_INT ? new_type(TYPE_LONG) : new_type(TYPE_DOUBLE);
                }
                else {
                    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
                    function_call_vreg = add_arg_move_to_register(function, ir, type, *call_arg, preg_class, i, &arg_register_set);
                    function_call_vreg_type = (*call_arg)->type;
                }

                function_call_values[i] = new_value();
                function_call_values[i]->type = function_call_vreg_type;
                function_call_values[i]->vreg = function_call_vreg;

                call_arg--;
                param_index--;
                pls--;
            }

            add_ir_call_reg_instructions(ir, function_call_values, register_count);
        }
    }
}

// Add instructions to copy a struct to the stack
static void add_function_call_arg_move_for_struct_or_union_on_stack(Function *function, Tac *ir) {
    int size = get_type_size(ir->src2->type);
    int rounded_up_size = (size + 7) & ~7;

    // Allocate stack space with a sub $n, %rsp instruction
    insert_instruction_from_operation(ir, IR_ALLOCATE_STACK, 0, new_integral_constant(TYPE_LONG, rounded_up_size), 0, 1);

    // Add an instruction to move the stack pointer %rsp to a temporary register
    Value *stack_pointer_temp = make_long_temp(function);
    insert_instruction_from_operation(ir, IR_MOVE_STACK_PTR, stack_pointer_temp, 0, 0, 1);

    // Prepare destination, which must be a * void
    Value *dst = dup_value(stack_pointer_temp);
    dst->type = make_pointer_to_void();
    dst->is_lvalue = 1;

    // Convert src to be a pointer to void
    Value *src = dup_value(ir->src2);
    src->type = make_pointer_to_void();

    if (debug_function_arg_mapping)
        printf("Adding memory copy for struct/union SI=%d rounded-up-size=%d\n", src->stack_index, rounded_up_size);

    add_memory_copy(function, ir, dst, src, size);
}

// Nuke all IR_ARG instructions that have had code added that moves the value into a register
static void remove_IR_ARG_instructions_that_have_been_handled(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_ARG) {
            FunctionParamLocations *pl = ir->src1->function_call_arg_locations;

            for (int loc = 0; loc < pl->count; loc++) {
                if (pl->locations[loc].int_register != -1 || pl->locations[loc].sse_register != -1) {
                    ir->operation = IR_NOP;
                    ir->dst = 0;
                    ir->src1 = 0;
                    ir->src2 = 0;
                    break;
                }
            }
        }
    }
}

// Process IR_ARG insructions. They are either loaded into a register, push onto the
// stack with an IR_ARG, or, in the case of a struct/union pushed onto the stack with
// generated code.
void add_function_call_arg_moves(Function *function) {
    add_function_call_arg_moves_for_preg_class(function, PC_INT);
    add_function_call_arg_moves_for_preg_class(function, PC_SSE);

    remove_IR_ARG_instructions_that_have_been_handled(function);

    // Add memory copies for struct and unions
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_ARG) {
            if (ir->src2->type->type == TYPE_STRUCT_OR_UNION) {
                FunctionParamLocations *pls = ir->src1->function_call_arg_locations;
                if (pls->count != 1) panic("Unexpected struct/union to stack move with locations->count != 1");
                add_function_call_arg_move_for_struct_or_union_on_stack(function, ir);

                ir->operation = IR_NOP;
                ir->dst = 0;
                ir->src1 = 0;
                ir->src2 = 0;
            }
        }
    }

    // Process any added memcpy calls due to struct and union copies
    add_function_call_arg_moves_for_preg_class(function, PC_INT);
    remove_IR_ARG_instructions_that_have_been_handled(function);

    if (debug_function_arg_mapping) {
        printf("After function call arg mapping\n");
        print_ir(function, 0, 0);
    }
}

// Set has_address_of to 1 if a value is a parameter in a register and it's used in a & instruction
static void check_param_value_has_used_in_an_address_of(int *has_address_of, Tac *tac, Value *v) {
    if (!v) return;
    if (tac->operation != IR_ADDRESS_OF) return;
    if (v->stack_index < 2) return;
    has_address_of[v->stack_index - 2] = 1;
    return;
}

static void assign_register_to_value(Value *v, int vreg) {
    v->stack_index = 0;
    v->is_lvalue = 0;
    v->vreg = vreg;
}

// Convert stack_index in value v to a parameter register
static void convert_register_param_stack_index_to_register(Function *function, int *register_param_vregs, Value *v) {
    if (v && v->stack_index >= 2 && register_param_vregs[v->stack_index  - 2] != -1)
        assign_register_to_value(v, register_param_vregs[v->stack_index  - 2]);
}

// Convert stack_index in value v to a parameter in the stack
static void convert_register_param_stack_index_to_stack(Function *function, int *register_param_stack_indexes, Value *v) {
    if (v && v->stack_index >= 2 && register_param_stack_indexes[v->stack_index  - 2]) {
        v->stack_index = register_param_stack_indexes[v->stack_index  - 2];
        v->is_lvalue = 0;
    }
}

// Convert a value that has a stack index >= 2, i.e. it's a pushed parameter into a vreg
static void convert_pushed_param_stack_index_to_register(Function *function, int *stack_param_vregs, Value *v) {
    if (v && !v->function_param_original_stack_index && v->stack_index >= 2 && stack_param_vregs[v->stack_index - 2] != -1)
        assign_register_to_value(v, stack_param_vregs[v->stack_index - 2]);
}

// Add an instruction to move a parameter in a register to another register
static Tac *make_param_move_to_register_tac(Function *function, Type *type, int function_param_index) {
    Tac *tac = new_instruction(IR_MOVE);
    tac->dst = new_value();
    tac->dst->type = dup_type(type);
    tac->dst->vreg = ++function->vreg_count;

    tac->src1 = new_value();
    tac->src1->type = dup_type(tac->dst->type);
    tac->src1->live_range_preg = is_sse_floating_point_type(type) ? sse_arg_registers[function_param_index] : int_arg_registers[function_param_index];

    return tac;
}

// Add an instruction to move a parameter in a register to a new stack entry
static Tac *make_param_move_to_stack_tac(Function *function, Type *type, int function_param_index) {
    Tac *tac = new_instruction(IR_MOVE);
    tac->dst = new_value();
    tac->dst->type = dup_type(type);
    tac->dst->stack_index = -(++function->stack_register_count);

    tac->src1 = new_value();
    tac->src1->type = dup_type(tac->dst->type);
    tac->src1->live_range_preg = is_sse_floating_point_type(type) ? sse_arg_registers[function_param_index] : int_arg_registers[function_param_index];

    return tac;
}

// Add instructions to move struct/union data from a param register to a struct on the stack
static int add_struct_or_union_param_move(Function *function, Tac *ir, Type *type, FunctionParamLocations *pl, RegisterSet *register_set) {
    // Allocate space on the stack for the struct
    Value *v = new_value();
    v->type = dup_type(type);
    v->is_lvalue = 1;
    v->stack_index = -(++function->stack_register_count);
    insert_instruction_from_operation(ir, IR_DECL_LOCAL_COMP_OBJ, 0, v, 0, 1);

    for (int loc = 0; loc < pl->count; loc++) {
        FunctionParamLocation *location = &(pl->locations[loc]);

        if (location->int_register != -1)
            make_int_struct_or_union_move_from_register_to_stack_instructions(function, ir, type, location, location->int_register, v->stack_index, register_set, 0);
        else if (location->sse_register != -1)
            make_sse_struct_or_union_move_from_register_to_stack_instructions(function, ir, type, location, location->sse_register, v->stack_index, register_set, 0);
        else
            panic("Got unexpected stack offset in add_struct_or_union_param_move");
    }

    return v->stack_index;
}

// Convert a stack_index if stack_index_map isn't -1
void remap_stack_index(int *stack_index_remap, Value *v) {
    if (v && v->stack_index >= 2 && !v->has_been_renamed && stack_index_remap[v->stack_index] != -1) {
        v->stack_index = stack_index_remap[v ->stack_index];
        v->has_been_renamed = 1;
    }
}

// if the function returns a struct/union in memory, then the caller puts a pointer to
// the target in rdi. Make a copy of rdi in return_value_pointer for use in the
// return value code. The function returns 1 if rdi has been used in this way.
static int setup_return_for_struct_or_union(Function *function) {
    FunctionParamAllocation *fpa = function->return_value_fpa;

    if (fpa->params[0].locations[0].stack_offset == -1) return 0;

    function->return_value_pointer = new_value();
    function->return_value_pointer->vreg = ++function->vreg_count;
    function->return_value_pointer->type = make_pointer_to_void();

    // Make value for rdi register
    Value *src1 = new_value();
    src1->type = make_pointer_to_void();
    src1->live_range_preg = LIVE_RANGE_PREG_RDI_INDEX;
    src1->type = make_pointer_to_void();
    src1->vreg = ++function->vreg_count;

    Value *dst = dup_value(function->return_value_pointer);

    insert_instruction_from_operation(ir, IR_MOVE, dst, src1, 0, 1);

    return 1;
}

// Add instructions that deal with the function arguments. Several cases are possible
// - Scalar in register -> register
// - Scalar in register -> stack, if an address of is used
// - Scalar on stack -> register
// - Scalar on stack -> keep on stack
// - Composite in register(s) -> stack
// - Composite in stack -> stack
//
// For register - register moves, intermediate registers are allocated.  Either, the
// moves will go to a new physical register, or, when possible, will remain in the
// original registers. They might get spilled, in which case
// function_param_original_stack_index is used rather than allocating more space.
//
// Return values for structs & unions with size > 16 bytes are passed in memory,
// with rdi containing a pointer to the memory.
void add_function_param_moves(Function *function, char *identifier) {
    if (debug_function_param_mapping) printf("Mapping function parameters for %s\n", identifier);

    ir = function->ir;

    // Add a second nop if nothing is there, which is used to insert instructions
    if (!ir->next) {
        ir->next = new_instruction(IR_NOP);
        ir->next->prev = ir;
    }

    ir = function->ir->next;

    FunctionParamAllocation *fpa = init_function_param_allocaton(identifier);

    int fpa_start = 0; // Which index in fpa->params has the first actualy parameter

    if (function->return_type && function->return_type->type == TYPE_STRUCT_OR_UNION) {
        fpa_start = setup_return_for_struct_or_union(function);
        if (fpa_start) add_function_param_to_allocation(fpa, function->return_value_pointer->type);
    }

    for (int i = 0; i < function->param_count; i++)
        add_function_param_to_allocation(fpa, function->param_types[i]);

    int *register_param_vregs = malloc(sizeof(int) * function->param_count);
    memset(register_param_vregs, -1, sizeof(int) * function->param_count);

    int *register_param_stack_indexes = malloc(sizeof(int) * function->param_count);
    memset(register_param_stack_indexes, 0, sizeof(int) * function->param_count);

    int *has_address_of = malloc(sizeof(int) * (function->param_count));
    memset(has_address_of, 0, sizeof(int) * (function->param_count));

    // The stack is never bigger than function->param_count * 2 & starts at 2
    int *stack_param_vregs = malloc(sizeof(int) * (function->param_count * 2 + 2));
    memset(stack_param_vregs, -1, sizeof(int) * (function->param_count * 2 + 2));

    // Determine which parameters in registers are used in IR_ADDRESS_OF instructions
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->dst);
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->src1);
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->src2);
    }

    // Add moves for registers
    for (int i = 0; i < function->param_count; i++) {
        if (fpa->params[fpa_start + i].locations[0].stack_offset != -1) continue;

        Type *type = function->param_types[i];

        int single_register_arg_count = fpa->params[fpa_start + i].locations[0].int_register == -1
            ? fpa->params[fpa_start + i].locations[0].sse_register
            : fpa->params[fpa_start + i].locations[0].int_register;

        if (type->type == TYPE_STRUCT_OR_UNION)  {
            int stack_index = add_struct_or_union_param_move(function, ir, type, &(fpa->params[fpa_start + i]), &arg_register_set);
            register_param_stack_indexes[i] = stack_index;
        }

        else {
            // Scalar value

            if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);

            if (has_address_of[i]) {
                // Add a move instruction to save the register to the stack
                Tac *tac = make_param_move_to_stack_tac(function, type, single_register_arg_count);
                register_param_stack_indexes[i] = tac->dst->stack_index;
                tac->src1->vreg = ++function->vreg_count;
                insert_instruction(ir, tac, 1);
                if (debug_function_param_mapping) printf("Param %d reg param reg %d -> local SI %d\n", i, tac->src1->vreg, tac->dst->stack_index);
            }
            else {
                // Add a move instruction to copy register to another register
                Tac *tac = make_param_move_to_register_tac(function, type, single_register_arg_count);
                register_param_vregs[i] = tac->dst->vreg;
                tac->src1->vreg = ++function->vreg_count;
                insert_instruction(ir, tac, 1);
                if (debug_function_param_mapping) printf("Param %d reg param reg %d -> local reg %d\n", i, tac->src1->vreg, tac->dst->vreg);
            }
        }
    }

    // Adapt the function's IR to use the values in registers/stack
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        convert_register_param_stack_index_to_register(function, register_param_vregs, ir->dst);
        convert_register_param_stack_index_to_register(function, register_param_vregs, ir->src1);
        convert_register_param_stack_index_to_register(function, register_param_vregs, ir->src2);

        convert_register_param_stack_index_to_stack(function, register_param_stack_indexes, ir->dst);
        convert_register_param_stack_index_to_stack(function, register_param_stack_indexes, ir->src1);
        convert_register_param_stack_index_to_stack(function, register_param_stack_indexes, ir->src2);
    }

    // Process parameters in the stack
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->dst ) ir->dst ->has_been_renamed = 0;
        if (ir->src1) ir->src1->has_been_renamed = 0;
        if (ir->src2) ir->src2->has_been_renamed = 0;
    }

    // Parameter stack indexes go from 2, 3, 4 for arg 0, arg 1, arg 2, ...
    // Determine the actual stack index based on type sizes and alignment and
    // remap stack_index.
    int *stack_index_remap = malloc(sizeof(int) * (function->param_count + 2));
    memset(stack_index_remap, -1, sizeof(int) * (function->param_count + 2));

    // Determine stack offsets for parameters on the stack and add moves

    for (int i = 0; i < function->param_count; i++) {
        if (fpa->params[fpa_start + i].locations[0].stack_offset == -1) continue;

        Type *type = function->param_types[i];

        int stack_index = (fpa->params[fpa_start + i].locations[0].stack_offset + 16) >> 3;

        // The rightmost arg has stack index 2
        if (i + 2 != stack_index) stack_index_remap[i + 2] = stack_index;

        if (debug_function_param_mapping) printf("Param %d SI %d -> SI %d\n", i, i + 2, stack_index);

        if (!has_address_of[i] && type->type != TYPE_LONG_DOUBLE && type->type != TYPE_STRUCT_OR_UNION) {
            Tac *tac = make_param_move_to_register_tac(function, type, i);
            stack_param_vregs[stack_index - 2] = tac->dst->vreg;
            tac->src1->function_param_original_stack_index = stack_index;
            tac->src1->stack_index = stack_index;
            tac->src1->has_been_renamed = 1;
            insert_instruction(ir, tac, 1);
        }
    }

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        remap_stack_index(stack_index_remap, ir->dst);
        remap_stack_index(stack_index_remap, ir->src1);
        remap_stack_index(stack_index_remap, ir->src2);

        convert_pushed_param_stack_index_to_register(function, stack_param_vregs, ir->dst);
        convert_pushed_param_stack_index_to_register(function, stack_param_vregs, ir->src1);
        convert_pushed_param_stack_index_to_register(function, stack_param_vregs, ir->src2);
    }

    free(register_param_vregs);
    free(stack_param_vregs);
    free(stack_index_remap);

    if (debug_function_param_mapping) {
        printf("After function param mapping\n");
        print_ir(function, 0, 0);
    }
}

Value *make_function_call_value(int function_call) {
    Value *src1 = new_value();

    src1->int_value = function_call;
    src1->is_constant = 1;
    src1->type = new_type(TYPE_LONG);

    return src1;
}

// Initialize data structures for the function param & arg allocation processor
FunctionParamAllocation *init_function_param_allocaton(char *function_identifier) {
    if (debug_function_param_allocation) printf("\nInitializing param allocation for function %s\n", function_identifier);

    FunctionParamAllocation *fpa = malloc(sizeof(FunctionParamAllocation));
    memset(fpa, 0, sizeof(FunctionParamAllocation));
    fpa->params = malloc(sizeof(FunctionParamLocations) * MAX_FUNCTION_CALL_ARGS);

    return fpa;
}

// Using the state of already allocated registers & stack entries in fpa, determine the location for a type and set it in fpl.
static void add_type_to_allocation(FunctionParamAllocation *fpa, FunctionParamLocation *fpl, Type *type, int force_stack) {
    fpl->int_register = -1;
    fpl->sse_register = -1;
    fpl->stack_offset = -1;
    fpl->stack_padding = -1;

    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);

    int is_single_int_register = type_fits_in_single_int_register(type);
    int is_single_sse_register = is_sse_floating_point_type(type);
    int is_long_double = type->type == TYPE_LONG_DOUBLE;
    int in_stack =
        is_long_double ||
        force_stack ||
        (is_single_int_register && fpa->single_int_register_arg_count >= 6) || (is_single_sse_register && fpa->single_sse_register_arg_count >= 8);

    int alignment = get_type_alignment(type);
    if (alignment < 8) alignment = 8;

    if (!in_stack && is_single_int_register)
        fpl->int_register = fpa->single_int_register_arg_count < 6 ? fpa->single_int_register_arg_count : -1;

    else if (!in_stack && is_single_sse_register)
        fpl->sse_register = fpa->single_sse_register_arg_count < 8 ? fpa->single_sse_register_arg_count : -1;

    else {
        // It's on the stack

        if (alignment > fpa->biggest_alignment) fpa->biggest_alignment = alignment;
        int padding = ((fpa->offset + alignment  - 1) & (~(alignment - 1))) - fpa->offset;
        fpa->offset += padding;

        fpl->stack_offset = fpa->offset;
        fpl->stack_padding = padding;

        int type_size = get_type_size(type);
        if (type_size < 8) type_size = 8;
        fpa->offset += type_size;

        if (debug_function_param_allocation)
            printf("  arg %2d with alignment %2d     offset 0x%04x with padding 0x%04x\n", fpa->arg_count, alignment, fpl->stack_offset, fpl->stack_padding);
    }

    if (debug_function_param_allocation && !in_stack && (is_single_int_register || is_single_sse_register)) {
        if (fpl->int_register != -1)
            printf("  arg %2d with alignment %2d     int reg %5d\n", fpa->arg_count, alignment, fpl->int_register);
        else
            printf("  arg %2d with alignment %2d     sse reg %5d\n", fpa->arg_count, alignment, fpl->sse_register);
    }

    fpa->single_int_register_arg_count += is_single_int_register;
    if (!in_stack && is_single_sse_register) fpa->single_sse_register_arg_count++;
}

// Recurse through struct/union and make list of all members + their offsets
static void flatten_struct_or_union(StructOrUnion *s, StructOrUnionScalars *scalars, int offset) {
    for (StructOrUnionMember **pmember = s->members; *pmember; pmember++) {
        StructOrUnionMember *member = *pmember;

        if (member->type->type == TYPE_STRUCT_OR_UNION)
            flatten_struct_or_union(member->type->struct_or_union_desc, scalars, offset + member->offset);
        else {
            StructOrUnionScalar *scalar = malloc(sizeof(StructOrUnionScalar));
            scalars->scalars[scalars->count++] = scalar;
            scalar->type = member->type;
            scalar->offset = offset + member->offset;
        }
    }
}

static void add_single_stack_function_param_location(FunctionParamAllocation *fpa, Type *type) {
    FunctionParamLocations *fpl = &(fpa->params[fpa->arg_count]);
    fpl->locations = malloc(sizeof(FunctionParamLocation));
    fpl->count = 1;
    add_type_to_allocation(fpa, &(fpl->locations[0]), type, 0);

}
// Add a param/arg to a function and allocate registers & stack entries
// Structs are decomposed.
void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type) {
    if (fpa->arg_count > MAX_FUNCTION_CALL_ARGS) panic1d("Maximum function call arg count of %d exceeded", MAX_FUNCTION_CALL_ARGS);

    if (type->type != TYPE_STRUCT_OR_UNION) {
        // Create a single location for the arg
        add_single_stack_function_param_location(fpa, type);
    }

    else {
        // TODO also force struct params with unaligned members into memory
        // In practice, if a struct is larger than 16 bytes, it's on the stack

        if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);

        int size = get_type_size(type);
        if (size > 16) {
            // The entire thing is on the stack
            add_single_stack_function_param_location(fpa, type);
        }

        else {
            // Decompose a struct or union into up to 8 8-bytes

            StructOrUnionScalars *scalars = malloc(sizeof(StructOrUnionScalars));
            scalars->scalars = malloc(sizeof(StructOrUnionScalar) * 16);
            scalars->count = 0;
            flatten_struct_or_union(type->struct_or_union_desc, scalars, 0);

            // Classify the eight bytes
            char *seen_integer = malloc(sizeof(char) * 8);
            memset(seen_integer, 0, sizeof(char) * 8);
            char *seen_sse = malloc(sizeof(char) * 8);
            memset(seen_sse, 0, sizeof(char) * 8);
            char *seen_memory = malloc(sizeof(char) * 8);
            memset(seen_memory, 0, sizeof(char) * 8);
            char *member_counts = malloc(sizeof(char) * 8);
            memset(member_counts, 0, sizeof(char) * 8);

            size = 0; // Needs recalculating to remove any paddding at the end
            for (int i = 0; i < scalars->count; i++) {
                StructOrUnionScalar *scalar = scalars->scalars[i];
                size +=  get_type_size(scalar->type);
                int eight_byte = scalar->offset >> 3;
                member_counts[eight_byte]++;

                if (type_fits_in_single_int_register(scalar->type))
                    seen_integer[eight_byte] = 1;
                else if (is_sse_floating_point_type(scalar->type))
                    seen_sse[eight_byte] = 1;
                else {
                    seen_memory[eight_byte] = 1;
                    seen_memory[eight_byte + 1] = 1;
                }
            }

            // Determine number of 8-bytes & allocate memory
            int eight_bytes_count = (size + 7) / 8;
            FunctionParamLocations *fpl = &(fpa->params[fpa->arg_count]);
            fpl->locations = malloc(sizeof(FunctionParamLocation) * eight_bytes_count);
            fpl->count = eight_bytes_count;

            // If one of the classes is MEMORY, the whole argument is passed in memory.
            int in_memory = 0;
            for (int i = 0; i < eight_bytes_count; i++) in_memory |= seen_memory[i];
            if  (in_memory) {
                // The entire thing is on the stack
                add_single_stack_function_param_location(fpa, type);
            }

            else {
                // Create a location for each eight byte
                for (int i = 0; i < eight_bytes_count; i++) {
                    fpl->locations[i].stru_size = size > 8 ? 8 : size;
                    size -= 8;

                    fpl->locations[i].stru_member_count = member_counts[i];
                    fpl->locations[i].stru_offset = i * 8;

                    if (seen_integer[i])
                        add_type_to_allocation(fpa, &(fpl->locations[i]), new_type(TYPE_INT), 0);
                    else
                        add_type_to_allocation(fpa, &(fpl->locations[i]), new_type(TYPE_FLOAT), 0);
                }
            }
        }
    }

    fpa->arg_count++;
}

// Calculate the final size and padding of the stack
void finalize_function_param_allocation(FunctionParamAllocation *fpa) {
    fpa->padding = ((fpa->offset + fpa->biggest_alignment  - 1) & (~(fpa->biggest_alignment - 1))) - fpa->offset;
    fpa->size = fpa->offset + fpa->padding;

    if (debug_function_param_allocation) {
        printf("  --------------------------------------------------------------\n");
        printf("  total                        size   0x%04x with padding 0x%04x\n", fpa->size, fpa->padding);
    }
}
