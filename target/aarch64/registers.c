#include "wcc.h"
#include "aarch64.h"

// Called once at startup
void init_allocate_registers(void) {
    physical_register_count     =  32 + 33; // integer + floaing point
    physical_int_register_count =  0;       // Available registers for integers TODO aarch64
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

    // TODO aarcb64
    // preg_map ...

    // TODO aarcb64
    // live_range_reserved_pregs_offset = physical_int_register_count + physical_fp_register_count;
}
