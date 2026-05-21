#include "wcc.h"
#include "aarch64.h"


Set *allocate_return_value_live_ranges(void) {
    return new_set(LIVE_RANGE_PREG_REG_R28);
}

void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type) {} // TODO aarch64

int *make_original_stack_indexes(Function *function) {
    int *result = wcalloc(function->vreg_count + 1, sizeof(int *));
    return result;
} // TODO aarch64

void process_target_functions(Function *function) {} // TODO aarch64

void add_function_call_clobbers(char *ig, int vreg_count, LongSet *livenow, Tac *tac) {} // TODO aarch64

