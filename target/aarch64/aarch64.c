#include "wcc.h"
#include "aarch64.h"

char is_32bit_to_aarch64_size(int is_32bit) {
    return is_32bit ? 'w' : 'x';
}

char *target_op_name(int operation) {
    panic("TODO aarch64: target_op_name");
}

void print_target_instruction(void *f, Tac *tac) {
    int o = tac->operation.id;

    // TODO aarch64 AARCH64_OP_ARG
    // if (o == AARCH64_OP_ARG) {
    //     fprintf(stderr, "TODO aarch64 AARCH64_OP_ARG");
    // }

         if (o == AARCH64_OP_CALL)      { fprintf(f, "call "  ); print_value(f, tac->src1, 1); if (tac->dst) { printf(" -> "); print_value(f, tac->dst, 1); } }
    else if (o == AARCH64_OP_MOV)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == AARCH64_OP_ADD)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else if (o == AARCH64_OP_MUL)       { fprintf(f, "%-6s", operation_string(o)); print_value(f, tac->src1, 1); fprintf(f, ", "); print_value(f, tac->dst,  1); }
    else
        panic("print_instruction(): Unknown operation: %d", tac->operation.id);
}

void print_backend_instruction(void *f, Tac *tac) {
    panic("TODO aarch64: print_backend_instruction");
}

void print_physical_register_name_for_lr_reg_index(int preg_reg_index) {
    panic("TODO aarch64: print_physical_register_name_for_lr_reg_index");
}

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

// Prepend an instruction that loads the address of tac->dst into a pointer
static Tac *load_dst_address_into_pointer(Function *function, Tac *tac) {
    Value *new_dst = new_value();
    new_dst->type = make_pointer(tac->dst->type);
    new_dst->vreg = ++function->vreg_count;
    return new_tac_before(tac, IR_ADDRESS_OF, new_dst, tac->dst, 0, 1);
}

// Convert stores to global variables to a IR_ADDRESS_OF of the global, followed by a IR_MOVE_TO_PTR
void make_load_store_instructions(Function *function) {
    make_vreg_count(function, live_range_reserved_pregs_offset);

    // TODO aarch64 more to do here

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->operation.id == IR_MOVE && tac->dst && tac->dst->global_symbol) {
            Tac *load_tac = load_dst_address_into_pointer(function, tac);
            tac->operation.id = IR_MOVE_TO_PTR;
            tac->src2 = tac->src1;
            tac->src1 = dup_value(load_tac->dst);
            tac->src1->type = load_tac->dst->type->target;
            tac->dst = NULL;
        }
    }
}