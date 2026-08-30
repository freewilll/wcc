#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

#include "x86_64.h"

// Is a type a floating point type, but not a long double?
static int is_sse_floating_point_type(Type *type) {
    return get_preg_class_for_scalar_type(type) == PC_FP;
}

// This implements the reverse of make_int_struct_or_union_arg_move_instructions
// This is also used for moving struct/unions into function return value registers
static int make_int_struct_or_union_move_from_register_to_stack_instructions(
        Function *function, Tac *ir, Type *type, CallValueLocation* pl,
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

        new_tac_before(ir, IR_MOVE, dst, shift_register, 0, 0);

        size -= size_unit;
        offset += size_unit;
        if (size == 0) break;

        // Shift the register over size_unit * 8 bytes
        shift_register = dup_value(shift_register);
        shift_register->type = new_type(TYPE_LONG);

        Value *new_shift_register = dup_value(shift_register);
        new_shift_register->live_range_preg = 0;
        new_shift_register->vreg = ++function->vreg_count;

        new_tac_before(ir, IR_BSHR, new_shift_register, shift_register, new_integral_constant(TYPE_LONG, size_unit * 8), 0);

        shift_register = new_shift_register;
    }

    return live_range_preg;
}

// This implements the reverse of make_sse_struct_or_union_arg_move_instructions.
// This is also used for moving struct/unions into function return value registers.
static int make_sse_struct_or_union_move_from_register_to_stack_instructions(
        Function *function, Tac *ir, Type *type, CallValueLocation* pl,
        int register_index, int stack_index, RegisterSet *register_set, int param_register_vreg) {

    if (debug_function_param_mapping)
        printf("Adding param move from sse register %d size %d to stack_index %d, offset %d\n", register_index, pl->stru_size, stack_index, pl->stru_offset);

    if (!param_register_vreg) param_register_vreg = ++function->vreg_count;

    // Make param register value
    Value *param_register = new_value();
    param_register->vreg = param_register_vreg;
    int live_range_preg = register_set->fp_registers[register_index];
    param_register->live_range_preg = live_range_preg;

    if (pl->stru_size == 4) {
        // Move a single float
        param_register->type = new_type(TYPE_FLOAT);
        Value *dst = new_value_in_stack(TYPE_FLOAT, stack_index, pl->stru_offset);
        new_tac_before(ir, IR_MOVE, dst, param_register, 0, 0);
    }

    else if (pl->stru_size == 8 && pl->stru_member_count == 1) {
        // Move a single double
        param_register->type = new_type(TYPE_DOUBLE);
        Value *dst = new_value_in_stack(TYPE_DOUBLE, stack_index, pl->stru_offset);
        new_tac_before(ir, IR_MOVE, dst, param_register, 0, 0);
    }

    else {
        // Move two floats. It must first be copied into an integer register and then
        // copied to the stack.
        param_register->type = new_type(TYPE_DOUBLE);
        Value *temp_int = new_value();
        temp_int->type = new_type(TYPE_LONG);
        temp_int->vreg = ++function->vreg_count;
        new_tac_before(ir, IR_MOVE_PREG_CLASS, temp_int, param_register, 0, 0);

        Value *dst = new_value_in_stack(TYPE_LONG, stack_index, pl->stru_offset);
        new_tac_before(ir, IR_MOVE, dst, temp_int, 0, 0);
    }

    return live_range_preg;
}

