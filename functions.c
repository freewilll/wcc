#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

static LongSet *allocated_functions;

void init_function_allocations(void) {
    allocated_functions = new_longset();
}

void free_function(Function *function, int remove_from_allocations) {
    free_strmap(function->labels);
    free(function);

    if (remove_from_allocations) longset_delete(allocated_functions, (long) function);
}

// Free remaining functions
void free_functions(void) {
    longset_foreach(allocated_functions, it) free_function((Function *) longset_iterator_element(&it), 0);
    free_longset(allocated_functions);
}

Function *new_function(void) {
    Function *function = wcalloc(1, sizeof(Function));
    longset_add(allocated_functions, (long) function);

    return function;
}

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

    Type *function_type = ir->src1->type;

    FunctionParamAllocation *fpa = function_type->function->return_value_fpa;
    FunctionParamLocations *fpl = &(fpa->params[0]);

    Value *function_value = ir->src1;

    if (fpl->locations[0].stack_offset == -1) {
        // Move registers to a struct/union on the stack

        ir->dst = 0;
        ir = ir->next;

        // Allocate result vregs and create a live range or them
        int *live_range_pregs = wmalloc(sizeof(int) * 8);
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

    Value **function_call_values = wcalloc(2, sizeof(Value *));

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
            int is_sse = is_sse_floating_point_type(function->type->target);
            int live_range_preg = is_sse ? LIVE_RANGE_PREG_XMM00_INDEX : LIVE_RANGE_PREG_RAX_INDEX;

            ir->src1->preferred_live_range_preg_index = live_range_preg;

            ir->dst = new_value();
            ir->dst->type = dup_type(function->type->target);
            if (ir->dst->type->type == TYPE_ENUM) ir->dst->type = new_type(TYPE_INT);
            ir->dst->vreg = ++function->vreg_count;
            ir->dst->live_range_preg = live_range_preg;
            ir->src1->preferred_live_range_preg_index = live_range_preg;

            insert_instruction_from_operation(ir, IR_MOVE, ir->dst, ir->src1, 0, 1);

            ir->operation = IR_RETURN;
            ir->dst = 0;
            ir->src1 = 0;
            ir->src2 = 0;
        }

        ir->src1 = 0;
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
    Value **arg_values = wcalloc(function_call_count * register_count, sizeof(Value *));

    // param_indexes maps the register indexes to a parameter index, e.g.
    // foo(int i, long double ld, int j) will produce
    // param_indexes[0] = 0
    // param_indexes[1] = 2
    int *param_indexes = wmalloc(sizeof(int) * function_call_count * register_count);
    memset(param_indexes, -1, sizeof(int) * function_call_count * register_count);

    // Set to 1 if the function returns a struct in memory, which reserves rdi for a
    // pointer to the return address
    char *has_struct_or_union_return_values = wcalloc(function_call_count, sizeof(char));

    FunctionParamLocations **param_locations = wmalloc(sizeof(FunctionParamLocations *) * function_call_count * register_count);
    memset(param_locations, -1, sizeof(FunctionParamLocations *) * function_call_count * register_count);

    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_ARG) {
            FunctionParamLocations *pl = ir->src1->function_call_arg_locations;
            if (ir->src1->has_struct_or_union_return_value) has_struct_or_union_return_values[ir->src1->int_value] = 1;

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
            int has_struct_or_union_return_value = has_struct_or_union_return_values[ir->src1->int_value];
            int *param_index = &(param_indexes[ir->src1->int_value * register_count]);
            FunctionParamLocations **pls = &(param_locations[ir->src1->int_value * register_count]);
            Type *called_function_type = ir->src1->type;

            // Allocated registers that hold the argument value
            Value **function_call_values = wcalloc(register_count, sizeof(Value *));

            // Add the moves backwards so that arg 0 (rsi) is last.
            int i = 0;

            // Advance past the first parameter, which holds the pointer to the struct/union return value
            if (has_struct_or_union_return_value && preg_class == PC_INT) {
                call_arg++;
                param_index++;
                pls++;
                i++;
            }

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
                // Bail if we're doing integers and RDI is reserved for a struct/union
                // return value.
                if (has_struct_or_union_return_value && preg_class == PC_INT && i == 0) break;

                Type *type;
                int pi = *param_index;
                if (pi >= 0 && pi < called_function_type->function->param_count) {
                    type = called_function_type->function->param_types->elements[pi];
                }
                else
                    type = apply_default_function_call_argument_promotions((*call_arg)->type);

                int function_call_vreg;
                Type *function_call_vreg_type;
                if (type->type == TYPE_STRUCT_OR_UNION) {
                    FunctionParamLocation *location = lookup_location(preg_class, i, *pls);
                    function_call_vreg = make_struct_or_union_arg_move_instructions(function, ir, *call_arg, preg_class, i, location, &arg_register_set);
                    function_call_vreg_type = preg_class == PC_INT ? new_type(TYPE_LONG) : new_type(TYPE_DOUBLE);
                }
                else {
                    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
                    if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);
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

    int max = is_sse_floating_point_type(type) ? 8 : 6;
    if (function_param_index < max)
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

    int max = is_sse_floating_point_type(type) ? 8 : 6;
    if (function_param_index < max)
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
    FunctionParamAllocation *fpa = function->type->function->return_value_fpa;

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

// For functions with variadic arguments, move registers into the register save area.
// The register save area has been allocated on the stack by the parser with the
// value set in function->register_save_area.
static void add_function_vararg_param_moves(Function *function, FunctionParamAllocation *fpa) {
    // Add moves for ints registers to register save area
    for (int i = fpa->single_int_register_arg_count; i < 6; i++) {
        Value *src = new_value();
        src->vreg = ++function->vreg_count;
        src->type = new_type(TYPE_LONG);
        src->live_range_preg = int_arg_registers[i];
        Value *dst = dup_value(function->register_save_area);
        dst->type = new_type(TYPE_LONG);
        dst->offset = i * 8;
        ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, src, 0);
    }

    // Skip SSE register copying if al is zero with a jump to "done"
    Value *ldone = new_label_dst();
    Value *rax = new_value();
    rax->vreg = ++function->vreg_count;
    rax->type = new_type(TYPE_CHAR);
    rax->live_range_preg = LIVE_RANGE_PREG_RAX_INDEX;
    ir = insert_instruction_after_from_operation(ir, IR_JZ, 0, rax, ldone);

    // add moves for SSE registers to register save area
    for (int i = fpa->single_sse_register_arg_count; i < 8; i++) {
        Value *src = new_value();
        src->vreg = ++function->vreg_count;
        src->type = new_type(TYPE_DOUBLE);
        src->live_range_preg = sse_arg_registers[i];

        Value *temp_int = new_value();
        temp_int->type = new_type(TYPE_LONG);
        temp_int->vreg = ++function->vreg_count;
        ir = insert_instruction_after_from_operation(ir, IR_MOVE_PREG_CLASS, temp_int, src, 0);

        Value *dst = dup_value(function->register_save_area);
        dst->type = new_type(TYPE_LONG);
        dst->offset = 48 + i * 16;
        ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, temp_int, 0);
    }

    // Add done label
    ir = insert_instruction_after_from_operation(ir, IR_NOP, 0, 0, 0);
    ir->label = ldone->label;
}

