#include <string.h>

#include "wcc.h"

// Keeps track of the mapping of stack index (>= 2) to registers.
// The low and high is used for int128.
typedef struct register_param_locations {
    int low;
    int high;
} RegisterParamLocations;

static LongSet *allocated_functions;
static List *allocated_function_param_allocatons;

RegisterSet arg_register_set;
RegisterSet function_return_value_register_set;

void init_function_allocations(void) {
    allocated_functions = new_longset();
    allocated_function_param_allocatons = new_list(128);
}

void free_function(Function *function, int remove_from_allocations) {
    if (function->labels) free_strmap(function->labels);
    if (function->static_symbols) free_list(function->static_symbols);

    for (int i = 1; i <= function->int128_register_mappings_count; i++) {
        SplitVreg *split_vreg = function->int128_register_mappings[i];
        if (split_vreg) wfree(split_vreg);
    }
    wfree(function->int128_register_mappings);

    wfree(function);

    if (remove_from_allocations) longset_delete(allocated_functions, (long) function);
}

// Free remaining functions and other memory
void free_functions(void) {
    longset_foreach(allocated_functions, it) free_function((Function *) longset_iterator_element(&it), 0);
    free_longset(allocated_functions);

    for (int i = 0; i < allocated_function_param_allocatons->length; i++)
        free_function_param_allocaton(allocated_function_param_allocatons->elements[i]);
    free_list(allocated_function_param_allocatons);
}

Function *new_function(char *identifier) {
    Function *function = wcalloc(1, sizeof(Function));
    longset_add(allocated_functions, (long) function);
    function->identifier = identifier;

    return function;
}

// In the initial parser IR, arguments are left to right.
// In the final IR, after arg processing the arguments are right to left.
// This matches the ABIs, where pushed arguments are also right to left.
void reverse_function_call_args_order(Function *function) {
    const int MAX_ARGS = 256;

    typedef struct tac_interval {
        Tac *start;
        Tac *end;
    } TacInterval;

    // Need to count this IR's function_call_count
    int max_function_call_value = make_max_function_call_value(function);

    // First index, function_id, second index arg_id
    TacInterval *function_args;
    function_args = wmalloc(sizeof(TacInterval) * (max_function_call_value + 1) * MAX_ARGS);

    int *arg_counts = wcalloc(max_function_call_value + 1, sizeof(int));
    Tac **calls = wcalloc(max_function_call_value + 1, sizeof(Tac *));
    Tac **call_starts = wcalloc(max_function_call_value + 1, sizeof(Tac *));

    ir = function->ir;

    // Collect function call details in one pass through the IR
    Tac *tac = function->ir;
    while (tac) {
        if (tac->operation.id == IR_START_CALL) {
            int func = tac->src1->int_value;
            if (func > max_function_call_value) panic("func (%d) > max_function_call_value (%d)", func, max_function_call_value);
            TacInterval *args = &(function_args[func * MAX_ARGS]);
            call_starts[func] = tac;
            tac = tac->next;
            args[arg_counts[func]].start = tac;
        }
        else if (tac->operation.id == IR_END_CALL) {
            int func = tac->src1->int_value;
            calls[func] = tac->prev;
            tac = tac->next;
        }
        else if (tac->operation.id == IR_ARG) {
            int func = tac->src1->int_value;
            TacInterval *args = &(function_args[func * MAX_ARGS]);
            args[arg_counts[func]].end = tac;
            tac = tac->next;

            if (tac->operation.id == IR_ARG_STACK_PADDING) {
                args[arg_counts[func]].end = tac;
                tac = tac->next;
            }

            arg_counts[func]++;
            if (tac->operation.id != IR_END_CALL) args[arg_counts[func]].start = tac;
        }
        else
            tac = tac->next;
    }

    // Reverse the args for each function call
    for (int i = 0; i <= max_function_call_value; i++) {
        TacInterval *args = &(function_args[i * MAX_ARGS]);
        int arg_count = arg_counts[i];
        Tac *call = calls[i];
        Tac *call_start = call_starts[i];

        if (arg_count > 1) {
            call_start->next = args[arg_count - 1].start;
            args[arg_count - 1].start->prev = call_start;
            args[0].end->next = call;
            call->prev = args[0].end;

            for (int j = 0; j < arg_count; j++) {
                // Rearrange args backwards from this IR
                // cs -> p0.start -> p0.end -> p1.start -> p1.end -> cs.end
                // cs -> p0.start -> p0.end -> p1.start -> p1.end -> p2.start -> p2.end -> cs.end
                if (j < arg_count - 1) {
                    args[j + 1].end->next = args[j].start;
                    args[j].start->prev = args[j + 1].end;
                }
            }
        }

    }

    wfree(function_args);
    wfree(arg_counts);
    wfree(calls);
    wfree(call_starts);
}