// Move struct/union data from registers or memory to the destination of a function call
static void add_function_call_result_moves_for_struct_or_union(Function *function, Tac *ir) {
    if (ir->dst->stack_index == 0) panic("Expected a stack index for a function call returning a struct/union");

    int stack_index = ir->dst->stack_index;
    Type *type = ir->dst->type;

    Type *function_type = ir->src1->type;

    CallValueAllocation *cva = function_type->function->return_value_cva;
    if (!cva) panic("In add_function_call_result_moves_for_struct_or_union() got an empty RV cva");
    CallValueLocations *cvl = cva->locations->elements[0];

    Value *function_value = ir->src1;

    if (cvl->locations[0].stack_offset == -1) {
        // Move registers to a struct/union on the stack

        ir->dst = 0;
        ir = ir->next;

        // Allocate result vregs and create a live range or them
        int *live_range_pregs = wmalloc(sizeof(int) * 8);
        for (int loc = 0; loc < cvl->count; loc++) {
            CallValueLocation *location = &(cvl->locations[loc]);
            live_range_pregs[loc] = ++function->vreg_count;
            Value *dst = new_value();
            dst->vreg = live_range_pregs[loc];
            dst->type = location->int_register != -1 ? new_type(TYPE_LONG) : new_type(TYPE_DOUBLE);
            new_tac_before(ir, IR_CALL_ARG_REG, dst, 0, 0, 1);
        }

        for (int loc = 0; loc < cvl->count; loc++) {
            CallValueLocation *location = &(cvl->locations[loc]);

            int live_range_preg;
            int param_register_vreg = live_range_pregs[loc];
            if (location->int_register != -1)
                live_range_preg = make_int_struct_or_union_move_from_register_to_stack_instructions(
                    function, ir, type, location, location->int_register, stack_index,
                    &function_return_value_register_set, param_register_vreg);
            else if (location->fp_register != -1)
                live_range_preg = make_sse_struct_or_union_move_from_register_to_stack_instructions(
                    function, ir, type, location, location->fp_register, stack_index,
                    &function_return_value_register_set, param_register_vreg);
            else
                panic("Got unexpected stack offset in add_function_call_result_moves_for_struct_or_union");

            add_to_set(function_value->return_value_live_ranges, live_range_preg);
        }

        wfree(live_range_pregs);
    }
    else {
        // Move registers to a struct/union in memory. rax contains the address

        // Ensure RAX doesn't get clobbered by the function call
        add_to_set(function_value->return_value_live_ranges, LIVE_RANGE_PREG_RAX);

        // Take address of dst struct/union on the stack
        Value *address_value = new_value();
        address_value->vreg = ++function->vreg_count;
        address_value->type = make_pointer_to_void();
        new_tac_before(ir, IR_ADDRESS_OF, address_value, ir->dst, 0, 1);

        // Move struct/union target address into rdi
        Value *rdi_value = new_value();
        rdi_value->vreg = ++function->vreg_count;
        rdi_value->type = make_pointer_to_void();
        rdi_value->live_range_preg = LIVE_RANGE_PREG_RDI;
        new_tac_before(ir, IR_MOVE, rdi_value, address_value, 0, 1);

        // Setup dst for rax
        Value *struct_dst = ir->dst;

        ir->dst = new_value();
        ir->dst->vreg = ++function->vreg_count;
        ir->dst->type = make_pointer_to_void();
        ir->dst->live_range_preg = LIVE_RANGE_PREG_RAX;

        // Prepare src1 to also be rax, but as an lvalue
        Value *src1 = dup_value(ir->dst);
        src1->is_lvalue = 1;

        // Add the memory copy
        int size = get_type_size(type);
        ir = add_memory_copy(function, ir, struct_dst, src1, size);
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
            tac->src1->live_range_preg = is_sse ? LIVE_RANGE_PREG_XMM00 : LIVE_RANGE_PREG_RAX;
            add_to_set(ir->src1->return_value_live_ranges, tac->src1->live_range_preg);

            ir->dst = value;
            insert_tac_before(ir->next, tac, 1);
        }
    }
}

// Move struct or union of size <= 32 into rax/rdx or xmm0/xmm1
static void add_function_return_moves_for_struct_or_union(Function *function, Tac *ir, char *identifier) {
    // Determine registers
    CallValueAllocation *cva = init_call_value_allocaton(identifier);
    add_type_to_cva(cva, ir->src1->type);
    CallValueLocations *cvl = cva->locations->elements[0];

    Value **function_call_values = wcalloc(2, sizeof(Value *));

    if (cvl->locations[0].stack_offset != -1) {
        // Move data into memory

        ir->operation.id = IR_NOP;

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
        dst->live_range_preg = LIVE_RANGE_PREG_RAX;
        dst->type = make_pointer_to_void();
        dst->vreg = ++function->vreg_count;

        ir = new_tac_after(ir, IR_MOVE, dst, function->return_value_pointer, 0);
        ir = new_tac_after(ir, IR_RETURN, 0, 0, 0);
    }
    else {
        // Move the data into registers
        for (int loc = cvl->count - 1; loc >= 0; loc--) {
            CallValueLocation *location = &(cvl->locations[loc]);
            int preg_class = (location->int_register != -1) ? PC_INT : PC_FP;
            int register_index = (preg_class == PC_INT) ? location->int_register : location->fp_register;
            Value *param = ir->src1;
            int vreg = make_struct_or_union_arg_move_instructions(function, ir, param, preg_class, register_index, location, &function_return_value_register_set);
            function_call_values[loc] = new_value();
            function_call_values[loc]->type = preg_class == PC_INT ? new_type(TYPE_LONG) : new_type(TYPE_DOUBLE);
            function_call_values[loc]->vreg = vreg;
        }

        // Add instructions to ensure the values stay in the registers
        add_ir_call_reg_instructions(ir, function_call_values, 2);
    }

    wfree(function_call_values);
}

