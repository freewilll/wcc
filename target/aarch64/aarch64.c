#include "wcc.h"
#include "aarch64.h"

int char_is_unsigned_by_default = 1;

char is_32bit_to_aarch64_size(int is_32bit) {
    return is_32bit ? 'w' : 'x';
}

char size_to_aarch64_size(int size) {
    switch (size) {
        case 1:  return 'w'; break;
        case 2:  return 'w'; break;
        case 3:  return 'w'; break;
        case 4:  return 'x'; break;
        default: panic("Unknown size %d", size);
    }
}

char *target_op_name(int operation) {
    switch (operation) {
        case AARCH64_OP_NULL:               return "(null)";
        case AARCH64_OP_STP:                return "stp";
        case AARCH64_OP_LDP:                return "ldp";
        case AARCH64_OP_MOV:                return "mov";
        case AARCH64_OP_MOV_INT_CST:        return "movcst";        // Implemented in codegen
        case AARCH64_OP_ADD:                return "add";
        case AARCH64_OP_SUB:                return "sub";
        case AARCH64_OP_MUL:                return "mul";
        case AARCH64_OP_DIV:                return "div";
        case AARCH64_OP_ADD_LO12:           return "addlo12";
        case AARCH64_OP_BAND:               return "and";
        case AARCH64_OP_BOR:                return "or";
        case AARCH64_OP_XOR:                return "xor";
        case AARCH64_OP_BNOT:               return "bnot";
        case AARCH64_OP_LSL:                return "lsl";
        case AARCH64_OP_LSR:                return "lsr";
        case AARCH64_OP_ASR:                return "asr";
        case AARCH64_OP_CSET:               return "cset";
        case AARCH64_OP_CALL:               return "call";
        case AARCH64_OP_CALL_FROM_FUNC:     return "callf";         // Used in codegen
        case AARCH64_OP_PUSH_DOUBLE_WORD:   return "pushdw";        // Used in codegen
        case AARCH64_OP_POP_DOUBLE_WORD:    return "popdw";         // Used in codegen
        case AARCH64_OP_ALLOCATE_STACK:     return "allocst";       // Used in codegen
        case AARCH64_OP_DEALLOCATE_STACK:   return "deallocst";     // Used in codegen
        case AARCH64_OP_ADRP:               return "adrp";

        default:                        panic("Unknown aarch64 operation %d", operation);
    }
}

void print_target_instruction(void *f, Tac *tac) {
    int o = tac->operation.id;
    switch (tac->operation.id) {
        case AARCH64_OP_CALL:
            fprintf(f, "%-12s", operation_string(o));
            print_value(f, tac->src1, 1);
            if (tac->dst) {
                printf(" -> ");
                print_value(f, tac->dst, 1);
            }
            break;

        case AARCH64_OP_CSET:
            fprintf(f, "%-12s\n", operation_string(o));
            break;

        case AARCH64_OP_BNOT:
            fprintf(f, "%-12s", operation_string(o));
            print_value(f, tac->dst, 1);
            break;

        case AARCH64_OP_MOV:
        case AARCH64_OP_STP:
        case AARCH64_OP_LDP:
        case AARCH64_OP_MOV_INT_CST:
        case AARCH64_OP_ADD:
        case AARCH64_OP_SUB:
        case AARCH64_OP_MUL:
        case AARCH64_OP_DIV:
        case AARCH64_OP_BAND:
        case AARCH64_OP_BOR:
        case AARCH64_OP_XOR:
        case AARCH64_OP_LSL:
        case AARCH64_OP_LSR:
        case AARCH64_OP_ASR:
        case AARCH64_OP_ADRP:
            fprintf(f, "%-12s", operation_string(o));
            print_value(f, tac->dst, 1);
            fprintf(f, ", ");
            print_value(f, tac->src1,  1);
            break;

        default:
            panic("print_instruction(): Unknown operation: %d", tac->operation.id);
    }
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
            tac->src1 = load_tac->dst;
            tac->dst = NULL;
        }
    }
}