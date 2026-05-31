#ifndef AARCH64_H
#define AARCH64_H

#include "../../wcc.h"

// Physical registers
enum {
    // Integers
    REG_R00,    // Parameter/result registers
    REG_R01,    // Parameter/result registers
    REG_R02,    // Parameter/result registers
    REG_R03,    // Parameter/result registers
    REG_R04,    // Parameter/result registers
    REG_R05,    // Parameter/result registers
    REG_R06,    // Parameter/result registers
    REG_R07,    // Parameter/result registers
    REG_R08,    // Indirect result location register
    REG_R09,    // Temporary registers
    REG_R10,    // Temporary registers
    REG_R11,    // Temporary registers
    REG_R12,    // Temporary registers
    REG_R13,    // Temporary registers
    REG_R14,    // Temporary registers, used to spill src1 TODO aarch64
    REG_R15,    // Temporary registers, used to spill src2 TODO aarch64
    REG_R16,    // The first intra-procedure-call scratch register
    REG_R17,    // The second intra-procedure-call temporary register
    REG_R18,    // The Platform Register, if needed; otherwise a temporary register. See notes.
    REG_R19,    // Callee-saved registers
    REG_R20,    // Callee-saved registers
    REG_R21,    // Callee-saved registers
    REG_R22,    // Callee-saved registers
    REG_R23,    // Callee-saved registers
    REG_R24,    // Callee-saved registers
    REG_R25,    // Callee-saved registers
    REG_R26,    // Callee-saved registers
    REG_R27,    // Callee-saved registers
    REG_R28,    // Callee-saved registers
    REG_R29,    // Frame register
    REG_R30,    // Link register
    REG_SP,     // Stack pointer

    // Floating point
    REG_V00,    // Parameter/result registers
    REG_V01,    // Parameter/result registers
    REG_V02,    // Parameter/result registers
    REG_V03,    // Parameter/result registers
    REG_V04,    // Parameter/result registers
    REG_V05,    // Parameter/result registers
    REG_V06,    // Parameter/result registers
    REG_V07,    // Parameter/result registers
    REG_V08,    // Callee-saved registers
    REG_V09,    // Callee-saved registers
    REG_V10,    // Callee-saved registers
    REG_V11,    // Callee-saved registers
    REG_V12,    // Callee-saved registers
    REG_V13,    // Callee-saved registers
    REG_V14,    // Callee-saved registers
    REG_V15,    // Callee-saved registers
    REG_V16,    // Temporary registers
    REG_V17,    // Temporary registers
    REG_V18,    // Temporary registers
    REG_V19,    // Temporary registers
    REG_V20,    // Temporary registers
    REG_V21,    // Temporary registers
    REG_V22,    // Temporary registers
    REG_V23,    // Temporary registers
    REG_V24,    // Temporary registers
    REG_V25,    // Temporary registers
    REG_V26,    // Temporary registers
    REG_V27,    // Temporary registers
    REG_V28,    // Temporary registers
    REG_V29,    // Temporary registers
    REG_V30,    // Temporary registers
    REG_V31,    // Temporary registers
    REG_FSPR,   // Status register
};

enum {
    // Liveness interval indexes corresponding to reserved physical registers

    LIVE_RANGE_PREG_R00 = 1,    // Parameter/result registers
    LIVE_RANGE_PREG_R01,        // Parameter/result registers
    LIVE_RANGE_PREG_R02,        // Parameter/result registers
    LIVE_RANGE_PREG_R03,        // Parameter/result registers
    LIVE_RANGE_PREG_R04,        // Parameter/result registers
    LIVE_RANGE_PREG_R05,        // Parameter/result registers
    LIVE_RANGE_PREG_R06,        // Parameter/result registers
    LIVE_RANGE_PREG_R07,        // Parameter/result registers
    LIVE_RANGE_PREG_R09,        // Temporary registers
    LIVE_RANGE_PREG_R10,        // Temporary registers
    LIVE_RANGE_PREG_R11,        // Temporary registers
    LIVE_RANGE_PREG_R12,        // Temporary registers
    LIVE_RANGE_PREG_R13,        // Temporary registers
    LIVE_RANGE_PREG_R19,        // Callee-saved registers
    LIVE_RANGE_PREG_R20,        // Callee-saved registers
    LIVE_RANGE_PREG_R21,        // Callee-saved registers
    LIVE_RANGE_PREG_R22,        // Callee-saved registers
    LIVE_RANGE_PREG_R23,        // Callee-saved registers
    LIVE_RANGE_PREG_R24,        // Callee-saved registers
    LIVE_RANGE_PREG_R25,        // Callee-saved registers
    LIVE_RANGE_PREG_R26,        // Callee-saved registers
    LIVE_RANGE_PREG_R27,        // Callee-saved registers
    LIVE_RANGE_PREG_R28,        // Callee-saved registers

