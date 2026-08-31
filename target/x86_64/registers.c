#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"
#include "x86_64.h"

// Registers used for function calls
const int int_arg_registers[] = {
    LIVE_RANGE_PREG_RDI,
    LIVE_RANGE_PREG_RSI,
    LIVE_RANGE_PREG_RDX,
    LIVE_RANGE_PREG_RCX,
    LIVE_RANGE_PREG_R08,
    LIVE_RANGE_PREG_R09,
};

const int fp_arg_registers[] = {
    LIVE_RANGE_PREG_XMM00,
    LIVE_RANGE_PREG_XMM01,
    LIVE_RANGE_PREG_XMM02,
    LIVE_RANGE_PREG_XMM03,
    LIVE_RANGE_PREG_XMM04,
    LIVE_RANGE_PREG_XMM05,
    LIVE_RANGE_PREG_XMM06,
    LIVE_RANGE_PREG_XMM07,
};

// Called once at startup
void init_allocate_registers(void) {
    physical_register_count     =  32; // integer + xmm
    physical_int_register_count =  12; // Allocatable registers for integers
    physical_fp_register_count  =  14; // Allocatable registers for floating points

    preg_map = wcalloc(physical_register_count + 1, sizeof(int));
    callee_saved_registers = wcalloc(physical_register_count + 1, sizeof(int));

    // Which registers are preserved across function calls
    callee_saved_registers[REG_RBX] = 1;
    callee_saved_registers[REG_R12] = 1;
    callee_saved_registers[REG_R13] = 1;
    callee_saved_registers[REG_R14] = 1;
    callee_saved_registers[REG_R15] = 1;

    arg_register_set.int_registers = int_arg_registers;
    arg_register_set.fp_registers = fp_arg_registers;

    // Make function return value register sets
    static int int_rv_registers[2];
    static int sse_rv_registers[2];

    int_rv_registers[0] = LIVE_RANGE_PREG_RAX;
    int_rv_registers[1] = LIVE_RANGE_PREG_RDX;

    sse_rv_registers[0] = LIVE_RANGE_PREG_XMM00;
    sse_rv_registers[1] = LIVE_RANGE_PREG_XMM01;

    function_return_value_register_set.int_registers = int_rv_registers;
    function_return_value_register_set.fp_registers = sse_rv_registers;

    // All registers except RSP, RBP, R10 and R11
    preg_map[LIVE_RANGE_PREG_RAX - 1] = REG_RAX;
    preg_map[LIVE_RANGE_PREG_RBX - 1] = REG_RBX;
    preg_map[LIVE_RANGE_PREG_RCX - 1] = REG_RCX;
    preg_map[LIVE_RANGE_PREG_RDX - 1] = REG_RDX;
    preg_map[LIVE_RANGE_PREG_RSI - 1] = REG_RSI;
    preg_map[LIVE_RANGE_PREG_RDI - 1] = REG_RDI;
    preg_map[LIVE_RANGE_PREG_R08 - 1] = REG_R08;
    preg_map[LIVE_RANGE_PREG_R09 - 1] = REG_R09;
    preg_map[LIVE_RANGE_PREG_R12 - 1] = REG_R12;
    preg_map[LIVE_RANGE_PREG_R13 - 1] = REG_R13;
    preg_map[LIVE_RANGE_PREG_R14 - 1] = REG_R14;
    preg_map[LIVE_RANGE_PREG_R15 - 1] = REG_R15;

    // Map all 16 SSE xmm* registers
    for (int i = 0; i < physical_fp_register_count; i++)
        preg_map[LIVE_RANGE_PREG_XMM00 + i - 1] = REG_XMM00 + i;

    live_range_reserved_pregs_offset = physical_int_register_count + physical_fp_register_count;
}

static Tac *make_spill_instruction(Value *v) {
    int x86_operation;
    char *target_template;

    make_value_target_size(v);

    if (v->type->type == TYPE_FUNCTION) {
        x86_operation = X86_OP_MOV;
        target_template = "movq %v1q, %vdq";
    }
    else if (v->type->type == TYPE_FLOAT) {
        x86_operation = X86_OP_MOV;
        target_template = "movss %v1F, %vdF";
    }
    else if (v->type->type == TYPE_FLOAT) {
        x86_operation = X86_OP_MOV;
        target_template = "movsd %v1D, %vdD";
    }
    else if (v->target_size == 1) {
        x86_operation = X86_OP_MOV;
        target_template = "movb %v1b, %vdb";
    }
    else if (v->target_size == 2) {
        x86_operation = X86_OP_MOV;
        target_template = "movw %v1w, %vdw";
    }
    else if (v->target_size == 3) {
        x86_operation = X86_OP_MOV;
        target_template = "movl %v1l, %vdl";
    }
    else if (v->target_size == 4) {
        x86_operation = X86_OP_MOV;
        target_template = "movq %v1q, %vdq";
    }
    else
        panic("Unknown x86 size %d", v->target_size);

    Tac *tac = new_instruction(x86_operation);
    tac->target_template = target_template;

    return tac;
}