// Add a move for a function return value. If it's a long double, a load can be done,
// otherwise, either rax or xmm0 must hold the result.
static void add_function_return_moves(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if ((ir->operation.id == IR_RETURN && !ir->src1) || ir->operation.id != IR_RETURN) continue;

        // Implicit else, operation == IR_RETURN & ir-src1 has a value
        if (ir->src1->type->type == TYPE_LONG_DOUBLE) {
            new_tac_before(ir, IR_LOAD_LONG_DOUBLE, 0, ir->src1, 0, 1);

            ir->operation.id = IR_RETURN;
            ir->dst = 0;
            ir->src1 = 0;
            ir->src2 = 0;
        }

        else if (ir->src1->type->type == TYPE_STRUCT_OR_UNION)
            add_function_return_moves_for_struct_or_union(function, ir, function->identifier);

        else {
            int is_sse = is_sse_floating_point_type(function->type->target);
            int live_range_preg = is_sse ? LIVE_RANGE_PREG_XMM00 : LIVE_RANGE_PREG_RAX;

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

        ir->src1 = 0;
    }
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
    CallValueLocation *pl, RegisterSet *register_set) {

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
            load_struct_scalar_into_value(function, ir, param, pl, type, result_register, offset);
            temp_loaded = 1;
        }
        else {
            // Load value
            Value *loaded_value = make_long_temp_vreg(function);
            Value *temp2 = dup_value(param);
            temp2->type = new_type(TYPE_CHAR + i);
            temp2->type->is_unsigned = 1;
            temp2->offset += pl->stru_offset + offset;
            new_tac_before(ir, lvalue_in_register ? IR_INDIRECT : IR_MOVE, loaded_value, temp2, 0, 1);

            // Shift loaded value
            Value *shifted_value;
            if (offset) {
                shifted_value = make_long_temp_vreg(function);
                new_tac_before(ir, IR_BSHL, shifted_value, loaded_value, new_integral_constant(TYPE_LONG, offset * 8), 1);
            }
            else
                shifted_value = loaded_value;

            // Bitwise or shifted_value and put result in result_register
            Value *orred_value = make_long_temp_vreg(function);
            new_tac_before(ir, IR_BOR, orred_value, shifted_value, result_register, 1);
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
    CallValueLocation *pl, RegisterSet *register_set) {

    if (debug_function_arg_mapping) printf("Adding arg move from struct to SSE register_index=%d register size=%d\n", register_index, pl->stru_size);

    if (pl->stru_size == 4) {
        // Move a single float
        Value *temp = load_struct_scalar_into_new_vreg(function, ir, param, pl, new_type(TYPE_FLOAT));
        return add_arg_move_to_register(function, ir, new_type(TYPE_FLOAT), temp, preg_class, register_index, register_set);
    }
    else if (pl->stru_size == 8 && pl->stru_member_count == 1) {
        // Move a single double
        Value *temp = load_struct_scalar_into_new_vreg(function, ir, param, pl, new_type(TYPE_DOUBLE));
        return add_arg_move_to_register(function, ir, new_type(TYPE_DOUBLE), temp, preg_class, register_index, register_set);
    }
    else {
        // Move two floats. It must first be loaded into an integer register and then
        // copied to an SSE register.
        Value *temp_int = load_struct_scalar_into_new_vreg(function, ir, param, pl, new_type(TYPE_LONG));
        Value *temp_sse = new_value();
        temp_sse->type = new_type(TYPE_DOUBLE);
        temp_sse->vreg = ++function->vreg_count;
        new_tac_before(ir, IR_MOVE_PREG_CLASS, temp_sse, temp_int, 0, 1);
        return add_arg_move_to_register(function, ir, new_type(TYPE_DOUBLE), temp_sse, preg_class, register_index, register_set);
    }
}

