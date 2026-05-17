#include "wcc.h"

// Registers used for function calls
const int int_arg_registers[] = {1};

const int sse_arg_registers[] = {1};

char *target_op_name(int operation) {}
void print_target_instruction(void *f, Tac *tac) {}
void print_backend_instruction(void *f, Tac *tac) {}
void print_physical_register_name_for_lr_reg_index(int preg_reg_index) {}
void perform_peephole_optimization(Function *function) {}

// Target functions related code
Set *allocate_return_value_live_ranges(void) {}
void add_function_param_to_allocation(FunctionParamAllocation *fpa, Type *type) {}
int *make_original_stack_indexes(Function *function) {}
void process_target_functions(Function *function) {}
void add_function_call_clobbers(char *ig, int vreg_count, LongSet *livenow, Tac *tac) {}

// Target instruction rules related code
void define_rules(void) {}

// Target registers related code
void init_allocate_registers(void) {}
void free_allocate_registers(void) {}
void remove_vreg_self_moves(Function *function) {}
void add_spill_code(Function *function) {}

// Codegen
char *register_name(int preg) {}
char *render_x86_operation(Tac *tac, int function_pc, int expect_preg) {}
void make_stack_offsets(Function *function) {}
void add_final_x86_instructions(Function *function) {}
void merge_rsp_func_call_add_subs(Function *function) {}
void output_code(char *input_filename, char *output_filename) {}
