#include "wcc.h"
#include "aarch64.h"

// Registers used for function calls
const int int_arg_registers[] = {
    LIVE_RANGE_PREG_R00,
    LIVE_RANGE_PREG_R01,
    LIVE_RANGE_PREG_R02,
    LIVE_RANGE_PREG_R03,
    LIVE_RANGE_PREG_R04,
    LIVE_RANGE_PREG_R05,
    LIVE_RANGE_PREG_R06,
    LIVE_RANGE_PREG_R07,
};

const int fp_arg_registers[] = {
    LIVE_RANGE_PREG_V00,
    LIVE_RANGE_PREG_V01,
    LIVE_RANGE_PREG_V02,
    LIVE_RANGE_PREG_V03,
    LIVE_RANGE_PREG_V04,
    LIVE_RANGE_PREG_V05,
    LIVE_RANGE_PREG_V06,
    LIVE_RANGE_PREG_V07,
};

const int clobbered_registers_in_function_call[] = {
    LIVE_RANGE_PREG_R09,
    LIVE_RANGE_PREG_R10,
    LIVE_RANGE_PREG_R11,
    LIVE_RANGE_PREG_R12,
    LIVE_RANGE_PREG_R13,

    LIVE_RANGE_PREG_V16,
    LIVE_RANGE_PREG_V17,
    LIVE_RANGE_PREG_V18,
    LIVE_RANGE_PREG_V19,
    LIVE_RANGE_PREG_V20,
    LIVE_RANGE_PREG_V21,
    LIVE_RANGE_PREG_V22,
    LIVE_RANGE_PREG_V23,
    LIVE_RANGE_PREG_V24,
    LIVE_RANGE_PREG_V25,
    LIVE_RANGE_PREG_V26,
    LIVE_RANGE_PREG_V27,
    LIVE_RANGE_PREG_V28,
    LIVE_RANGE_PREG_V29,
    LIVE_RANGE_PREG_V30,
    LIVE_RANGE_PREG_V31,
};

int clobbered_registers_in_function_call_count = sizeof(clobbered_registers_in_function_call) / sizeof(int);

// Called once at startup
void init_allocate_registers(void) {
    physical_register_count     =  32 + 33; // integer + floating point
    physical_int_register_count =  23;      // Available registers for integers
    physical_fp_register_count  =  8 + 23;  // Available registers for floating points TODO aarch64, this only covers arg registers for now

    preg_map = wcalloc(physical_register_count + 1, sizeof(int));
    callee_saved_registers = wcalloc(physical_register_count + 1, sizeof(int));

    // Which registers are preserved across function calls
    callee_saved_registers[REG_R19] = 1;
    callee_saved_registers[REG_R20] = 1;
    callee_saved_registers[REG_R21] = 1;
    callee_saved_registers[REG_R22] = 1;
    callee_saved_registers[REG_R23] = 1;
    callee_saved_registers[REG_R24] = 1;
    callee_saved_registers[REG_R25] = 1;
    callee_saved_registers[REG_R26] = 1;
    callee_saved_registers[REG_R27] = 1;
    callee_saved_registers[REG_R28] = 1;
    callee_saved_registers[REG_V08] = 1;
    callee_saved_registers[REG_V09] = 1;
    callee_saved_registers[REG_V10] = 1;
    callee_saved_registers[REG_V11] = 1;
    callee_saved_registers[REG_V12] = 1;
    callee_saved_registers[REG_V13] = 1;
    callee_saved_registers[REG_V14] = 1;
    callee_saved_registers[REG_V15] = 1;

    arg_register_set.int_registers = int_arg_registers;
    arg_register_set.fp_registers = fp_arg_registers;

    // Make function return value register sets
    static int int_rv_registers[2];
    static int sse_rv_registers[2];

    int_rv_registers[0] = LIVE_RANGE_PREG_R00;
    int_rv_registers[1] = LIVE_RANGE_PREG_R01;

    sse_rv_registers[0] = LIVE_RANGE_PREG_V00;
    sse_rv_registers[1] = LIVE_RANGE_PREG_V01;

    function_return_value_register_set.int_registers = int_rv_registers;
    function_return_value_register_set.fp_registers = sse_rv_registers;

    // All regular registers
    preg_map[LIVE_RANGE_PREG_R00 - 1] = REG_R00;
    preg_map[LIVE_RANGE_PREG_R01 - 1] = REG_R01;
    preg_map[LIVE_RANGE_PREG_R02 - 1] = REG_R02;
    preg_map[LIVE_RANGE_PREG_R03 - 1] = REG_R03;
    preg_map[LIVE_RANGE_PREG_R04 - 1] = REG_R04;
    preg_map[LIVE_RANGE_PREG_R05 - 1] = REG_R05;
    preg_map[LIVE_RANGE_PREG_R06 - 1] = REG_R06;
    preg_map[LIVE_RANGE_PREG_R07 - 1] = REG_R07;
    preg_map[LIVE_RANGE_PREG_R09 - 1] = REG_R09;
    preg_map[LIVE_RANGE_PREG_R10 - 1] = REG_R10;
    preg_map[LIVE_RANGE_PREG_R11 - 1] = REG_R11;
    preg_map[LIVE_RANGE_PREG_R12 - 1] = REG_R12;
    preg_map[LIVE_RANGE_PREG_R13 - 1] = REG_R13;
    preg_map[LIVE_RANGE_PREG_R19 - 1] = REG_R19;
    preg_map[LIVE_RANGE_PREG_R20 - 1] = REG_R20;
    preg_map[LIVE_RANGE_PREG_R21 - 1] = REG_R21;
    preg_map[LIVE_RANGE_PREG_R22 - 1] = REG_R22;
    preg_map[LIVE_RANGE_PREG_R23 - 1] = REG_R23;
    preg_map[LIVE_RANGE_PREG_R24 - 1] = REG_R24;
    preg_map[LIVE_RANGE_PREG_R25 - 1] = REG_R25;
    preg_map[LIVE_RANGE_PREG_R26 - 1] = REG_R26;
    preg_map[LIVE_RANGE_PREG_R27 - 1] = REG_R27;
    preg_map[LIVE_RANGE_PREG_R28 - 1] = REG_R28;

    live_range_reserved_pregs_offset = physical_int_register_count + physical_fp_register_count;
}