    LIVE_RANGE_PREG_V00,        // Parameter/result registers
    LIVE_RANGE_PREG_V01,        // Parameter/result registers
    LIVE_RANGE_PREG_V02,        // Parameter/result registers
    LIVE_RANGE_PREG_V03,        // Parameter/result registers
    LIVE_RANGE_PREG_V04,        // Parameter/result registers
    LIVE_RANGE_PREG_V05,        // Parameter/result registers
    LIVE_RANGE_PREG_V06,        // Parameter/result registers
    LIVE_RANGE_PREG_V07,        // Parameter/result registers

    LIVE_RANGE_PREG_V09,        // Callee-saved registers
    LIVE_RANGE_PREG_V10,        // Callee-saved registers
    LIVE_RANGE_PREG_V11,        // Callee-saved registers
    LIVE_RANGE_PREG_V12,        // Callee-saved registers
    LIVE_RANGE_PREG_V13,        // Callee-saved registers
    LIVE_RANGE_PREG_V14,        // Callee-saved registers
    LIVE_RANGE_PREG_V15,        // Callee-saved registers
    LIVE_RANGE_PREG_V16,        // Temporary registers
    LIVE_RANGE_PREG_V17,        // Temporary registers
    LIVE_RANGE_PREG_V18,        // Temporary registers
    LIVE_RANGE_PREG_V19,        // Temporary registers
    LIVE_RANGE_PREG_V20,        // Temporary registers
    LIVE_RANGE_PREG_V21,        // Temporary registers
    LIVE_RANGE_PREG_V22,        // Temporary registers
    LIVE_RANGE_PREG_V23,        // Temporary registers
    LIVE_RANGE_PREG_V24,        // Temporary registers
    LIVE_RANGE_PREG_V25,        // Temporary registers
    LIVE_RANGE_PREG_V26,        // Temporary registers
    LIVE_RANGE_PREG_V27,        // Temporary registers
    LIVE_RANGE_PREG_V28,        // Temporary registers
    LIVE_RANGE_PREG_V29,        // Temporary registers
    LIVE_RANGE_PREG_V30,        // Temporary registers
    LIVE_RANGE_PREG_V31,        // Temporary registers
};

enum aarch64_instruction_op {
    // aarch64 instructions
    AARCH64_OP_NULL = TARGET_OPS_START,  // An general OP that needs no special handling
    AARCH64_OP_STP,
    AARCH64_OP_LDP,
    AARCH64_OP_RET_FROM_FUNC,
    AARCH64_OP_MOV,
    AARCH64_OP_MOV_INT_CST,             // Pseudo operation, implemented by codegen
    AARCH64_OP_ADD,
    AARCH64_OP_SUB,
    AARCH64_OP_MUL,
    AARCH64_OP_DIV,
    AARCH64_OP_ADD_LO12,
    AARCH64_OP_BAND,
    AARCH64_OP_BOR,
    AARCH64_OP_XOR,
    AARCH64_OP_BNOT,
    AARCH64_OP_LSL,
    AARCH64_OP_LSR,
    AARCH64_OP_ASR,
    AARCH64_OP_CSET,
    AARCH64_OP_CMP,
    AARCH64_OP_BEQ,
    AARCH64_OP_BNE,
    AARCH64_OP_B,
    AARCH64_OP_CALL,
    AARCH64_OP_CALL_FROM_FUNC,
    AARCH64_OP_PUSH_DOUBLE_WORD,
    AARCH64_OP_POP_DOUBLE_WORD,
    AARCH64_OP_ALLOCATE_STACK,
    AARCH64_OP_DEALLOCATE_STACK,
    AARCH64_OP_ADRP,
};

extern const int clobbered_registers_in_function_call[];
extern int clobbered_registers_in_function_call_count;

char size_to_aarch64_size(int size);
char is_32bit_to_aarch64_size(int is_32bit);

#endif
