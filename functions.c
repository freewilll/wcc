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

// Convert a stack_index if stack_index_map isn't -1
void remap_stack_index(int *stack_index_remap, Value *v) {
    if (v && v->stack_index >= 2 && !v->has_been_renamed && stack_index_remap[v->stack_index] != -1) {
        v->stack_index = stack_index_remap[v ->stack_index];
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