// Insert load from stack instructions for a vreg value that has been spilled.
// temp_preg has the physical temp register that is used to hold the
// pointer in the stack.
static void add_spill_load(Tac *tac, Value *value, int temp_preg) {
    int size = value->target_size;
    int offset = value->stack_offset;

    if (size < 1 || size > 4) panic("Expected a target size between 1 and 4: %d", size);
    if (offset < 0) panic("Got a negative stack_index: %d", offset);

    char *templates[]={"ldrb %vdw, [%v1x]", "ldrh %vdw, [%v1x]", "ldr %vdw, [%v1x]", "ldr %vdx, [%v1x]"};

    // Make a value for the sp register
    Value *sp = new_value();
    sp->type = new_type(TYPE_LONG);
    sp->preg = REG_SP;

    // Make a value for the temp_preg register
    Value *temp_preg_value = new_value();
    temp_preg_value->type = new_type(TYPE_LONG);
    temp_preg_value->preg = temp_preg;

    // Make a value for the offset
    Value *offset_value = new_integral_constant(TYPE_LONG, offset);

    // Add mov temp_preg, offset and encode the constant if necessary
    Tac *pre_tac = new_tac_before(tac, AARCH64_OP_MOV, temp_preg_value, offset_value, 0, 1);
    pre_tac->target_template = "mov %vdx, %v1x";
    process_integer_constant_move_to_register(pre_tac);

    // Add add temp_preg, sp, temp_preg
    pre_tac = new_tac_before(tac, AARCH64_OP_ADD, temp_preg_value, sp, temp_preg_value, 1);
    pre_tac->target_template = "add %vdx, %v1x, %v2x";

    // Add add temp_preg, sp, temp_preg
    pre_tac = new_tac_before(tac, AARCH64_OP_LDR, temp_preg_value, temp_preg_value, 0, 1);
    pre_tac->target_template = templates[size - 1];

    // Modify original value
    value->stack_offset = 0;
    value->offset = 0;
    value->preg = temp_preg;

    if (value->offset) panic("TODO aarch64 offsets in a spilled vreg");
}

// Append store to stack instructions for a vreg value that has been spilled
static Tac *add_spill_store(Tac *tac) {
    int size = tac->dst->target_size;
    int offset = tac->dst->stack_offset;

    if (size < 1 || size > 4) panic("Expected a target size between 1 and 4: %d", size);
    if (offset < 0) panic("Got a negative stack_index: %d", offset);

    char *templates[]={"strb %vdw, [%v1x]", "strh %vdw, [%v1x]", "str %vdw, [%v1x]", "str %vdx, [%v1x]"};

    // Make a value for the sp register
    Value *sp = new_value();
    sp->type = new_type(TYPE_LONG);
    sp->preg = REG_SP;

    // Make a value for the r14 register, which holds the dst value
    Value *r14_value = new_value();
    r14_value->type = new_type(size > 3 ? TYPE_LONG : TYPE_INT);
    r14_value->preg = REG_R14;

    // Make a value for the r15 register, which holds the pointer to the dst
    Value *r15_value = new_value();
    r15_value->type = new_type(TYPE_LONG);
    r15_value->preg = REG_R15;

    // Make a value for the offset
    Value *offset_value = new_integral_constant(TYPE_LONG, offset);

    // Add mov temp_preg, offset and encode the constant if necessary
    Tac *after_tac = new_tac_after(tac, AARCH64_OP_MOV, r15_value, offset_value, 0);
    after_tac->target_template = "mov %vdx, %v1x";
    after_tac = process_integer_constant_move_to_register(after_tac);

    // Add add temp_preg, sp, temp_preg
    after_tac = new_tac_after(after_tac, AARCH64_OP_ADD, r15_value, sp, r15_value);
    after_tac->target_template = "add %vdx, %v1x, %v2x";

    // Add add temp_preg, sp, temp_preg
    after_tac = new_tac_after(after_tac, AARCH64_OP_STR, r14_value, r15_value, 0);
    after_tac->target_template = templates[size - 1];

    // Modify original value
    tac->dst->stack_offset = 0;
    tac->dst->offset = 0;
    tac->dst->preg = REG_R14;

    if (tac->dst->offset) panic("TODO aarch64 offsets in a spilled vreg");

    return after_tac;
}

void add_spill_code(Function *function) {
    make_aarch64_stack_offsets(function);

    if (debug_instsel_spilling) printf("\nAdding spill code\n");

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (debug_instsel_spilling) print_instruction(stdout, tac, 0);

        if (tac->src1 && tac->src1->spilled) {
            if (debug_instsel_spilling) printf("Adding spill load\n");
            add_spill_load(tac, tac->src1, REG_R14);
        }

        if (tac->src2 && tac->src2->spilled) {
            if (debug_instsel_spilling) printf("Adding spill load\n");
            add_spill_load(tac, tac->src2, REG_R15);
        }

        if (tac->dst && tac->dst->spilled) {
            if (debug_instsel_spilling) printf("Adding spill store\n");
            tac = add_spill_store(tac);
        }
    }
}