// Initialize the return value FPA for a function, if needed
FunctionParamAllocation *initialize_function_return_value_fpa(Type *function_type) {
    if (function_type->target->type == TYPE_STRUCT_OR_UNION) {
        FunctionParamAllocation *fpa = init_function_param_allocaton("function rv");
        add_function_param_to_allocation(fpa, function_type->target);
        function_type->function->return_value_fpa = fpa;
        return fpa;
    }

    return NULL;
}

// Prepare register/stack allocation for args in function calls
void process_function_call_arg_allocations(Function *function) {
    FunctionParamAllocation *fpa = NULL;

    int has_struct_or_union_return_value = -1;

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation.id == IR_START_CALL) {
            has_struct_or_union_return_value = 0;

            if (!ir->src1) panic("src1 NULL in IR_START_CALL");
            Symbol *symbol = ir->src1->function_call.function_symbol;
            char *symbol_name = symbol ? symbol->global_identifier : "(anonymous)";
            Type *function_type = ir->src1->function_call.function_type;
            if (!function_type) panic("function_type NULL in IR_START_CALL in function %s", symbol ? symbol->global_identifier : "(anonymous)");
            fpa = init_function_param_allocaton(symbol_name);

            if (function_type->target->type == TYPE_STRUCT_OR_UNION) {
                FunctionParamAllocation *rv_fpa = initialize_function_return_value_fpa(function_type);
                FunctionParamLocations *rv_fpl = rv_fpa->param_locations->elements[0];
                if (rv_fpl->locations[0].stack_offset != -1) {
                    // Allocate an integer slot if the function returns a slot in memory. The
                    // RDI register must contain a pointer to the return value, set by the caller.
                    // Allocate the RDI register which has the pointer to the struct, passed in by the caller
                    add_function_param_to_allocation(fpa, make_pointer_to_void());
                    has_struct_or_union_return_value = 1;
                }
            }
        }
        else if (ir->operation.id == IR_ARG) {
            Symbol *symbol = ir->src1->function_call.function_symbol;
            char *symbol_name = symbol ? symbol->global_identifier : "(anonymous)";

            Value *arg = ir->src1;

            if (!fpa) panic("fpa was NULL in an IR_ARG for a function call to %s in function %s", symbol_name, function->identifier);

            if (has_struct_or_union_return_value == -1) panic("has_struct_or_union_return_value was not set");

            int fpa_arg_count = fpa->param_locations->length;
            int arg_count = fpa_arg_count - has_struct_or_union_return_value;
            arg->function_call.function_call_arg_index = arg_count;

            add_function_param_to_allocation(fpa, ir->src2->type);
            FunctionParamLocations *fpl = fpa->param_locations->elements[fpa_arg_count];
            arg->function_call.function_call_arg_locations = fpl;
            if (fpl->locations[0].stack_padding >= 8) new_tac_after(ir, IR_ARG_STACK_PADDING, 0, 0, 0);
        }
        else if (ir->operation.id == IR_CALL) {
            Symbol *symbol = ir->src1->function_call.function_symbol;
            char *symbol_name = symbol ? symbol->global_identifier : "(anonymous)";

            Value *function_value = ir->src1;

            if (!fpa) panic("fpa was NULL in an IR_CALL for a function call to %s in function %s", symbol_name, function->identifier);

            function_value->function_call.function_call_fp_register_arg_count = fpa->single_fp_register_arg_count;

            if (has_struct_or_union_return_value == -1) panic("has_struct_or_union_return_value was not set");
            function_value->has_struct_or_union_return_value = has_struct_or_union_return_value;
        }

        else if (ir->operation.id == IR_END_CALL) {
            Symbol *symbol = ir->src1->function_call.function_symbol;
            char *symbol_name = symbol ? symbol->global_identifier : "(anonymous)";

            Value *arg = ir->src1;

            if (!fpa) panic("fpa was NULL in an IR_END_CALL for a function call to %s in function %s", symbol_name, function->identifier);

            finalize_function_param_allocation(fpa);
            arg->function_call.function_call_arg_stack_padding = fpa->padding;
            arg->function_call.function_call_arg_push_count = (fpa->size + 7) / 8;
        }
    }
}

// Add IR_CALL_ARG_REG instructions that don't do anything, but ensure
// that the interference graph and register selection code create
// a live range for the interval between the function register assignment and
// the function call. Without this, there is a chance that function call
// registers are used as temporaries during the code instructions
// emitted above.
void add_ir_call_reg_instructions(Tac *ir, Value **function_call_values, int count) {
    for (int i = 0; i < count; i++)
        if (function_call_values[i])
            new_tac_before(ir, IR_CALL_ARG_REG, 0, function_call_values[i], 0, 1);
}