// Load a function parameter register from an struct or union 8-byte
int make_struct_or_union_arg_move_instructions(
        Function *function, Tac *ir, Value *param, int preg_class, int register_index,
        CallValueLocation *location, RegisterSet *register_set) {

    if (preg_class == PC_INT)
        return make_int_struct_or_union_arg_move_instructions(function, ir, param, preg_class, register_index, location, register_set);
    else
        return make_sse_struct_or_union_arg_move_instructions(function, ir, param, preg_class, register_index, location, register_set);
}

// Add instructions to move struct/union data from a param register to a struct on the stack
int add_struct_or_union_param_move(Function *function, Tac *ir, Type *type, CallValueLocations *pl, RegisterSet *register_set) {
    // Allocate space on the stack for the struct
    Value *v = new_value();
    v->type = dup_type(type);
    v->is_lvalue = 1;
    v->stack_index = -(++function->stack_register_count);
    new_tac_before(ir, IR_DECL_LOCAL_COMP_OBJ, 0, v, 0, 0);

    for (int loc = 0; loc < pl->count; loc++) {
        CallValueLocation *location = &(pl->locations[loc]);

        if (location->int_register != -1)
            make_int_struct_or_union_move_from_register_to_stack_instructions(function, ir, type, location, location->int_register, v->stack_index, register_set, 0);
        else if (location->fp_register != -1)
            make_sse_struct_or_union_move_from_register_to_stack_instructions(function, ir, type, location, location->fp_register, v->stack_index, register_set, 0);
        else
            panic("Got unexpected stack offset in add_struct_or_union_param_move()");
    }

    return v->stack_index;
}

// If the function returns a struct/union in memory, then the caller puts a pointer to
// the target in rdi. Make a copy of rdi in return_value_pointer for use in the
// return value code. The function returns 1 if rdi has been used in this way.
static int setup_return_for_struct_or_union(Function *function) {
    CallValueAllocation *cva = function->type->function->return_value_cva;
    if (!cva) panic("In setup_return_for_struct_or_union() got an empty RV cva");
    if (CVA_CVL(cva, 0).locations[0].stack_offset == -1) return 0;

    function->return_value_pointer = new_value();
    function->return_value_pointer->vreg = ++function->vreg_count;
    function->return_value_pointer->type = make_pointer_to_void();

    // Make value for rdi register
    Value *src1 = new_value();
    src1->type = make_pointer_to_void();
    src1->live_range_preg = LIVE_RANGE_PREG_RDI;
    src1->type = make_pointer_to_void();
    src1->vreg = ++function->vreg_count;

    Value *dst = dup_value(function->return_value_pointer);

    new_tac_before(ir, IR_MOVE, dst, src1, 0, 0);

    return 1;
}

// For functions with variadic arguments, move registers into the register save area.
// The register save area has been allocated on the stack by the parser with the
// value set in function->register_save_area.
void add_function_vararg_param_moves(Function *function, CallValueAllocation *cva) {
    // Add moves for ints registers to register save area
    for (int i = cva->single_int_register_arg_count; i < 6; i++) {
        Value *src = new_value();
        src->vreg = ++function->vreg_count;
        src->type = new_type(TYPE_LONG);
        src->live_range_preg = int_arg_registers[i];
        Value *dst = dup_value(function->register_save_area);
        dst->type = new_type(TYPE_LONG);
        dst->offset = i * 8;
        ir = new_tac_after(ir, IR_MOVE, dst, src, 0);
    }

    // Skip SSE register copying if al is zero with a jump to "done"
    Value *ldone = new_label_dst();
    Value *rax = new_value();
    rax->vreg = ++function->vreg_count;
    rax->type = new_type(TYPE_CHAR);
    rax->live_range_preg = LIVE_RANGE_PREG_RAX;
    ir = new_tac_after(ir, IR_JZ, 0, rax, ldone);

    // Add moves for SSE registers to register save area
    for (int i = cva->single_fp_register_arg_count; i < 8; i++) {
        Value *src = new_value();
        src->vreg = ++function->vreg_count;
        src->type = new_type(TYPE_DOUBLE);
        src->live_range_preg = fp_arg_registers[i];

        Value *temp_int = new_value();
        temp_int->type = new_type(TYPE_LONG);
        temp_int->vreg = ++function->vreg_count;
        ir = new_tac_after(ir, IR_MOVE_PREG_CLASS, temp_int, src, 0);

        Value *dst = dup_value(function->register_save_area);
        dst->type = new_type(TYPE_LONG);
        dst->offset = 48 + i * 16;
        ir = new_tac_after(ir, IR_MOVE, dst, temp_int, 0);
    }

    // Add done label
    ir = new_tac_after(ir, IR_NOP, 0, 0, 0);
    ir->label = ldone->label;
}