// Add code for a va_start() call. The va_list struct, present in src1, has to be
// populated.
static void process_function_va_start(Function *function, Tac *ir) {
    Value *va_list = ir->src1;
    ir->operation = IR_NOP;
    ir->src1 = 0;

    // Set va_list.fp_offset, the offset of the first vararg integer register
    Value *fp_offset_value = new_value();
    fp_offset_value->type = new_type(TYPE_INT);
    fp_offset_value->type->is_unsigned = 1;
    fp_offset_value->is_constant = 1;
    fp_offset_value->int_value = function->fpa->single_int_register_arg_count * 8;

    Value *dst = dup_value(va_list);
    dst->type = new_type(TYPE_INT);
    dst->type->is_unsigned = 1;
    ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, fp_offset_value, 0);

    // Set va_list.gp_offset, the offset of the first vararg SSE register
    Value *gp_offset_value = dup_value(fp_offset_value);
    gp_offset_value->int_value = 48 + function->fpa->single_sse_register_arg_count * 16;
    dst = dup_value(dst);
    dst->offset = 4;
    ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, gp_offset_value, 0);

    // Set va_list.overflow_arg_area, the address of the first vararg pushed on the stack
    Value *tmp_dst = dup_value(dst);
    tmp_dst->type = make_pointer_to_void();
    tmp_dst->vreg = ++function->vreg_count;

    Value *overflow = dup_value(dst);
    overflow->type = make_pointer_to_void();
    overflow->vreg = 0;
    overflow->offset = 0;
    overflow->stack_index = OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX;
    ir = insert_instruction_after_from_operation(ir, IR_ADDRESS_OF, tmp_dst, overflow, 0);

    dst = dup_value(dst);
    dst->type = make_pointer_to_void();
    dst->offset = 8;
    ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, tmp_dst, 0);

    // Set va_list.reg_save_area, the address of the register save area
    tmp_dst = dup_value(tmp_dst);
    tmp_dst->vreg = ++function->vreg_count;
    ir = insert_instruction_after_from_operation(ir, IR_ADDRESS_OF, tmp_dst, function->register_save_area, 0);

    dst = dup_value(dst);
    dst->type = make_pointer_to_void();
    dst->offset = 16;
    ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, tmp_dst, 0);
}

