#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

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

// Insert IR_MOVE instructions before IR_ARG instructions for the first 6 scalar args.
// The dst of the move will be constrained so that rdi, rsi etc are allocated to it.
void add_function_call_arg_moves(Function *function) {
    int function_call_count = make_function_call_count(function);

    // Values of the passed argument, i.e. by the caller
    Value **arg_values = malloc(sizeof(Value *) * function_call_count * 6);
    memset(arg_values, 0, sizeof(Value *) * function_call_count * 6);

    // param_indexes maps the register indexes to a parameter index, e.g.
    // foo(int i, long double ld, int j) will produce
    // param_indexes[0] = 0
    // param_indexes[1] = 2
    int *param_indexes = malloc(sizeof(int) * function_call_count * 6);
    memset(param_indexes, -1, sizeof(int) * function_call_count * 6);

    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation == IR_ARG && ir->src1->function_call_register_arg_index >= 0) {
            arg_values[ir->src1->int_value * 6 + ir->src1->function_call_register_arg_index] = ir->src2;
            param_indexes[ir->src1->int_value * 6 + ir->src1->function_call_register_arg_index] = ir->src1->function_call_arg_index;

            ir->operation = IR_NOP;
            ir->dst = 0;
            ir->src1 = 0;
            ir->src2 = 0;
        }

        if (ir->operation == IR_CALL) {
            Value **call_arg = &(arg_values[ir->src1->int_value * 6]);
            int *param_index = &(param_indexes[ir->src1->int_value * 6]);
            Function *called_function = ir->src1->function_symbol->function;

            // Allocated registers
            int *function_call_arg_registers = malloc(sizeof(int) * 6);
            memset(function_call_arg_registers, 0, sizeof(int) * 6);
            Type **function_call_arg_types =  malloc(sizeof(Type *) * 6);

            // Add the moves backwards so that arg 0 (rsi) is last.
            int i = 0;
            while (i < 6 && *call_arg) {
                call_arg++;
                param_index++;
                i++;
            }

            call_arg--;
            param_index--;
            i--;

            for (; i >= 0; i--) {
                Tac *tac = new_instruction(IR_MOVE);
                tac->dst = new_value();

                int pi = *param_index;
                if (pi >=0 && pi < called_function->param_count) {
                    Type *type = called_function->param_types[pi];
                    tac->dst->type = dup_type(type);
                }
                else {
                    // Apply default argument promotions
                    if ((*call_arg)->type->type < TYPE_INT) {
                        tac->dst->type = new_type(TYPE_INT);
                        if ((*call_arg)->type->is_unsigned) tac->dst->type->is_unsigned = 1;
                    }
                    else
                        tac->dst->type = dup_type((*call_arg)->type);
                }

                tac->dst->vreg = ++function->vreg_count;

                tac->src1 = *call_arg;
                tac->src1->preferred_live_range_preg_index = arg_registers[i];
                tac->dst->is_function_call_arg = 1;
                tac->dst->function_call_register_arg_index = i;
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
            for (int i = 0; i < 6; i++) {
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

static void assign_register_param_value_to_register(Value *v, int vreg) {
    v->stack_index = 0;
    v->is_lvalue = 0;
    v->vreg = vreg;
}

// Convert stack_indexes to parameter register
// stack-index = 1 + param_count - arg  =>
// arg = param_count - stack-index + 1
// 0 <= arg < 6                         =>
// -1 <= param_count - stack-index < 5
static void convert_register_param_stack_index(Function *function, int *register_param_vregs, Value *v) {
    if (v && v->stack_index >= 2 && register_param_vregs[v->stack_index  - 2] != -1)
        assign_register_param_value_to_register(v, register_param_vregs[v->stack_index  - 2]);
}

// Convert a value that has a stack index >= 2, i.e. it's a pushed parameter into a vreg
static void convert_pushed_param_stack_index(Function *function, int *stack_param_vregs, Value *v) {
    if (v && !v->function_param_original_stack_index && v->stack_index >= 2 && stack_param_vregs[v->stack_index - 2] != -1)
        assign_register_param_value_to_register(v, stack_param_vregs[v->stack_index - 2]);
}

static Tac *make_param_move_tac(Function *function, Type *type, int function_param_index) {
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

// Convert a stack_index if stack_index_map isn't -1
void remap_stack_index(int *stack_index_remap, Value *v) {
    if (v  && v->stack_index >= 2 && !v->has_been_renamed && stack_index_remap[v->stack_index] != -1) {
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
void add_function_param_moves(Function *function) {
    int *pushes = malloc(sizeof(int) * (function->param_count));
    memset(pushes, -1, sizeof(int) * (function->param_count));

    int *register_param_vregs = malloc(sizeof(int) * function->param_count);
    memset(register_param_vregs, -1, sizeof(int) * function->param_count);

    int *stack_param_vregs = malloc(sizeof(int) * function->param_count);
    memset(stack_param_vregs, -1, sizeof(int) * function->param_count);

    ir = function->ir;
    // Add a second nop if nothing is there, which is used to insert instructions
    if (!ir->next) {
        ir->next = new_instruction(IR_NOP);
        ir->next->prev = ir;
    }

    ir = function->ir->next;

    int register_param_count = function->param_count;
    if (register_param_count > 6) register_param_count = 6;

    int scalar_arg_count = 0;

    // Determine which parameters on the stack
    for (int i = 0; i < function->param_count; i++) {
        Type *type = function->param_types[i];
        int is_scalar = is_scalar_type(type);
        int is_push = !is_scalar || scalar_arg_count >= 6;
        scalar_arg_count += is_scalar;
        pushes[i] = is_push;
    }

    // Add moves for registers
    scalar_arg_count = 0;
    for (int i = 0; i < function->param_count; i++) {
        if (pushes[i]) continue;

        Type *type = function->param_types[i];
        Tac *tac = make_param_move_tac(function, type, scalar_arg_count);
        register_param_vregs[i] = tac->dst->vreg;
        tac->src1->vreg = ++function->vreg_count;
        insert_instruction(ir, tac, 1);

        scalar_arg_count++;
    }

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        convert_register_param_stack_index(function, register_param_vregs, ir->dst);
        convert_register_param_stack_index(function, register_param_vregs, ir->src1);
        convert_register_param_stack_index(function, register_param_vregs, ir->src2);
    }

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

    int offset = 16; // Includes pushed instruction and stack pointers
    for (int i = 0; i < function->param_count; i++) {
        if (!pushes[i]) continue;

        Type *type = function->param_types[i];

        int alignment = get_type_alignment(type);
        if (alignment < 8) alignment = 8;
        offset = ((offset + alignment  - 1) & (~(alignment - 1)));
        int stack_index = offset >> 3;

        // The rightmost arg has stack index 2
        if (i + 2 != stack_index) stack_index_remap[i + 2] = stack_index;

        if (type->type != TYPE_LONG_DOUBLE) {
            Tac *tac = make_param_move_tac(function, type, i);
            stack_param_vregs[stack_index - 2] = tac->dst->vreg;
            tac->src1->function_param_original_stack_index = stack_index;
            // Add an offset to distinghuish them from parameters in registers
            tac->src1->stack_index = stack_index;
            ir->src1->has_been_renamed = 1;
            insert_instruction(ir, tac, 1);
        }

        int type_size = get_type_size(type);
        if (type_size < 8) type_size = 8;
        offset += type_size;
    }


    for (Tac *ir = function->ir; ir; ir = ir->next) {
        remap_stack_index(stack_index_remap, ir->dst);
        remap_stack_index(stack_index_remap, ir->src1);
        remap_stack_index(stack_index_remap, ir->src2);

        convert_pushed_param_stack_index(function, stack_param_vregs, ir->dst);
        convert_pushed_param_stack_index(function, stack_param_vregs, ir->src1);
        convert_pushed_param_stack_index(function, stack_param_vregs, ir->src2);
    }

    free(register_param_vregs);
    free(stack_param_vregs);
    free(stack_index_remap);
}