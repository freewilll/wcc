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

// Convert v1 = IR_CALL... to:

// 1. v2 = IR_CALL...
// 2. v1 = v2
// v2 will be constrained by the register allocator to bind to RAX.
// Live range coalescing prevents the v1-v2 live range from getting merged with
// a special check for IR_CALL.
void add_function_call_result_moves(Function *function) {
    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_CALL && ir->dst && ir->dst->type->type != TYPE_LONG_DOUBLE) {
            Value *value = dup_value(ir->dst);
            value->vreg = ++function->vreg_count;
            Tac *tac = new_instruction(IR_MOVE);
            tac->dst = ir->dst;
            tac->src1 = value;
            ir->dst = value;
            insert_instruction(ir->next, tac, 1);
        }
    }
}

// Add a move for a function return value. The target of the move will be constraint
// and forced into the RAX register.
void add_function_return_moves(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_RETURN && ir->src1 && ir->src1->type->type != TYPE_LONG_DOUBLE) {
            ir->dst = new_value();
            ir->dst->type = dup_type(function->return_type);
            ir->dst->vreg = ++function->vreg_count;
            ir->src1->preferred_live_range_preg_index = LIVE_RANGE_PREG_RAX_INDEX;
        }
    }
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

    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_ARG) {
            int function_call_register_arg_index = preg_class == PC_INT
                ? ir->src1->function_call_int_register_arg_index
                : ir->src1->function_call_sse_register_arg_index;

            if (function_call_register_arg_index >= 0) {
                int i = ir->src1->int_value * register_count + function_call_register_arg_index;
                arg_values[i] = ir->src2;
                param_indexes[i] = ir->src1->function_call_arg_index;

                ir->operation = IR_NOP;
                ir->dst = 0;
                ir->src1 = 0;
                ir->src2 = 0;
            }
        }

        if (ir->operation == IR_CALL) {
            Value **call_arg = &(arg_values[ir->src1->int_value * register_count]);
            int *param_index = &(param_indexes[ir->src1->int_value * register_count]);
            Function *called_function = ir->src1->function_symbol->type->function;

            // Allocated registers
            int *function_call_arg_registers = malloc(sizeof(int) * register_count);
            memset(function_call_arg_registers, 0, sizeof(int) * register_count);
            Type **function_call_arg_types =  malloc(sizeof(Type *) * register_count);

            // Add the moves backwards so that arg 0 (rsi) is last.
            int i = 0;
            while (i < register_count && *call_arg) {
                call_arg++;
                param_index++;
                i++;
            }

            call_arg--;
            param_index--;
            i--;

            int *arg_registers = preg_class == PC_INT ? int_arg_registers : sse_arg_registers;

            for (; i >= 0; i--) {
                Tac *tac = new_instruction(IR_MOVE);
                tac->dst = new_value();

                int pi = *param_index;
                if (pi >= 0 && pi < called_function->param_count) {
                    Type *type = called_function->param_types[pi];
                    tac->dst->type = dup_type(type);
                }
                else
                    tac->dst->type = dup_type((*call_arg)->type);

                tac->dst->vreg = ++function->vreg_count;

                tac->src1 = *call_arg;
                tac->src1->preferred_live_range_preg_index = arg_registers[i];
                tac->dst->is_function_call_arg = 1;

                if (preg_class == PC_INT)
                    tac->dst->function_call_int_register_arg_index = i;
                else
                    tac->dst->function_call_sse_register_arg_index = i;

                insert_instruction(ir, tac, 1);

                function_call_arg_registers[i] = tac->dst->vreg;
                function_call_arg_types[i] = tac->src1->type;

                call_arg--;
                param_index--;
            }

            // Add IR_CALL_ARG_REG instructions that don't do anything, but ensure
            // that the interference graph and register selection code create
            // a live range for the interval between the function register assignment and
            // the function call. Without this, there is a chance that function call
            // registers are used as temporaries during the code instructions
            // emitted above.
            for (int i = 0; i < register_count; i++) {
                if (function_call_arg_registers[i]) {
                    Tac *tac = new_instruction(IR_CALL_ARG_REG);
                    tac->src1 = new_value();
                    tac->src1->type = function_call_arg_types[i];
                    tac->src1->vreg = function_call_arg_registers[i];
                    insert_instruction(ir, tac, 1);
                }
            }

            free(function_call_arg_registers);
        }
    }
}

void add_function_call_arg_moves(Function *function) {
    add_function_call_arg_moves_for_preg_class(function, PC_INT);
    add_function_call_arg_moves_for_preg_class(function, PC_SSE);
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
    tac->src1->is_function_param = 1;
    tac->src1->function_param_index = function_param_index;

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
    tac->src1->is_function_param = 1;
    tac->src1->function_param_index = function_param_index;

    return tac;
}