// Add code for a va_start() call. The va_list struct, present in src1, has to be
// populated.
static void process_function_va_start(Function *function, Tac *ir) {
    Value *va_list = ir->src1;
    ir->operation.id = IR_NOP;
    ir->src1 = 0;

    // Set va_list.fp_offset, the offset of the first vararg integer register
    Value *fp_offset_value = new_value();
    fp_offset_value->type = new_type(TYPE_INT);
    fp_offset_value->type->is_unsigned = 1;
    fp_offset_value->is_constant = 1;
    fp_offset_value->int_value = function->cva->single_int_register_arg_count * 8;

    Value *dst = dup_value(va_list);
    dst->type = new_type(TYPE_INT);
    dst->type->is_unsigned = 1;
    ir = new_tac_after(ir, IR_MOVE, dst, fp_offset_value, 0);

    // Set va_list.gp_offset, the offset of the first vararg SSE register
    Value *gp_offset_value = dup_value(fp_offset_value);
    gp_offset_value->int_value = 48 + function->cva->single_fp_register_arg_count * 16;
    dst = dup_value(dst);
    dst->offset = 4;
    ir = new_tac_after(ir, IR_MOVE, dst, gp_offset_value, 0);

    // Set va_list.overflow_arg_area, the address of the first vararg pushed on the stack
    Value *tmp_dst = new_value();
    tmp_dst->type = make_pointer_to_void();
    tmp_dst->vreg = ++function->vreg_count;

    Value *overflow = dup_value(dst);
    overflow->type = make_pointer_to_void();
    overflow->vreg = 0;
    overflow->offset = 0;
    overflow->stack_index = OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX;
    ir = new_tac_after(ir, IR_ADDRESS_OF, tmp_dst, overflow, 0);

    dst = dup_value(dst);
    dst->type = make_pointer_to_void();
    dst->offset = 8;
    ir = new_tac_after(ir, IR_MOVE, dst, tmp_dst, 0);

    // Set va_list.reg_save_area, the address of the register save area
    tmp_dst = new_value();
    tmp_dst->type = make_pointer_to_void();
    tmp_dst->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_ADDRESS_OF, tmp_dst, function->register_save_area, 0);

    dst = dup_value(dst);
    dst->type = make_pointer_to_void();
    dst->offset = 16;
    ir = new_tac_after(ir, IR_MOVE, dst, tmp_dst, 0);
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
        ir = new_tac_after(ir, IR_MOVE, tmp, va_list, 0);

        src1->type = make_pointer(dst_type);
        src1->offset = offset;
        ir = new_tac_after(ir, IR_INDIRECT, *result, src1, 0);
    }
    else {
        // Read from the stack
        src1->offset = offset;
        ir = new_tac_after(ir, IR_MOVE, *result, src1, 0);
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
        ir = new_tac_after(ir, IR_MOVE, tmp, value, 0);
    }
    else {
        // Write to the stack
    dst->type = value->type;
        dst->offset = offset;
        ir = new_tac_after(ir, IR_MOVE, dst, value, 0);
    }

    return ir;
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
    ir = new_tac_after(ir, IR_GT, gp_fp_offset_ge_result, gp_fp_offset, last_offset_value);
    ir = new_tac_after(ir, IR_JNZ, 0, gp_fp_offset_ge_result, fetch_from_stack);

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
    ir = new_tac_after(ir, IR_ADD, ptr, register_save_area, gp_fp_offset);

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
        ir = new_tac_after(ir, IR_INDIRECT, dst, ptr, 0);

    // Add 8 or 16 to gp_fp_offset register
    Value *new_gp_fp_offset = dup_value(gp_fp_offset);
    new_gp_fp_offset->vreg = ++function->vreg_count;
    Value *size = new_integral_constant(TYPE_INT, gp_count * 8 + fp_count * 16);
    size->type->is_unsigned = 1;
    ir = new_tac_after(ir, IR_ADD, new_gp_fp_offset, gp_fp_offset, size);

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
        ir = new_tac_after(ir, IR_ADD, tmp1, long_overflow_arg_area, new_integral_constant(TYPE_LONG, 15));

        Value *tmp2 = dup_value(long_overflow_arg_area);
        tmp2->vreg = ++function->vreg_count;
        ir = new_tac_after(ir, IR_BAND, tmp2, tmp1, new_integral_constant(TYPE_LONG, ~15));
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
        ir = new_tac_after(ir, IR_INDIRECT, dst, overflow_arg_area, 0);

    // Add 8 or 16 to overflow_arg_area register
    Value *new_overflow_arg_area = dup_value(overflow_arg_area);
    new_overflow_arg_area->vreg = ++function->vreg_count;
    ir = new_tac_after(ir, IR_ADD, new_overflow_arg_area, overflow_arg_area, size_value);

    // Write new gp overflow_arg_area back to va_list
    // Value *final_gp_overflow_arg_area = dup_value(va_list);
    // final_gp_overflow_arg_area->type = make_pointer_to_void();
    // final_gp_overflow_arg_area->offset = 8;
    // ir = new_tac_after(ir, IR_MOVE, va_list, new_overflow_arg_area, 0);
    ir = write_to_va_list(function, ir, va_list, new_overflow_arg_area, 8);

    return ir;
}

