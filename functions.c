#include "wcc.h"

static LongSet *allocated_functions;
static List *allocated_function_param_allocatons;

RegisterSet arg_register_set;
RegisterSet function_return_value_register_set;

void init_function_allocations(void) {
    allocated_functions = new_longset();
    allocated_function_param_allocatons = new_list(128);
}

void free_function(Function *function, int remove_from_allocations) {
    free_strmap(function->labels);
    if (function->static_symbols) free_list(function->static_symbols);
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

// In the initial parser IR, argumensts are left to right.
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
    int function_calls_size = make_max_function_call_id(function) + 1;
    FunctionParamAllocation **fpas = wcalloc(function_calls_size, sizeof(FunctionParamAllocation));

    int has_struct_or_union_return_value = -1;

    for (Tac *ir = function->ir; ir; ir = ir->next) {
        if (ir->operation.id == IR_START_CALL) {
            has_struct_or_union_return_value = 0;

            if (!ir->src1) panic("src1 NULL in IR_START_CALL");
            Symbol *symbol = ir->src1->function_call.function_symbol;
            char *symbol_name = symbol ? symbol->global_identifier : "(anonymous)";
            Type *function_type = ir->src1->function_call.function_type;
            if (!function_type) panic("function_type NULL in IR_START_CALL in function %s", symbol ? symbol->global_identifier : "(anonymous)");
            FunctionParamAllocation *fpa = init_function_param_allocaton(symbol_name);
            int function_call_number = ir->src1->int_value;
            fpas[function_call_number] = fpa;


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
            int function_call_number = arg->int_value;

            FunctionParamAllocation *fpa = fpas[function_call_number];
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
            int function_call_number = function_value->int_value;

            FunctionParamAllocation *fpa = fpas[function_call_number];
            if (!fpa) panic("fpa was NULL in an IR_CALL for a function call to %s in function %s", symbol_name, function->identifier);

            function_value->function_call.function_call_fp_register_arg_count = fpa->single_fp_register_arg_count;

            if (has_struct_or_union_return_value == -1) panic("has_struct_or_union_return_value was not set");
            function_value->has_struct_or_union_return_value = has_struct_or_union_return_value;
        }

        else if (ir->operation.id == IR_END_CALL) {
            Symbol *symbol = ir->src1->function_call.function_symbol;
            char *symbol_name = symbol ? symbol->global_identifier : "(anonymous)";

            Value *arg = ir->src1;
            int function_call_number = arg->int_value;

            FunctionParamAllocation *fpa = fpas[function_call_number];
            if (!fpa) panic("fpa was NULL in an IR_END_CALL for a function call to %s in function %s", symbol_name, function->identifier);

            finalize_function_param_allocation(fpa);
            arg->function_call.function_call_arg_stack_padding = fpa->padding;
            arg->function_call.function_call_arg_push_count = (fpa->size + 7) / 8;
        }
    }

    wfree(fpas);
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
int add_arg_move_to_register(Function *function, Tac *ir, Type *type, Value *param, int preg_class, int register_index, RegisterSet *register_set) {
    const int *arg_registers = preg_class == PC_INT ? int_arg_registers : sse_arg_registers;

    Tac *tac = new_instruction(IR_MOVE);

    // dst
    tac->dst = new_value();
    tac->dst->type = dup_type(type);
    tac->dst->vreg = ++function->vreg_count;
    tac->dst->live_range_preg = preg_class == PC_INT ? register_set->int_registers[register_index] : register_set->fp_registers[register_index];

    // src
    tac->src1 = param;
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
void add_function_call_arg_move_for_struct_or_union_on_stack(Function *function, Tac *ir) {
    int size = get_type_size(ir->src2->type);
    int rounded_up_size = (size + 7) & ~7;

    // Allocate stack space
    new_tac_before(ir, IR_ALLOCATE_STACK, 0, new_integral_constant(TYPE_LONG, rounded_up_size), 0, 1);

    // Add an instruction to move the stack pointer to a temporary register
    Value *stack_pointer_temp = make_long_temp_vreg(function);
    new_tac_before(ir, IR_MOVE_STACK_PTR, stack_pointer_temp, 0, 0, 1);

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