// Read a value from va_list. This could be either directly from the stack or from
// an lvalue in a register.
static Tac *read_from_va_list(Function *function, Tac *ir, Value *va_list, Type *src_type, Type *dst_type, int offset, Value **result) {
    *result = new_value();
    (*result)->type = dst_type;
    (*result)->vreg = ++function->vreg_count;

    Value *src1 = dup_value(va_list);
    src1->type = src_type;

    if (src1->vreg) {
        // va_list is a pointer in a register. Load it, add the offset to it, then
        // indirect.
        Value *tmp = dup_value(va_list);
        tmp->type = make_pointer(dst_type);
        tmp->vreg = ++function->vreg_count;
        ir = insert_instruction_after_from_operation(ir, IR_MOVE, tmp, va_list, 0);

        src1->type = make_pointer(dst_type);
        src1->offset = offset;
        ir = insert_instruction_after_from_operation(ir, IR_INDIRECT, *result, src1, 0);
    }
    else {
        // Read from the stack
        src1->offset = offset;
        ir = insert_instruction_after_from_operation(ir, IR_MOVE, *result, src1, 0);
    }

    return ir;
}

// Write a value to va_list. This could be either directly from the stack or from
// an lvalue in a register.
static Tac *write_to_va_list(Function *function, Tac *ir, Value *va_list, Value *value, int offset) {
    Value *dst = dup_value(va_list);

    if (va_list->vreg) {
        // va_list is a pointer in a register. Load it, add the offset to it, then
        // write to the pointer.

        Value *tmp = dup_value(dst);
        tmp->is_lvalue = 1;
        tmp->offset = offset;
        tmp->type = value->type;
        ir = insert_instruction_after_from_operation(ir, IR_MOVE, tmp, value, 0);
    }
    else {
        // Write to the stack
    dst->type = value->type;
        dst->offset = offset;
        ir = insert_instruction_after_from_operation(ir, IR_MOVE, dst, value, 0);
    }
}