// Convert a stack_index if stack_index_map isn't -1
void remap_stack_index(int *stack_index_remap, Value *v) {
    if (v && v->stack_index >= 2 && !v->has_been_renamed && stack_index_remap[v->stack_index] != -1) {
        v->stack_index = stack_index_remap[v ->stack_index];
        v->has_been_renamed = 1;
    }
}

// Add MOVE instructions for up to the the first 6 parameters of a function.
// They are passed in as registers (rdi, rsi, ...). Either, the moves will go to
// a new physical register, or, when possible, will remain in the original registers.
//
// Move function parameters pushed the stack by the caller into registers.
// They might get spilled, in which case function_param_original_stack_index is used
// rather than allocating more space.
void add_function_param_moves(Function *function, char *identifier) {
    FunctionParamAllocation *fpa = init_function_param_allocaton(identifier);

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

    ir = function->ir;

    // Add a second nop if nothing is there, which is used to insert instructions
    if (!ir->next) {
        ir->next = new_instruction(IR_NOP);
        ir->next->prev = ir;
    }

    ir = function->ir->next;

    // Determine which parameters in registers are used in IR_ADDRESS_OF instructions
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->dst);
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->src1);
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->src2);
    }

    // Add moves for registers
    for (int i = 0; i < function->param_count; i++) {
        if (fpa->locations[i][0].stack_offset != -1) continue;

        Type *type = function->param_types[i];
        int single_register_arg_count = fpa->locations[i][0].int_register == -1 ? fpa->locations[i][0].sse_register : fpa->locations[i][0].int_register;

        if (has_address_of[i]) {
            // Add a move instruction to save the register to the stack
            Tac *tac = make_param_move_to_stack_tac(function, type, single_register_arg_count);
            register_param_stack_indexes[i] = tac->dst->stack_index;
            tac->src1->vreg = ++function->vreg_count;
            insert_instruction(ir, tac, 1);
            if (debug_function_param_mapping) printf("Param %d reg %d -> SI %d\n", i, tac->src1->vreg, tac->dst->stack_index);
        }
        else {
            // Add a move instruction to copy register to another register
            Tac *tac = make_param_move_to_register_tac(function, type, single_register_arg_count);
            register_param_vregs[i] = tac->dst->vreg;
            tac->src1->vreg = ++function->vreg_count;
            insert_instruction(ir, tac, 1);
            if (debug_function_param_mapping) printf("Param %d reg %d -> reg %d\n", i, tac->src1->vreg, tac->dst->vreg);
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
        if (fpa->locations[i][0].stack_offset == -1) continue;

        Type *type = function->param_types[i];

        int stack_index = (fpa->locations[i][0].stack_offset + 16) >> 3;

        // The rightmost arg has stack index 2
        if (i + 2 != stack_index) stack_index_remap[i + 2] = stack_index;

        if (debug_function_param_mapping) printf("Param %d SI %d -> SI %d\n", i, i + 2, stack_index);

        if (!has_address_of[i] && type->type != TYPE_LONG_DOUBLE) {
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
    fpa->locations = malloc(sizeof(FunctionParamLocation *) * MAX_FUNCTION_CALL_ARGS);
    fpa->location_counts = malloc(sizeof(int) * MAX_FUNCTION_CALL_ARGS);

    return fpa;
}

// Using the state of already allocated registers & stack entries in fpa, determine the location for a type and set it in fpl.
static void add_type_to_allocation(FunctionParamAllocation *fpa, FunctionParamLocation *fpl, Type *type, int force_stack) {
    fpl->int_register = -1;
    fpl->sse_register = -1;
    fpl->stack_offset = -1;
    fpl->stack_padding = -1;

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

    if (debug_function_param_allocation && !in_stack) {
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
    FunctionParamLocation **fpl = &(fpa->locations[fpa->arg_count]);
    fpl[0] = malloc(sizeof(FunctionParamLocation));
    fpa->location_counts[fpa->arg_count] = 1;
    add_type_to_allocation(fpa, fpl[0], type, 0);

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

            for (int i = 0; i < scalars->count; i++) {
                StructOrUnionScalar *scalar = scalars->scalars[i];
                int eight_byte = scalar->offset >> 3;

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
            int eight_bytes_count = size / 8;
            FunctionParamLocation *fpl = malloc(sizeof(FunctionParamLocation) * eight_bytes_count);
            fpa->locations[fpa->arg_count] = fpl;
            fpa->location_counts[fpa->arg_count] = eight_bytes_count;

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
                    if (seen_integer[i])
                        add_type_to_allocation(fpa, &(fpl[i]), new_type(TYPE_INT), 0);
                    else
                        add_type_to_allocation(fpa, &(fpl[i]), new_type(TYPE_FLOAT), 0);
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
