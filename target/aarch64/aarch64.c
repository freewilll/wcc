#include "wcc.h"
#include "aarch64.h"

// Registers used for function calls
const int int_arg_registers[] = {1};

const int fp_arg_registers[] = {1};

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

int get_preg_class_for_scalar_type(Type *type) {
    return (type->type >= TYPE_FLOAT && type->type <= TYPE_LONG_DOUBLE) ? PC_FP : PC_INT;
}

static void remove_self_register_copies(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next)
        if (tac->dst && tac->dst->preg != -1 && tac->src1 && tac->src1->preg != -1 && tac->dst->preg == tac->src1->preg)
            if (tac->operation.id == AARCH64_OP_MOV) tac->operation.id = IR_NOP;
}

void perform_peephole_optimization(Function *function) {
    // remove_stack_self_moves(function); // TODO aarch64
    remove_self_register_copies(function);
}

// This removes instructions that copy a register to itself by replacing them with noops.
void remove_vreg_self_moves(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation.id == AARCH64_OP_MOV && tac->dst && tac->dst->vreg && tac->src1 && tac->src1->vreg && tac->dst->vreg == tac->src1->vreg) {
            tac->operation.id = IR_NOP;
            tac->dst = 0;
            tac->src1 = 0;
            tac->src2 = 0;
            tac->target_template = 0;
        }
    }
}

void add_spill_code(Function *function) {} // TODO aarch64