// Add instructions that read the FP or GP offset, determine if the type can fit
// in a register, and if not, jmp to the stack read label fetch_from_stack.
static Tac *add_function_va_arg_in_register_check(Function *function, Tac *ir, Type *type, Value *va_list, int gp_count, int fp_count, Value *fetch_from_stack, Value **result_gp_fp_offset) {
    // Read gp_offset or fp_offset into gp_fp_offset register
    Type *gp_fp_offset_type = new_type(TYPE_INT);
    gp_fp_offset_type->is_unsigned = 1;
    int gp_fp_offset_offset = gp_count ? 0 : 4; // gp_offset or fp_offset
    Value *gp_fp_offset;
    ir = read_from_va_list(function, ir, va_list, gp_fp_offset_type, gp_fp_offset_type, gp_fp_offset_offset, &gp_fp_offset);

    // Check if gp_fp_offset >= 48, and if so jump to the stack fetching code
    Value *gp_fp_offset_ge_result = new_value();
    gp_fp_offset_ge_result->type = new_type(TYPE_INT);
    gp_fp_offset_ge_result->type->is_unsigned = 1;
    gp_fp_offset_ge_result->vreg = ++function->vreg_count;

    int last_offset = gp_count ? 48 - gp_count * 8 : 176 - fp_count * 16;
    Value *last_offset_value = new_integral_constant(TYPE_INT, last_offset);
    last_offset_value->type->is_unsigned = 1;

    // Compare and jump to stack code if the arg doesn't fit in the register save area
    ir = insert_instruction_after_from_operation(ir, IR_GT, gp_fp_offset_ge_result, gp_fp_offset, last_offset_value);
    ir = insert_instruction_after_from_operation(ir, IR_JNZ, 0, gp_fp_offset_ge_result, fetch_from_stack);

    *result_gp_fp_offset = gp_fp_offset;

    return ir;
}

static int va_arg_size(Type *type) {
    if (type->type == TYPE_LONG_DOUBLE)
        return 16;
    else if (type->type == TYPE_STRUCT_OR_UNION) {
        int size = get_type_size(type);
        return (size + 7) & ~7;
    }
    else
        return 8;
}

// Add instructions to read a vararg from the register save area
static Tac *add_function_va_arg_register_save_area_read(Function *function, Tac *ir, Type *type, Value *va_list, int gp_count, int fp_count, Value *gp_fp_offset, Value *dst) {
    // Read register_save_area into register_save_area register
    Value *register_save_area;
    ir = read_from_va_list(function, ir, va_list, make_pointer_to_void(), make_pointer(dst->type), 16, &register_save_area);

    // Add gp_fp_offset to register_save_area
    Value *ptr = new_value();
    ptr->type = make_pointer(dst->type);
    ptr->vreg = ++function->vreg_count;
    ir = insert_instruction_after_from_operation(ir, IR_ADD, ptr, register_save_area, gp_fp_offset);

    if (type->type == TYPE_STRUCT_OR_UNION) {
        // Copy memory for struct/union
        Value *memcpy_ptr = dup_value(ptr);
        memcpy_ptr->is_lvalue = 1;
        int size = va_arg_size(type);
        int splay_offsets = fp_count > 0; // Splay offsets if it's an SSE struct
        ir = add_memory_copy_with_registers(function, ir, dst, memcpy_ptr, size, splay_offsets);
    }
    else
        // Read a scalar value from a pointer
        ir = insert_instruction_after_from_operation(ir, IR_INDIRECT, dst, ptr, 0);

    // Add 8 or 16 to gp_fp_offset register
    Value *new_gp_fp_offset = dup_value(gp_fp_offset);
    new_gp_fp_offset->vreg = ++function->vreg_count;
    Value *size = new_integral_constant(TYPE_INT, gp_count * 8 + fp_count * 16);
    size->type->is_unsigned = 1;
    ir = insert_instruction_after_from_operation(ir, IR_ADD, new_gp_fp_offset, gp_fp_offset, size);

    // Write new gp offset back to va_list
    int offset = gp_count ? 0 : 4; // gp_offset or fp_offset
    ir = write_to_va_list(function, ir, va_list, new_gp_fp_offset, offset);

    return ir;
}