// Add a IR_MOVE instruction from a value to a function call register
// function_call_*_register_arg_index ensures that dst will become the actual
// x86_64 physical register rdi, rsi, etc
int add_arg_move_to_register(Function *function, Tac *ir, Type *type, Value *arg, int preg_class, int register_index, RegisterSet *register_set) {
    const int *arg_registers = preg_class == PC_INT ? int_arg_registers : fp_arg_registers;

    Tac *tac = new_instruction(IR_MOVE);

    // dst
    tac->dst = new_value();
    tac->dst->type = dup_type(type);
    tac->dst->vreg = ++function->vreg_count;
    tac->dst->live_range_preg = preg_class == PC_INT ? register_set->int_registers[register_index] : register_set->fp_registers[register_index];

    // src
    tac->src1 = arg;
    tac->src1->preferred_live_range_preg_index = arg_registers[register_index];

    if (debug_function_arg_mapping) printf("Adding arg move from register for preg-class=%d register_index=%d\n", preg_class, register_index);

    insert_tac_before(ir, tac, 1);

    return tac->dst->vreg;
}

// Load a scalar in a struct into a register. The scalar can be either a local, global, or lvalue in register
// If it's an lvalue in a register, it is indirected, otherwise moved.
void load_struct_scalar_into_value(Function *function, Tac *ir, Value *param, FunctionParamLocation *pl, Type *type, Value *dst, int offset) {
    int lvalue_in_register = param->is_lvalue && param->vreg;
    Value *src1 = dup_value(param);
    src1->type = type;
    src1->offset += pl->stru_offset + offset;

    new_tac_before(ir, lvalue_in_register ? IR_INDIRECT : IR_MOVE, dst, src1, 0, 1);
}

// Load a scalar in a struct into a register. The scalar can be either a local, global, or lvalue in register
// If it's an lvalue in a register, it is indirected, otherwise moved.
Value *load_struct_scalar_into_new_vreg(Function *function, Tac *ir, Value *param, FunctionParamLocation *pl, Type *type) {
    Value *temp = new_value();
    temp->type = type;
    temp->vreg = ++function->vreg_count;

    load_struct_scalar_into_value(function, ir, param, pl, temp->type, temp, 0);

    return temp;
}

Value *make_long_temp_vreg(Function *function) {
    Value *result = new_value();

    result->type = new_type(TYPE_LONG);
    result->type->is_unsigned = 1;
    result->vreg = ++function->vreg_count;

    return result;
}