static void add_spill_load(Tac *ir, int src, int preg) {
    Value *v = src == 1 ? ir->src1 : ir->src2;

    Tac *tac = make_spill_instruction(v);
    tac->src1 = v;
    tac->dst = new_value();

    // Codegen needs to know if this is a function; in all other cases the spill is
    // done on a full 64 bit register.
    if (v->type->type == TYPE_FUNCTION)
        tac->dst->type = new_type(TYPE_FUNCTION);
    else
        tac->dst->type = new_type(TYPE_LONG);

    tac->dst->target_size = 4;
    tac->dst->vreg = -1000;   // Dummy value
    tac->dst->preg = preg;

    if (src == 1)
        ir->src1 = dup_value(tac->dst);
    else
        ir->src2 = dup_value(tac->dst);

    insert_tac_before(ir, tac, 1);

    // If a register has an offset, it's used in an indirect operation, such
    // as 4(rax). This means, take the value in rax and add four to it. However, by
    // moving it to the stack, without any manipulation the meaning changes from
    // 4(rax) to 4 + allocated_stack_offset(rbp), which would be incorrect. To fix this,
    // the offset is removed and converted to an add instruction, so that we get
    // movq allocated_stack_offset(rbp), spill_reg
    // addq spill_reg, 4.
    if (v->offset) {
        Tac *tac = new_instruction(X86_OP_MOV);
        tac->target_template = "addq $%v1q, %vdq";

        tac->src1 = new_integral_constant(TYPE_LONG, v->offset);

        tac->dst = new_value();
        tac->dst->type = new_type(TYPE_LONG);
        tac->dst->target_size = 4;
        tac->dst->vreg = -1000;   // Dummy value
        tac->dst->preg = preg;

        insert_tac_before(ir, tac, 1);

        v->offset = 0;
    }
}

static void add_spill_store(Tac *ir, Value *v, int preg) {
    Tac *tac = make_spill_instruction(v);
    tac->src1 = new_value();
    tac->src1->type = new_type(TYPE_LONG);
    tac->src1->target_size = 4;
    tac->src1->vreg = -1000;   // Dummy value
    tac->src1->preg = preg;

    tac->dst = v;
    ir->dst = dup_value(tac->src1);

    if (!ir->next) {
        ir->next = tac;
        tac->prev = ir;
    }
    else
        insert_tac_before(ir->next, tac, 0);
}

// Return one of the int or sse registers used as temporary in spill code
static int get_spill_register(Value *v, int spill_register) {
    if (is_floating_point_type(v->type))
        return spill_register == 1 ? REG_XMM14 : REG_XMM15;
    else
        return spill_register == 1 ? REG_R10 : REG_R11;
}

void add_spill_code(Function *function) {
    if (debug_instsel_spilling) printf("\nAdding spill code to %s\n", function->identifier);

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (debug_instsel_spilling) print_instruction(stdout, tac, 0);

        // Allow all moves where either dst is a register and src is on the stack
        if (tac->operation.id == X86_OP_MOV || tac->operation.id == X86_OP_MOVS || tac->operation.id == X86_OP_MOVZ)
            if (tac->dst && tac->dst->preg != -1 && tac->src1 && tac->src1->stack.index) continue;

        // Allow non sign-extends moves if the dst is on the stack and the src is a register
        if (tac->operation.id == X86_OP_MOV && tac->dst && tac->dst->stack.index && tac->src1 && tac->src1->preg != -1) continue;

        int dst_eq_src1 = (tac->dst && tac->src1 && tac->dst->stack.index == tac->src1->stack.index);

        if (tac->src1 && tac->src1->spilled)  {
            if (debug_instsel_spilling) printf("Adding spill load\n");
            add_spill_load(tac, 1, get_spill_register(tac->src1, 1));
            if (dst_eq_src1) {
                // Special case where src1 is the same as dst, in that case, r10/xmm14 contains the result.
                int spill_register = get_spill_register(tac->dst, 1);
                add_spill_store(tac, tac->dst, spill_register);
                tac = tac->next;
            }
        }

        if (tac->src2 && tac->src2->spilled) {
            if (debug_instsel_spilling) printf("Adding spill load\n");
            add_spill_load(tac, 2, get_spill_register(tac->src2, 2));
        }

        if (tac->dst && tac->dst->spilled) {
            if (debug_instsel_spilling) printf("Adding spill store\n");
            if (tac->operation.id == X86_OP_CALL) panic("Unexpected spill from X_CALL");
            add_spill_store(tac, tac->dst, get_spill_register(tac->dst, 2));
            tac = tac->next;
        }
    }
}
