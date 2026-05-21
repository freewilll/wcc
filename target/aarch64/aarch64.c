#include "wcc.h"
#include "aarch64.h"

// Registers used for function calls
const int int_arg_registers[] = {1};

const int sse_arg_registers[] = {1};

char is_32bit_to_aarch64_size(int is_32bit) {
    return is_32bit ? 'w' : 'x';
}

char *target_op_name(int operation) {
    return "TODO: target_op_name";
} // TODO aarch64

void print_target_instruction(void *f, Tac *tac) {
    fprintf(f, "TODO: print_target_instruction");
} // TODO aarch64

void print_backend_instruction(void *f, Tac *tac) {
    fprintf(f, "TODO: print_backend_instruction");
} // TODO aarch64

void print_physical_register_name_for_lr_reg_index(int preg_reg_index) {
    printf("TODO: print_physical_register_name_for_lr_reg_index");
} // TODO aarch64

void perform_peephole_optimization(Function *function) {} // TODO aarch64

// Target functions related code
Set *allocate_return_value_live_ranges(void) {
    return new_set(1); // TODO aarch64
}

void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type) {} // TODO aarch64

int *make_original_stack_indexes(Function *function) {
    int *result = wcalloc(function->vreg_count + 1, sizeof(int *));
    return result;
} // TODO aarch64

void process_target_functions(Function *function) {} // TODO aarch64

void add_function_call_clobbers(char *ig, int vreg_count, LongSet *livenow, Tac *tac) {} // TODO aarch64


// Target registers related code
void remove_vreg_self_moves(Function *function) {} // TODO aarch64

void add_spill_code(Function *function) {} // TODO aarch64