// Add instructions to copy a struct to the stack
static void add_function_call_arg_move_for_struct_or_union_to_stack(Function *function, Tac *ir) {
    int size = get_type_size(ir->src2->type);
    int rounded_up_size = (size + 7) & ~7;

    // Allocate stack space
    new_tac_before(ir, IR_ALLOCATE_STACK, 0, new_integral_constant(TYPE_LONG, rounded_up_size), 0, 1);

    // Add an instruction to move the stack pointer to a temporary register
    Value *stack_pointer_temp = make_long_temp_vreg(function);
    new_tac_before(ir, IR_MOVE_STACK_PTR, stack_pointer_temp, 0, 0, 1);

    // Prepare destination, which must be a *void
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

// Add instructions to copy an int128 from a register/stack to the stack
static void add_function_call_arg_move_for_int128_to_stack(Function *function, Tac *ir) {
    if (ir->src2->stack_index)
        panic("Got unexpected stack index %d in add_function_call_arg_move_for_int128_to_stack", ir->src2->stack_index);

    SplitVreg *split_vreg = function->int128_register_mappings[ir->src2->vreg];
    if (!split_vreg) panic("NULL pointer when fetching split vreg for int128 for vreg %d", ir->src2->vreg);

    if (debug_function_arg_mapping)
        printf("Adding copy from vregs for int128 vreg=%d, split %d / %d\n", ir->src2->vreg, split_vreg->low, split_vreg->high);

    Value *src2_low = dup_value(ir->src2);
    src2_low->type->type =TYPE_LONG;
    src2_low->vreg = split_vreg->low;

    Value *src2_high = dup_value(ir->src2);
    src2_high->type->type =TYPE_LONG;
    src2_high->vreg = split_vreg->high;

    new_tac_before(ir, IR_ARG, 0, ir->src1, src2_high, 1);
    new_tac_before(ir, IR_ARG, 0, ir->src1, src2_low, 1);
}

// Lookup corresponding location for preg_class/register
static FunctionParamLocation *lookup_location(int preg_class, int register_index, FunctionParamLocations *pl) {
    // For the location that matches register_index
    for (int loc = 0; loc < pl->count; loc++) {
        FunctionParamLocation *location = &(pl->locations[loc]);
        int function_call_register_arg_index = preg_class == PC_INT
            ? location->int_register
            : location->fp_register;

        if (function_call_register_arg_index == register_index) return location;
    }

    panic("Unhandled struct/union arg move into a register");
}

// For a int128 in a vreg, lookup the split low/high vregs and add arg moves for its 8-byte parts.
static Value *make_function_call_arg_value_for_int128(Function *function, FunctionParamLocations **pls, Value *arg, int index, int preg_class) {
    int vreg = arg->vreg;
    if (!vreg) panic("Arg moves for int128 not implemented unless the int128 is in a vreg");

    SplitVreg *split_vreg = function->int128_register_mappings[vreg];
    if (!split_vreg) panic("NULL pointer when fetching split vreg for int128 for vreg %d", vreg);

   FunctionParamLocation *location = lookup_location(preg_class, index, *pls);

   Type *long_type = dup_type(arg->type);
   long_type->type = TYPE_LONG;

   Value *value = new_value();
   value->type = long_type;
   value->vreg = location->i128_part ? split_vreg->high : split_vreg->low;

   return value;
}

// Insert IR_MOVE instructions before IR_ARG instructions for
// - the first 6 single-register args.
// - the first 8 floating point args.
// The dst of the move will be constrained so that rdi, rsi, xmm0, xmm1 etc are allocated to it.
static void add_function_call_arg_moves_for_preg_class(Function *function, int preg_class) {
    int function_calls_size = make_max_function_call_id(function) + 1;
    int register_count = preg_class == PC_INT ? 6 : 8; // TODO aarch64 unhardcode this

    // Values of the passed argument, i.e. by the caller
    int allocated_count = function_calls_size * register_count;
    Value **arg_values = wcalloc(allocated_count, sizeof(Value *));

    // param_indexes maps the register indexes to a parameter index, e.g.
    // foo(int i, long double ld, int j) will produce
    // param_indexes[0] = 0
    // param_indexes[1] = 2
    int *param_indexes = wmalloc(sizeof(int) * function_calls_size * register_count);
    memset(param_indexes, -1, sizeof(int) * function_calls_size * register_count);

    FunctionParamLocations **param_locations = wmalloc(sizeof(FunctionParamLocations *) * function_calls_size * register_count);
    memset(param_locations, -1, sizeof(FunctionParamLocations *) * function_calls_size * register_count);

    make_vreg_count(function, 0);

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation.id == IR_ARG) {
            FunctionParamLocations *pl = ir->src1->function_call.function_call_arg_locations;

            for (int loc = 0; loc < pl->count; loc++) {
                int function_call_register_arg_index = preg_class == PC_INT
                    ? pl->locations[loc].int_register
                    : pl->locations[loc].fp_register;

                if (function_call_register_arg_index >= 0) {
                    int i = ir->src1->int_value * register_count + function_call_register_arg_index;
                    if (i >= allocated_count) panic("Exceeding arg_values space, want=%d, allocated=%d", i, allocated_count);
                    arg_values[i] = ir->src2;
                    param_indexes[i] = ir->src1->function_call.function_call_arg_index;
                    param_locations[i] = pl;
                }
            }
        }

        if (ir->operation.id == IR_CALL) {
            // Rewind the ir so that it moves onto the last IR_CALL_ARG_REG operation, if any are present from a previous pass.
            // This can happen when processing floating point args after integer args have already been processed.
            // This results in the folowing sequence:
            // r58_LRpreg1:int = ...
            // r59_LRpreg24:float = ...
            // call reg arg r58:int
            // call reg arg r59:double
            // call "foo"
            Tac *moves_ir = ir;
            while (moves_ir->prev->operation.id == IR_CALL_ARG_REG) moves_ir = moves_ir->prev;

            Value **call_arg = &(arg_values[ir->src1->int_value * register_count]);
            if (ir->src1->int_value >= function_calls_size) panic("Exceeding param_locations space, want=%d, allocated=%d", ir->src1->int_value, function_calls_size);
            int *param_index = &(param_indexes[ir->src1->int_value * register_count]);
            FunctionParamLocations **pls = &(param_locations[ir->src1->int_value * register_count]);
            Type *called_function_type = ir->src1->type;
            int has_struct_or_union_return_value = ir->src1->has_struct_or_union_return_value;

            // Allocated registers that hold the argument value
            Value **function_call_values = wcalloc(register_count, sizeof(Value *));

            // Add the moves backwards so that arg 0 is last.
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
                // Bail if we're doing integers and the first arg is reserved for a struct/union
                // return value.
                if (has_struct_or_union_return_value && preg_class == PC_INT && i == 0) break; // TODO aarch64 use something like prepend_function_params

                Type *type;
                int pi = *param_index;
                if (pi >= 0 && pi < called_function_type->function->param_count) {
                    type = called_function_type->function->param_types->elements[pi];
                }
                else
                    type = apply_default_function_call_argument_promotions((*call_arg)->type);

                int function_call_vreg;
                Type *function_call_vreg_type;

                if (type->type == TYPE_INT128) {
                    Value *v = make_function_call_arg_value_for_int128(function, pls, *call_arg, i, preg_class);
                    function_call_vreg = add_arg_move_to_register(function, moves_ir, v->type, v, preg_class, i, &arg_register_set);
                    function_call_vreg_type = v->type;
                }
                else if (type->type == TYPE_STRUCT_OR_UNION) {
                    FunctionParamLocation *location = lookup_location(preg_class, i, *pls);
                    function_call_vreg = make_struct_or_union_arg_move_instructions(function, moves_ir, *call_arg, preg_class, i, location, &arg_register_set);
                    function_call_vreg_type = preg_class == PC_INT ? new_type(TYPE_LONG) : new_type(TYPE_DOUBLE);
                }
                else {
                    if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
                    if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);
                    function_call_vreg = add_arg_move_to_register(function, moves_ir, type, *call_arg, preg_class, i, &arg_register_set);
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

            wfree(function_call_values);
        }
    }

    wfree(arg_values);
    wfree(param_indexes);
    wfree(param_locations);
}

