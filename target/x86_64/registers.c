#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../wcc.h"

// Registers used for function calls
const int int_arg_registers[6] = {
    LIVE_RANGE_PREG_RDI_INDEX,
    LIVE_RANGE_PREG_RSI_INDEX,
    LIVE_RANGE_PREG_RDX_INDEX,
    LIVE_RANGE_PREG_RCX_INDEX,
    LIVE_RANGE_PREG_R08_INDEX,
    LIVE_RANGE_PREG_R09_INDEX,
};

const int sse_arg_registers[8] = {
    LIVE_RANGE_PREG_XMM00_INDEX,
    LIVE_RANGE_PREG_XMM01_INDEX,
    LIVE_RANGE_PREG_XMM02_INDEX,
    LIVE_RANGE_PREG_XMM03_INDEX,
    LIVE_RANGE_PREG_XMM04_INDEX,
    LIVE_RANGE_PREG_XMM05_INDEX,
    LIVE_RANGE_PREG_XMM06_INDEX,
    LIVE_RANGE_PREG_XMM07_INDEX,
};


// Called once at startup
void init_allocate_registers(void) {
    // Which registers are preserved across function calls
    callee_saved_registers[REG_RBX] = 1;
    callee_saved_registers[REG_R12] = 1;
    callee_saved_registers[REG_R13] = 1;
    callee_saved_registers[REG_R14] = 1;
    callee_saved_registers[REG_R15] = 1;

    arg_register_set.int_registers = int_arg_registers;
    arg_register_set.fp_registers = sse_arg_registers;

    // Make function return value register sets
    static int int_rv_registers[2];
    static int sse_rv_registers[2];

    int_rv_registers[0] = LIVE_RANGE_PREG_RAX_INDEX;
    int_rv_registers[1] = LIVE_RANGE_PREG_RDX_INDEX;

    sse_rv_registers[0] = LIVE_RANGE_PREG_XMM00_INDEX;
    sse_rv_registers[1] = LIVE_RANGE_PREG_XMM01_INDEX;

    function_return_value_register_set.int_registers = int_rv_registers;
    function_return_value_register_set.fp_registers = sse_rv_registers;

    // All registers except RSP, RBP, R10 and R11
    preg_map[LIVE_RANGE_PREG_RAX_INDEX - 1] = REG_RAX;
    preg_map[LIVE_RANGE_PREG_RBX_INDEX - 1] = REG_RBX;
    preg_map[LIVE_RANGE_PREG_RCX_INDEX - 1] = REG_RCX;
    preg_map[LIVE_RANGE_PREG_RDX_INDEX - 1] = REG_RDX;
    preg_map[LIVE_RANGE_PREG_RSI_INDEX - 1] = REG_RSI;
    preg_map[LIVE_RANGE_PREG_RDI_INDEX - 1] = REG_RDI;
    preg_map[LIVE_RANGE_PREG_R08_INDEX - 1] = REG_R08;
    preg_map[LIVE_RANGE_PREG_R09_INDEX - 1] = REG_R09;
    preg_map[LIVE_RANGE_PREG_R12_INDEX - 1] = REG_R12;
    preg_map[LIVE_RANGE_PREG_R13_INDEX - 1] = REG_R13;
    preg_map[LIVE_RANGE_PREG_R14_INDEX - 1] = REG_R14;
    preg_map[LIVE_RANGE_PREG_R15_INDEX - 1] = REG_R15;

    // Map all 16 SSE xmm* registers
    for (int i = 0; i < PHYSICAL_FP_REGISTER_COUNT; i++)
        preg_map[LIVE_RANGE_PREG_XMM00_INDEX + i - 1] = REG_XMM00 + i;

    live_range_reserved_pregs_offset = PHYSICAL_INT_REGISTER_COUNT + PHYSICAL_FP_REGISTER_COUNT;
}