// Generate code to read a vararg from either the register save area or the stack
static void process_function_va_arg(Function *function, Tac *ir) {
    Value *va_list = ir->src1;
    Value *dst = ir->dst;
    Type *type = dst->type;

    ir->operation.id = IR_NOP;
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
        CallValueAllocation *cva = init_call_value_allocaton("vararg struct");
        add_type_to_cva(cva, type);
        CallValueLocations *cvl = cva->locations->elements[0];

        if (cvl->locations[0].stack_offset != -1) {
            // It's on the stack
            ir = add_function_va_arg_stack_read(function, ir, type, va_list, dst);
            return;
        }

        // The arg can be in registers. Although it might end up on the stack
        // if registers have run out.
        else if (cvl->locations[0].int_register != -1)
            gp_count = cvl->count;
        else
            fp_count = cvl->count;
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
    ir = new_tac_after(ir, IR_JMP, 0, ldone, 0);

    // Add fetch_from_stack label
    ir = new_tac_after(ir, IR_NOP, 0, 0, 0);
    ir->label = fetch_from_stack->label;

    // Read from stack
    ir = add_function_va_arg_stack_read(function, ir, type, va_list, dst);

    // Add done label
    ir = new_tac_after(ir, IR_NOP, 0, 0, 0);
    ir->label = ldone->label;
}