// Add instructions to read a vararg from the stack
static Tac *add_function_va_arg_stack_read(Function *function, Tac *ir, Type *type, Value *va_list, Value *dst) {
    int size = va_arg_size(type);
    Value *size_value = new_integral_constant(TYPE_INT, size);
    size_value->type->is_unsigned = 1;

    // Read overflow_arg_area into overflow_arg_area register
    Value *overflow_arg_area;
    ir = read_from_va_list(function, ir, va_list, make_pointer_to_void(), make_pointer(dst->type), 8, &overflow_arg_area);

    if (type->type == TYPE_LONG_DOUBLE) {
        // Align overflow arg area to 16 byte boundary, by adding 15 then anding with ~15

        Value *long_overflow_arg_area = dup_value(overflow_arg_area);
        long_overflow_arg_area->type = new_type(TYPE_LONG);

        // Add 15
        Value *tmp1 = dup_value(long_overflow_arg_area);
        tmp1->vreg = ++function->vreg_count;
        ir = insert_instruction_after_from_operation(ir, IR_ADD, tmp1, long_overflow_arg_area, new_integral_constant(TYPE_LONG, 15));

        Value *tmp2 = dup_value(long_overflow_arg_area);
        tmp2->vreg = ++function->vreg_count;
        ir = insert_instruction_after_from_operation(ir, IR_BAND, tmp2, tmp1, new_integral_constant(TYPE_LONG, ~15));
        overflow_arg_area = dup_value(tmp2);
        overflow_arg_area->type = make_pointer(dst->type);
    }

    // Read arg from stack
    if (type->type == TYPE_STRUCT_OR_UNION) {
        // Copy memory for struct/union
        Value *memcpy_overflow_arg_area = dup_value(overflow_arg_area);
        memcpy_overflow_arg_area->is_lvalue = 1;
        ir = add_memory_copy(function, ir, dst, memcpy_overflow_arg_area, size);
    }
    else
        // Read a scalar value from a pointer
        ir = insert_instruction_after_from_operation(ir, IR_INDIRECT, dst, overflow_arg_area, 0);

    // Add 8 or 16 to overflow_arg_area register
    Value *new_overflow_arg_area = dup_value(overflow_arg_area);
    new_overflow_arg_area->vreg = ++function->vreg_count;
    ir = insert_instruction_after_from_operation(ir, IR_ADD, new_overflow_arg_area, overflow_arg_area, size_value);

    // Write new gp overflow_arg_area back to va_list
    // Value *final_gp_overflow_arg_area = dup_value(va_list);
    // final_gp_overflow_arg_area->type = make_pointer_to_void();
    // final_gp_overflow_arg_area->offset = 8;
    // ir = insert_instruction_after_from_operation(ir, IR_MOVE, va_list, new_overflow_arg_area, 0);
    ir = write_to_va_list(function, ir, va_list, new_overflow_arg_area, 8);

    return ir;
}

// Generate code to read a vararg from either the register save area or the stack
static void process_function_va_arg(Function *function, Tac *ir) {
    Value *va_list = ir->src1;
    Value *dst = ir->dst;
    Type *type = dst->type;

    ir->operation = IR_NOP;
    ir->src1 = 0;
    ir->dst = 0;

    if (type->type == TYPE_LONG_DOUBLE) {
        ir = add_function_va_arg_stack_read(function, ir, type, va_list, dst);
        return;
    }

    // Determine how many gp and fp registers are needed
    int gp_count = 0; // Integer registers
    int fp_count = 0; // SSE registers

    if (type->type == TYPE_STRUCT_OR_UNION) {
        FunctionParamAllocation *fpa = init_function_param_allocaton("vararg struct");
        add_function_param_to_allocation(fpa, type);
        FunctionParamLocations *fpl = &(fpa->params[0]);

        if (fpl->locations[0].stack_offset != -1) {
            // It's on the stack
            ir = add_function_va_arg_stack_read(function, ir, type, va_list, dst);
            return;
        }

        // The arg can be in registers. Although it might end up on the stack
        // if registers have run out.
        else if (fpl->locations[0].int_register != -1)
            gp_count = fpl->count;
        else
            fp_count = fpl->count;
    }
    else {
        // A scalar arg is either in int (gp) or sse (fp) registers
        gp_count = is_integer_type(type) || is_pointer_type(type);
        fp_count = !gp_count;
    }

    Value *ldone = new_label_dst();
    Value *fetch_from_stack = new_label_dst();

    // Check if the arg is in the register save area or on stack
    Value *gp_fp_offset;
    ir = add_function_va_arg_in_register_check(function, ir, type, va_list, gp_count, fp_count, fetch_from_stack, &gp_fp_offset);

    // Read from register save area & jump to the done label
    ir = add_function_va_arg_register_save_area_read(function, ir, type, va_list, gp_count, fp_count, gp_fp_offset, dst);
    ir = insert_instruction_after_from_operation(ir, IR_JMP, 0, ldone, 0);

    // Add fetch_from_stack label
    ir = insert_instruction_after_from_operation(ir, IR_NOP, 0, 0, 0);
    ir->label = fetch_from_stack->label;

    // Read from stack
    ir = add_function_va_arg_stack_read(function, ir, type, va_list, dst);

    // Add done label
    ir = insert_instruction_after_from_operation(ir, IR_NOP, 0, 0, 0);
    ir->label = ldone->label;
}

