#ifndef X86_64_H
#define X86_64_H

#include "../../wcc.h"

// Physical registers
enum {
    // Integers
    REG_RAX,
    REG_RBX,
    REG_RCX,
    REG_RDX,
    REG_RSI,
    REG_RDI,
    REG_RBP,
    REG_RSP,
    REG_R08,
    REG_R09,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,

    // SSE
    REG_XMM00,
    REG_XMM14 = 30,
    REG_XMM15,
};

#define OVERFLOW_AREA_ADDRESS_MAGIC_STACK_INDEX 256

enum {
    // Liveness interval indexes corresponding to reserved physical registers

    LIVE_RANGE_PREG_RAX = 1,
    LIVE_RANGE_PREG_RBX,
    LIVE_RANGE_PREG_RCX,
    LIVE_RANGE_PREG_RDX,
    LIVE_RANGE_PREG_RSI,
    LIVE_RANGE_PREG_RDI,
    LIVE_RANGE_PREG_R08,
    LIVE_RANGE_PREG_R09,
    LIVE_RANGE_PREG_R12,
    LIVE_RANGE_PREG_R13,
    LIVE_RANGE_PREG_R14,
    LIVE_RANGE_PREG_R15,      // 12

    LIVE_RANGE_PREG_XMM00,    // 13
    LIVE_RANGE_PREG_XMM01,
    LIVE_RANGE_PREG_XMM02,
    LIVE_RANGE_PREG_XMM03,
    LIVE_RANGE_PREG_XMM04,
    LIVE_RANGE_PREG_XMM05,
    LIVE_RANGE_PREG_XMM06,
    LIVE_RANGE_PREG_XMM07,
};


enum x86_instruction_op {
    // x86 instructions
    X86_OP_ARG = TARGET_OPS_START,             // A function call argument, pushed into the stack
    X86_OP_ALLOCATE_STACK,
    X86_OP_MOVE_STACK_PTR,
    X86_OP_CALL,

    X86_OP_MOV,
    X86_OP_MOVZ, // Zero extend
    X86_OP_MOVS, // Sign extend
    X86_OP_MOVC, // Move, but not allowed to be coalesced

    X86_OP_MOV_FROM_IND,
    X86_OP_MOV_FROM_SCALED_IND,
    X86_OP_MOV_TO_IND,

    X86_OP_LEA,
    X86_OP_LEA_FROM_SCALED_IND,
    X86_OP_ADD,
    X86_OP_ADDC,
    X86_OP_SUB,
    X86_OP_SUBC,
    X86_OP_MUL,
    X86_OP_MUL128A,
    X86_OP_MUL128B,
    X86_OP_MOD,
    X86_OP_IDIV,
    X86_OP_CQTO,
    X86_OP_CLTD,
    X86_OP_SHC, // Shift with a constant
    X86_OP_SHR, // Shift with a register
    X86_OP_BOR,
    X86_OP_BAND,
    X86_OP_XOR,
    X86_OP_BNOT,
    X86_OP_BSR,   // Bit shift reverse
    X86_OP_TZCNT, // Count the number of trailing zero bits

    X86_OP_FADD,
    X86_OP_FSUB,
    X86_OP_FMUL,
    X86_OP_FDIV,
    X86_OP_LD_EQ_CMP, // Long double == and != comparisons

    X86_OP_CMP,
    X86_OP_CMPZ,
    X86_OP_COMIS,
    X86_OP_TEST,
    X86_OP_JZ,
    X86_OP_JNZ,
    X86_OP_JMP,

    X86_OP_JE,
    X86_OP_JNE,

    X86_OP_JLT,
    X86_OP_JGT,
    X86_OP_JLE,
    X86_OP_JGE,

    X86_OP_JB,
    X86_OP_JA,
    X86_OP_JBE,
    X86_OP_JAE,

    X86_OP_SETE,
    X86_OP_SETNE,

    X86_OP_SETP,
    X86_OP_SETNP,

    X86_OP_SETLT,
    X86_OP_SETGT,
    X86_OP_SETLE,
    X86_OP_SETGE,

    X86_OP_SETB,
    X86_OP_SETA,
    X86_OP_SETBE,
    X86_OP_SETAE,

    X86_OP_PUSH,
    X86_OP_POP,
    X86_OP_LEAVE,
    X86_OP_RET_FROM_FUNC,
    X86_OP_CALL_FROM_FUNC,
};

char size_to_x86_size(int size);

#endif