static void process_function_varargs(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation.id == IR_VA_START) process_function_va_start(function, ir);
        else if (ir->operation.id == IR_VA_ARG) process_function_va_arg(function, ir);
    }
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
    int is_single_fp_register = is_sse_floating_point_type(type);
    int is_long_double = type->type == TYPE_LONG_DOUBLE;
    int in_stack =
        is_long_double ||
        force_stack ||
        (is_single_int_register && cva->single_int_register_arg_count >= 6) || (is_single_fp_register && cva->single_fp_register_arg_count >= 8);

    int alignment = get_type_alignment(type);
    if (alignment < 8) alignment = 8;

    if (!in_stack && is_single_int_register)
        cvl->int_register = cva->single_int_register_arg_count < 6 ? cva->single_int_register_arg_count : -1;

    else if (!in_stack && is_single_fp_register)
        cvl->fp_register = cva->single_fp_register_arg_count < 8 ? cva->single_fp_register_arg_count : -1;

    else {
        // It's on the stack

        if (alignment > cva->biggest_alignment) cva->biggest_alignment = alignment;
        int padding = ((cva->offset + alignment  - 1) & (~(alignment - 1))) - cva->offset;
        cva->offset += padding;

        cvl->stack_offset = cva->offset;
        cvl->stack_padding = padding;

        int type_size = get_type_size(type);
        if (type_size < 8) type_size = 8;
        cva->offset += type_size;

        if (debug_function_param_allocation)
            printf("  arg %2d with alignment %2d     offset 0x%04x with padding 0x%04x\n", cva->locations->length, alignment, cvl->stack_offset, cvl->stack_padding);
    }

    if (debug_function_param_allocation && !in_stack && (is_single_int_register || is_single_fp_register)) {
        if (cvl->int_register != -1)
            printf("  arg %2d with alignment %2d     int reg %5d\n", cva->locations->length, alignment, cvl->int_register);
        else
            printf("  arg %2d with alignment %2d     sse reg %5d\n", cva->locations->length, alignment, cvl->fp_register);
    }

    cva->single_int_register_arg_count += is_single_int_register;
    if (!in_stack && is_single_fp_register) cva->single_fp_register_arg_count++;
}

// Make a set large enough to hold all live ranges
Set *allocate_return_value_live_ranges(void) {
    return new_set(LIVE_RANGE_PREG_XMM01);
}

// Prepend parameters to a function's parameter list.
// In x86_64, for functions that return a large struct,
// rdi contains the address where the return struct must be copied to.
// Returns the amount of prepended parameters that were added.
int prepend_function_params(Function *function) {
    int cva_start = 0;

    if (function->type->target->type == TYPE_STRUCT_OR_UNION) {
        cva_start = setup_return_for_struct_or_union(function);
        if (cva_start) add_type_to_cva(function->cva, function->return_value_pointer->type);
    }

    return cva_start;
}

// Add a type to a call value allocation
// Add a param/arg to a function and allocate registers & stack entries
// Structs are decomposed.
void add_type_to_cva(CallValueAllocation *cva, Type *type) {
    if (type->type == TYPE_INT128) {
        // An int-128 fits into two 8 bytes

        CallValueLocations *cvl = wcalloc(1, sizeof(CallValueLocations));
        cvl->locations = wmalloc(sizeof(CallValueLocation) * 2);
        cvl->count = 2;

        CallValueAllocation *backup_cva = wmalloc(sizeof(CallValueAllocation));
        *backup_cva = *cva;

        add_type_to_cvl(cva, &(cvl->locations[0]), new_type(TYPE_INT), 0);
        add_type_to_cvl(cva, &(cvl->locations[1]), new_type(TYPE_INT), 0);

        cvl->locations[0].i128_part = 0;
        cvl->locations[1].i128_part = 1;

        int in_stack = cvl->locations[0].stack_offset != -1 || cvl->locations[1].stack_offset != -1;
        if (in_stack) {
            *cva = *backup_cva;
            add_single_call_value_location(cva, type);
            free_call_value_locations(cvl);
        }
        else {
            append_to_list(cva->locations, cvl);
        }

        wfree(backup_cva);
    }

    else if (type->type != TYPE_STRUCT_OR_UNION) {
        // Create a single location for the arg
        add_single_call_value_location(cva, type);
    }

    else {
        if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
        if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

        int size = get_type_size(type);
        if (size > 16) {
            // The entire thing is on the stack
            add_single_call_value_location(cva, type);
        }

        else {
            // Decompose a struct or union into up to 8 8-bytes

            StructOrUnionScalars *scalars = wmalloc(sizeof(StructOrUnionScalars));
            scalars->scalars = wmalloc(sizeof(StructOrUnionScalar *) * MAX_STRUCT_OR_UNION_SCALARS);
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
            CallValueLocations *cvl = wcalloc(1, sizeof(CallValueLocations));
            cvl->locations = wmalloc(sizeof(CallValueLocation) * eight_bytes_count);
            cvl->count = eight_bytes_count;

            // If one of the classes is MEMORY, the whole argument is passed in memory.
            int in_memory = 0;
            for (int i = 0; i < eight_bytes_count; i++) in_memory |= seen_memory[i];

            if  (in_memory || unaligned) {
                // The entire thing is on the stack
                add_single_call_value_location(cva, type);
                free_call_value_locations(cvl);
            }

            else {
                CallValueAllocation *backup_cva = wmalloc(sizeof(CallValueAllocation));
                *backup_cva = *cva;
                int on_stack = 0;

                // Create a location for each eight byte
                for (int i = 0; i < eight_bytes_count; i++) {
                    cvl->locations[i].stru_size = size > 8 ? 8 : size;
                    size -= 8;

                    cvl->locations[i].stru_member_count = member_counts[i];
                    cvl->locations[i].stru_offset = i * 8;

                    if (seen_integer[i])
                        add_type_to_cvl(cva, &(cvl->locations[i]), new_type(TYPE_INT), 0);
                    else
                        add_type_to_cvl(cva, &(cvl->locations[i]), new_type(TYPE_FLOAT), 0);

                    if (cvl->locations[i].stack_offset != -1) on_stack = 1;
                }

                // Part of the struct/union overflowed into the stack due to a shortage
                // of registers. Roll back & put the whole thing in the stack.
                if (on_stack && eight_bytes_count > 1) {
                    if (debug_function_param_allocation) printf("         ran out of registers, rewinding ... \n");
                    *cva = *backup_cva;
                    add_single_call_value_location(cva, type);
                    free_call_value_locations(cvl);
                }
                else
                    append_to_list(cva->locations, cvl);

                wfree(backup_cva);
            }

            wfree(seen_integer);
            wfree(seen_sse);
            wfree(seen_memory);
            wfree(member_counts);

            for (int i = 0; i < scalars->count; i++) wfree(scalars->scalars[i]);
            wfree(scalars->scalars);
            wfree(scalars);
        }
    }
}