void process_function_varargs(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_VA_START) process_function_va_start(function, ir);
        else if (ir->operation == IR_VA_ARG) process_function_va_arg(function, ir);
    }
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

    int fpa_start = 0; // Which index in fpa->params has the first actual parameter

    if (function->type->target->type == TYPE_STRUCT_OR_UNION) {
        fpa_start = setup_return_for_struct_or_union(function);
        if (fpa_start) add_function_param_to_allocation(fpa, function->return_value_pointer->type);
    }

    for (int i = 0; i < function->type->function->param_count; i++)
        add_function_param_to_allocation(fpa, function->type->function->param_types->elements[i]);

    function->fpa = fpa;
    finalize_function_param_allocation(fpa);

    int *register_param_vregs = wmalloc(sizeof(int) * function->type->function->param_count);
    memset(register_param_vregs, -1, sizeof(int) * function->type->function->param_count);

    int *register_param_stack_indexes = wcalloc(function->type->function->param_count, sizeof(int));

    int *has_address_of = wcalloc(function->type->function->param_count, sizeof(int));

    // The stack is never bigger than function->type->function->param_count * 2 & starts at 2
    int *stack_param_vregs = wmalloc(sizeof(int) * (function->type->function->param_count * 2 + 2));
    memset(stack_param_vregs, -1, sizeof(int) * (function->type->function->param_count * 2 + 2));

    // Determine which parameters in registers are used in IR_ADDRESS_OF instructions
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->dst);
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->src1);
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->src2);
    }

    // Add moves for registers
    for (int i = 0; i < function->type->function->param_count; i++) {
        if (fpa->params[fpa_start + i].locations[0].stack_offset != -1) continue;

        Type *type = function->type->function->param_types->elements[i];

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
            if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

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

    if (function->type->function->is_variadic) add_function_vararg_param_moves(function, fpa);

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
    int *stack_index_remap = wmalloc(sizeof(int) * (function->type->function->param_count + 2));
    memset(stack_index_remap, -1, sizeof(int) * (function->type->function->param_count + 2));

    // Determine stack offsets for parameters on the stack and add moves

    for (int i = 0; i < function->type->function->param_count; i++) {
        if (fpa->params[fpa_start + i].locations[0].stack_offset == -1) continue;

        Type *type = function->type->function->param_types->elements[i];

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

    FunctionParamAllocation *fpa = wcalloc(1, sizeof(FunctionParamAllocation));
    fpa->params = wmalloc(sizeof(FunctionParamLocations) * MAX_FUNCTION_CALL_ARGS);

    return fpa;
}

// Using the state of already allocated registers & stack entries in fpa, determine the location for a type and set it in fpl.
static void add_type_to_allocation(FunctionParamAllocation *fpa, FunctionParamLocation *fpl, Type *type, int force_stack) {
    fpl->int_register = -1;
    fpl->sse_register = -1;
    fpl->stack_offset = -1;
    fpl->stack_padding = -1;

    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
    if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

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

// Recurse through a type and make list of all scalars + their offsets
static void flatten_type(Type *type, StructOrUnionScalars *scalars, int offset) {
    if (type->type == TYPE_STRUCT_OR_UNION) {
        StructOrUnion *s = type->struct_or_union_desc;
        for (StructOrUnionMember **pmember = s->members; *pmember; pmember++) {
            StructOrUnionMember *member = *pmember;
            flatten_type(member->type, scalars, offset + member->offset);
        }
    }

    else if (type->type == TYPE_ARRAY) {
        int element_size = get_type_size(type->target);
        for (int i = 0; i < type->array_size; i++)
            flatten_type(type->target, scalars, offset + element_size * i);
    }

    else {
        StructOrUnionScalar *scalar = wmalloc(sizeof(StructOrUnionScalar));
        if (scalars->count == MAX_STRUCT_OR_UNION_SCALARS) panic("Exceeded max number of scalars in a struct/union");
        scalars->scalars[scalars->count++] = scalar;
        scalar->type = type;
        scalar->offset = offset;
    }
}

static void add_single_stack_function_param_location(FunctionParamAllocation *fpa, Type *type) {
    FunctionParamLocations *fpl = &(fpa->params[fpa->arg_count]);
    fpl->locations = wmalloc(sizeof(FunctionParamLocation));
    fpl->count = 1;
    add_type_to_allocation(fpa, &(fpl->locations[0]), type, 0);

}
// Add a param/arg to a function and allocate registers & stack entries
// Structs are decomposed.
void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type) {
    if (fpa->arg_count > MAX_FUNCTION_CALL_ARGS) panic("Maximum function call arg count of %d exceeded", MAX_FUNCTION_CALL_ARGS);

    if (type->type != TYPE_STRUCT_OR_UNION) {
        // Create a single location for the arg
        add_single_stack_function_param_location(fpa, type);
    }

    else {
        if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
        if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

        int size = get_type_size(type);
        if (size > 16) {
            // The entire thing is on the stack
            add_single_stack_function_param_location(fpa, type);
        }

        else {
            // Decompose a struct or union into up to 8 8-bytes

            StructOrUnionScalars *scalars = wmalloc(sizeof(StructOrUnionScalars));
            scalars->scalars = wmalloc(sizeof(StructOrUnionScalar) * MAX_STRUCT_OR_UNION_SCALARS);
            scalars->count = 0;
            flatten_type(type, scalars, 0);

            // Classify the eight bytes
            char *seen_integer = wcalloc(8, sizeof(char));
            char *seen_sse = wcalloc(8, sizeof(char));
            char *seen_memory = wcalloc(8, sizeof(char));
            char *member_counts = wcalloc(8, sizeof(char));

            int unaligned = 0;

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

                if (scalar->offset & (get_type_alignment(scalar->type) - 1))
                    unaligned = 1;
            }

            // Determine number of 8-bytes & allocate memory
            int eight_bytes_count = (size + 7) / 8;
            FunctionParamLocations *fpl = &(fpa->params[fpa->arg_count]);
            fpl->locations = wmalloc(sizeof(FunctionParamLocation) * eight_bytes_count);
            fpl->count = eight_bytes_count;

            // If one of the classes is MEMORY, the whole argument is passed in memory.
            int in_memory = 0;
            for (int i = 0; i < eight_bytes_count; i++) in_memory |= seen_memory[i];

            if  (in_memory || unaligned) {
                // The entire thing is on the stack
                add_single_stack_function_param_location(fpa, type);
            }

            else {
                FunctionParamAllocation *backup_fpa = wmalloc(sizeof(FunctionParamAllocation));
                *backup_fpa = *fpa;
                int on_stack = 0;

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

                    if (fpl->locations[i].stack_offset != -1) on_stack = 1;
                }

                // Part of the struct/union overflowed into the stack due to a shortage
                // of registers. Roll back & put the whole thing in the stack.
                if (on_stack && eight_bytes_count > 1) {
                    if (debug_function_param_allocation) printf("         ran out of registers, rewinding ... \n");
                    *fpa = *backup_fpa;
                    add_single_stack_function_param_location(fpa, type);
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