// Process IR_ARG insructions. They are either loaded into a register, pushed onto the
// stack with an IR_ARG, or, in the case of a struct/union pushed onto the stack with
// generated code.
void add_function_call_arg_moves(Function *function) {
    add_function_call_arg_moves_for_preg_class(function, PC_INT);
    add_function_call_arg_moves_for_preg_class(function, PC_FP);

    remove_IR_ARG_instructions_that_have_been_handled(function);

    // Add memory copies for struct and unions
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation.id == IR_ARG) {
            if (ir->src2->type->type == TYPE_INT128) {
                FunctionParamLocations *pls = ir->src1->function_call.function_call_arg_locations;
                if (pls->count != 1) panic("Unexpected int128 to stack move with locations->count != 1");
                add_function_call_arg_move_for_int128_to_stack(function, ir);
                make_instruction_a_nop(ir);
            }
            else if (ir->src2->type->type == TYPE_STRUCT_OR_UNION) {
                FunctionParamLocations *pls = ir->src1->function_call.function_call_arg_locations;
                if (pls->count != 1) panic("Unexpected struct/union to stack move with locations->count != 1");
                add_function_call_arg_move_for_struct_or_union_to_stack(function, ir);
                make_instruction_a_nop(ir);
            }
        }
    }

    // Process any added memcpy calls due to struct and union copies
    add_function_call_arg_moves_for_preg_class(function, PC_INT);
    remove_IR_ARG_instructions_that_have_been_handled(function);

    if (debug_function_arg_mapping) {
        printf("After function call arg mapping\n");
        print_ir(function, 0);
    }
}

// Nuke all IR_ARG instructions that have had code added that moves the value into a register
void remove_IR_ARG_instructions_that_have_been_handled(Function *function) {
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation.id == IR_ARG) {
            FunctionParamLocations *pl = ir->src1->function_call.function_call_arg_locations;

            for (int loc = 0; loc < pl->count; loc++) {
                if (pl->locations[loc].int_register != -1 || pl->locations[loc].fp_register != -1) {
                    ir->operation.id = IR_NOP;
                    ir->dst = 0;
                    ir->src1 = 0;
                    ir->src2 = 0;
                    break;
                }
            }
        }
    }
}