// For vregs that are passed on as a function param, but are on the stack,
// Make a mapping from vreg to the stack index.
int *make_original_stack_indexes(Function *function) {
    int *result = wcalloc(function->vreg_count + 1, sizeof(int *));

    for (Tac *tac = function->ir; tac; tac = tac->next)
        if (tac->operation.id == X86_OP_MOV && tac->src1 && tac->src1->function_call.function_param_original_stack_index)
            result[tac->dst->vreg] = tac->src1->function_call.function_param_original_stack_index;

    return result;
}


// Process target function calls, args, params and return values
void process_target_functions(Function *function) {
    // Callee
    initialize_function_return_value_cva(function->type);
    add_function_param_moves(function);
    add_function_return_moves(function);
    process_function_varargs(function);

    // Caller
    process_function_call_arg_allocations(function);
    reverse_function_call_args_order(function);
    add_function_call_result_moves(function);
    add_function_call_arg_moves(function);
}

void add_function_call_clobbers(char *ig, int vreg_count, LongSet *livenow, Tac *tac) {
    // Integer arguments are clobbered
    for (int i = 0; i < 6; i++) {
        if (i == 2) continue; // RDX is a special case, see below
        clobber_livenow(ig, vreg_count, livenow, tac, int_arg_registers[i]);
    }

    // Unless the function returns something in rax, clobber rax
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_RAX))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_RAX);

    // Unless the function returns something in rdx, clobber rdx
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_RDX))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_RDX);

    // All SSE registers xmm2, xmm3, ... are clobbered
    for (int j = 2; j < physical_fp_register_count; j++)
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_XMM00 + j);

    // Unless the function returns something in xmm0, clobber xmm0
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_XMM00))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_XMM00);
    // Unless the function returns something in xmm1, clobber xmm1
    if (!tac->src1->return_value_live_ranges || !in_set(tac->src1->return_value_live_ranges, LIVE_RANGE_PREG_XMM01))
        clobber_livenow(ig, vreg_count, livenow, tac, LIVE_RANGE_PREG_XMM01);

    // If it's a function call from a pointer in a vreg, ensure it doesn't reside in RAX
    if (tac->src1->vreg)
        add_ig_edge(ig, vreg_count, LIVE_RANGE_PREG_RAX, tac->src1->vreg);
}
