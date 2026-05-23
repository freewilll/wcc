#include "wcc.h"
#include "aarch64.h"

// Registers used for function calls
const int int_arg_registers[] = {
    LIVE_RANGE_PREG_REG_R00,
    LIVE_RANGE_PREG_REG_R01,
    LIVE_RANGE_PREG_REG_R02,
    LIVE_RANGE_PREG_REG_R03,
    LIVE_RANGE_PREG_REG_R04,
    LIVE_RANGE_PREG_REG_R05,
    LIVE_RANGE_PREG_REG_R06,
    LIVE_RANGE_PREG_REG_R07,
};

const int fp_arg_registers[] = {
    LIVE_RANGE_PREG_REG_V00,
    LIVE_RANGE_PREG_REG_V01,
    LIVE_RANGE_PREG_REG_V02,
    LIVE_RANGE_PREG_REG_V03,
    LIVE_RANGE_PREG_REG_V04,
    LIVE_RANGE_PREG_REG_V05,
    LIVE_RANGE_PREG_REG_V06,
    LIVE_RANGE_PREG_REG_V07,
};

// Called once at startup
void init_allocate_registers(void) {
    physical_register_count     =  32 + 33; // integer + floating point
    physical_int_register_count =  23;      // Available registers for integers
    physical_fp_register_count  =  0;       // Available registers for floating points TODO aarch64

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

    // All regular registers
    preg_map[LIVE_RANGE_PREG_REG_R00 - 1] = REG_R00;
    preg_map[LIVE_RANGE_PREG_REG_R01 - 1] = REG_R01;
    preg_map[LIVE_RANGE_PREG_REG_R02 - 1] = REG_R02;
    preg_map[LIVE_RANGE_PREG_REG_R03 - 1] = REG_R03;
    preg_map[LIVE_RANGE_PREG_REG_R04 - 1] = REG_R04;
    preg_map[LIVE_RANGE_PREG_REG_R05 - 1] = REG_R05;
    preg_map[LIVE_RANGE_PREG_REG_R06 - 1] = REG_R06;
    preg_map[LIVE_RANGE_PREG_REG_R07 - 1] = REG_R07;
    preg_map[LIVE_RANGE_PREG_REG_R09 - 1] = REG_R09;
    preg_map[LIVE_RANGE_PREG_REG_R10 - 1] = REG_R10;
    preg_map[LIVE_RANGE_PREG_REG_R11 - 1] = REG_R11;
    preg_map[LIVE_RANGE_PREG_REG_R12 - 1] = REG_R12;
    preg_map[LIVE_RANGE_PREG_REG_R13 - 1] = REG_R13;
    preg_map[LIVE_RANGE_PREG_REG_R19 - 1] = REG_R19;
    preg_map[LIVE_RANGE_PREG_REG_R20 - 1] = REG_R20;
    preg_map[LIVE_RANGE_PREG_REG_R21 - 1] = REG_R21;
    preg_map[LIVE_RANGE_PREG_REG_R22 - 1] = REG_R22;
    preg_map[LIVE_RANGE_PREG_REG_R23 - 1] = REG_R23;
    preg_map[LIVE_RANGE_PREG_REG_R24 - 1] = REG_R24;
    preg_map[LIVE_RANGE_PREG_REG_R25 - 1] = REG_R25;
    preg_map[LIVE_RANGE_PREG_REG_R26 - 1] = REG_R26;
    preg_map[LIVE_RANGE_PREG_REG_R27 - 1] = REG_R27;
    preg_map[LIVE_RANGE_PREG_REG_R28 - 1] = REG_R28;

    live_range_reserved_pregs_offset = physical_int_register_count + physical_fp_register_count;
}