// Recurse through a type and make list of all scalars + their offsets
void flatten_type(Type *type, StructOrUnionScalars *scalars, int offset) {
    if (type->type == TYPE_STRUCT_OR_UNION) {
        StructOrUnion *s = type->struct_or_union_desc;
        for (StructOrUnionMember **pmember = s->members; *pmember; pmember++) {
            StructOrUnionMember *member = *pmember;
            flatten_type(member->type, scalars, offset + member->offset);
        }
    }

    else if (type->type == TYPE_ARRAY) {
        int element_size = get_type_size(type->target);
        for (int i = 0; i < type->array_length; i++)
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

// Convert a stack_index if stack_index_map isn't -1
void remap_stack_index(int *stack_index_remap, Value *v) {
    if (v && v->stack_index >= 2 && !v->has_been_renamed && stack_index_remap[v->stack_index] != -1) {
        v->stack_index = stack_index_remap[v->stack_index];
        v->has_been_renamed = 1;
    }
}

Value *make_function_call_value(int function_call, Type *type) {
    Value *src1 = new_value();

    src1->int_value = function_call;
    src1->is_constant = 1;
    src1->type = new_type(TYPE_LONG);
    src1->function_call.function_type = type;

    return src1;
}

// Add an instruction to move a parameter in a register/stack to another register
Tac *make_param_move_to_register_tac(Function *function, Type *type, int single_register_arg_count, int in_register) {
    Tac *tac = new_instruction(IR_MOVE);
    tac->dst = new_value();
    tac->dst->type = dup_type(type);
    tac->dst->vreg = ++function->vreg_count;

    tac->src1 = new_value();
    tac->src1->type = dup_type(tac->dst->type);

    if (in_register)
        tac->src1->live_range_preg = get_preg_class_for_scalar_type(type) == PC_FP ? fp_arg_registers[single_register_arg_count] : int_arg_registers[single_register_arg_count];

    return tac;
}

// Add an instruction to move a parameter in a register to a new stack entry
Tac *make_param_move_to_stack_tac(Function *function, Type *type, int single_register_arg_count) {
    Tac *tac = new_instruction(IR_MOVE);
    tac->dst = new_value();
    tac->dst->type = dup_type(type);
    tac->dst->stack_index = -(++function->stack_register_count);

    tac->src1 = new_value();
    tac->src1->type = dup_type(tac->dst->type);

    tac->src1->live_range_preg = get_preg_class_for_scalar_type(type) == PC_FP ? fp_arg_registers[single_register_arg_count] : int_arg_registers[single_register_arg_count];

    return tac;
}

// Set has_address_of to 1 if a value is a parameter in a register and it's used in a & instruction
static void check_param_value_has_used_in_an_address_of(int *has_address_of, Tac *tac, Value *v) {
    if (!v) return;
    if (tac->operation.id != IR_ADDRESS_OF) return;
    if (v->stack_index < 2) return;
    has_address_of[v->stack_index - 2] = 1;
    return;
}

// Convert stack_index in value v to a parameter register
static void convert_register_param_stack_index_to_register(Function *function, RegisterParamLocations *register_param_vregs, Value *v) {
    if (!v || v->stack_index < 2) return;

    if (register_param_vregs[v->stack_index  - 2].low != -1 && v->offset == 0) {
        assign_register_to_value(v, register_param_vregs[v->stack_index  - 2].low);
    }
    else if (register_param_vregs[v->stack_index  - 2].high != -1 && v->offset == 8) {
        assign_register_to_value(v, register_param_vregs[v->stack_index  - 2].high);
        v->offset = 0;
    }
}

// Convert stack_index in value v to a parameter in the stack
static void convert_register_param_stack_index_to_stack(Function *function, int *register_param_stack_indexes, Value *v) {
    if (v && v->stack_index >= 2 && register_param_stack_indexes[v->stack_index  - 2]) {
        v->stack_index = register_param_stack_indexes[v->stack_index  - 2];
        v->is_lvalue = 0;
    }
}

// Convert a value that has a stack index >= 2, i.e. it's a pushed parameter into a vreg
static void convert_pushed_param_stack_index_to_register(Function *function, RegisterParamLocations *stack_param_vregs, Value *v) {
    if (!v || v->function_call.function_param_original_stack_index || v->stack_index< 2) return;

    if (stack_param_vregs[v->stack_index - 2].low != -1  && v->offset == 0) {
        assign_register_to_value(v, stack_param_vregs[v->stack_index - 2].low);
    }
    else if (stack_param_vregs[v->stack_index - 2].high != -1  && v->offset == 8) {
        assign_register_to_value(v, stack_param_vregs[v->stack_index - 2].high);
        v->offset = 0;
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
void add_function_param_moves(Function *function) {
    if (debug_function_param_mapping) {
        printf("Mapping function parameters for %s\n", function->identifier);
        print_ir(function, 0);
    }

    ir = function->ir;

    // Add a nop if nothing is there, which is used to insert instructions
    if (!ir->next) {
        ir->next = new_instruction(IR_NOP);
        ir->next->prev = ir;
    }

    ir = function->ir->next;

    // Make FPA for the function
    FunctionParamAllocation *fpa = init_function_param_allocaton(function->identifier);
    function->fpa = fpa;

    // fpa_start is the index in fpa->param_locations that has the first actual parameter
    int fpa_start = prepend_function_params(function);

    for (int i = 0; i < function->type->function->param_count; i++)
        add_function_param_to_allocation(fpa, function->type->function->param_types->elements[i]);

    finalize_function_param_allocation(fpa);

    RegisterParamLocations *register_param_vregs = wmalloc(sizeof(RegisterParamLocations) * function->type->function->param_count);
    memset(register_param_vregs, -1, sizeof(RegisterParamLocations) * function->type->function->param_count);

    int *register_param_stack_indexes = wcalloc(function->type->function->param_count, sizeof(int));

    int *has_address_of = wcalloc(function->type->function->param_count, sizeof(int));

    // The stack is never bigger than function->type->function->param_count * 2 & starts at 2
    RegisterParamLocations *stack_param_vregs = wmalloc(sizeof(RegisterParamLocations) * (function->type->function->param_count * 2 + 2));
    memset(stack_param_vregs, -1, sizeof(RegisterParamLocations) * (function->type->function->param_count * 2 + 2));

    // Determine which parameters in registers are used in IR_ADDRESS_OF instructions
    for (Tac *ir = function->ir; ir; ir = ir->next) {
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->dst);
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->src1);
        check_param_value_has_used_in_an_address_of(has_address_of, ir, ir->src2);
    }

    // Add moves for params in registers
    for (int i = 0; i < function->type->function->param_count; i++) {
        FunctionParamLocations fpl = fpa_pl(fpa, fpa_start + i);
        if (fpl.locations[0].stack_offset != -1) continue;

        Type *type = function->type->function->param_types->elements[i];

        if (type->type == TYPE_INT128)  {
            // int128 value

            if (has_address_of[i]) {
                // Add move instructions to save the registers to the stack

                int low_arg_register = fpl.locations[0].int_register;
                int high_arg_register = fpl.locations[1].int_register;

                Type *long_type = dup_type(type);
                long_type->type = TYPE_LONG;

                Tac *tac_high = make_param_move_to_stack_tac(function, long_type, high_arg_register);
                tac_high->src1->vreg = ++function->vreg_count;
                tac_high->src1->type->type = TYPE_LONG;
                insert_tac_before(ir, tac_high, 0);

                Tac *tac_low = make_param_move_to_stack_tac(function, long_type, low_arg_register);
                register_param_stack_indexes[i] = tac_low->dst->stack_index;
                tac_low->src1->vreg = ++function->vreg_count;
                tac_low->src1->type->type = TYPE_LONG;
                insert_tac_before(ir, tac_low, 0);

                if (debug_function_param_mapping)
                    printf("Param %d, reg param reg %d / %d -> local SI %d / %d\n",
                        i, tac_low->src1->vreg, tac_high->src1->vreg, tac_low->dst->stack_index, tac_high->dst->stack_index);
            }
            else {
                int low_arg_register = fpl.locations[0].int_register;
                int high_arg_register = fpl.locations[1].int_register;

                Type *long_type = dup_type(type);
                long_type->type = TYPE_LONG;

                // Add a move instruction to copy register to another register
                Tac *tac_low = make_param_move_to_register_tac(function, long_type, low_arg_register, 1);
                register_param_vregs[i].low = tac_low->dst->vreg;
                tac_low->src1->vreg = ++function->vreg_count;
                insert_tac_before(ir, tac_low, 0);

                Tac *tac_high = make_param_move_to_register_tac(function, long_type, high_arg_register, 1);
                register_param_vregs[i].high = tac_high->dst->vreg;
                tac_high->src1->vreg = ++function->vreg_count;
                insert_tac_before(ir, tac_high, 0);

                if (debug_function_param_mapping)
                    printf("Param %d reg param reg %d / %d -> local reg %d / %d\n", i,
                        tac_low->src1->vreg, tac_high->src1->vreg,
                        tac_low->dst->vreg, tac_high->dst->vreg);
            }
        }

        else if (type->type == TYPE_STRUCT_OR_UNION)  {
            int stack_index = add_struct_or_union_param_move(function, ir, type, fpa->param_locations->elements[fpa_start + i], &arg_register_set);
            register_param_stack_indexes[i] = stack_index;
        }

        else {
            // Scalar value

            int single_register_arg_count = fpl.locations[0].int_register == -1
                ? fpl.locations[0].fp_register
                : fpl.locations[0].int_register;

            if (type->type == TYPE_ARRAY) type = decay_array_to_pointer(type);
            if (type->type == TYPE_ENUM) type = new_type(TYPE_INT);

            if (has_address_of[i]) {
                // Add a move instruction to save the register to the stack
                Tac *tac = make_param_move_to_stack_tac(function, type, single_register_arg_count);
                register_param_stack_indexes[i] = tac->dst->stack_index;
                tac->src1->vreg = ++function->vreg_count;
                insert_tac_before(ir, tac, 0);
                if (debug_function_param_mapping) printf("Param %d reg param reg %d -> local SI %d\n", i, tac->src1->vreg, tac->dst->stack_index);
            }
            else {
                // Add a move instruction to copy register to another register
                Tac *tac = make_param_move_to_register_tac(function, type, single_register_arg_count, 1);
                register_param_vregs[i].low = tac->dst->vreg;
                tac->src1->vreg = ++function->vreg_count;
                insert_tac_before(ir, tac, 0);
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

    // Add moves for params in the stack.
    // Parameter stack indexes go from 2, 3, 4 for arg 0, arg 1, arg 2, ...
    // Determine the actual stack index based on type sizes and alignment and
    // remap stack_index.
    int *stack_index_remap = wmalloc(sizeof(int) * (function->type->function->param_count + 2));
    memset(stack_index_remap, -1, sizeof(int) * (function->type->function->param_count + 2));

    // Determine stack offsets for parameters on the stack and add moves
    for (int i = 0; i < function->type->function->param_count; i++) {
        FunctionParamLocations fpl = fpa_pl(fpa, fpa_start + i);
        if (fpl.locations[0].stack_offset == -1) continue;

        Type *type = function->type->function->param_types->elements[i];

        int stack_index = (fpa_pl(fpa, fpa_start + i).locations[0].stack_offset + 16) >> 3;

        // The rightmost arg has stack index 2
        if (i + 2 != stack_index) stack_index_remap[i + 2] = stack_index;

        if (debug_function_param_mapping) printf("Param %d SI %d -> SI %d\n", i, i + 2, stack_index);

        if (!has_address_of[i] && type->type == TYPE_INT128) {
            Type *long_type = dup_type(type);
            long_type->type = TYPE_LONG;

            Tac *tac_low = make_param_move_to_register_tac(function, long_type, i, 0);
            stack_param_vregs[stack_index - 2].low = tac_low->dst->vreg;
            tac_low->src1->function_call.function_param_original_stack_index = stack_index;
            tac_low->src1->stack_index = stack_index;
            tac_low->src1->has_been_renamed = 1; // Stop remap_stack_index() from changing the stack index again
            insert_tac_before(ir, tac_low, 0);

            Tac *tac_high = make_param_move_to_register_tac(function, long_type, i, 0);
            stack_param_vregs[stack_index - 2].high = tac_high->dst->vreg;
            tac_high->src1->function_call.function_param_original_stack_index = stack_index + 1;
            tac_high->src1->stack_index = stack_index;
            tac_high->src1->offset = 8;
            tac_high->src1->has_been_renamed = 1; // Stop remap_stack_index() from changing the stack index again
            insert_tac_before(ir, tac_high, 0);
        }

        else if (!has_address_of[i] && (!long_doubles_are_in_the_stack || type->type != TYPE_LONG_DOUBLE) && type->type != TYPE_STRUCT_OR_UNION) {
            Tac *tac = make_param_move_to_register_tac(function, type, i, 0);
            stack_param_vregs[stack_index - 2].low = tac->dst->vreg;
            tac->src1->function_call.function_param_original_stack_index = stack_index;
            tac->src1->stack_index = stack_index;
            tac->src1->has_been_renamed = 1; // Stop remap_stack_index() from changing the stack index again
            insert_tac_before(ir, tac, 0);
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

    wfree(register_param_vregs);
    wfree(register_param_stack_indexes);
    wfree(has_address_of);
    wfree(stack_param_vregs);
    wfree(stack_index_remap);

    if (debug_function_param_mapping) {
        printf("After function param mapping\n");
        print_ir(function, 0);
    }
}

// Initialize data structures for the function param & arg allocation processor
FunctionParamAllocation *init_function_param_allocaton(char *function_identifier) {
    if (debug_function_param_allocation) printf("\nInitializing param allocation for function %s\n", function_identifier);

    FunctionParamAllocation *fpa = wcalloc(1, sizeof(FunctionParamAllocation));
    append_to_list(allocated_function_param_allocatons, fpa);
    fpa->param_locations = new_list(8);

    return fpa;
}

void free_function_param_locations(FunctionParamLocations *fpl) {
    wfree(fpl->locations);
    wfree(fpl);
}

void free_function_param_allocaton(FunctionParamAllocation *fpa) {
    List *fpls_list = fpa->param_locations;
    for (int i = 0; i < fpls_list->length; i++)
        free_function_param_locations(fpls_list->elements[i]);

    free_list(fpa->param_locations);
    wfree(fpa);
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

