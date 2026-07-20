#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"
#include "../../aarch64.h"

Function *function;

void assert_long(long expected, long actual);

void assert_int(int expected, int actual, char *message) {
    if (expected != actual) {
        failures++;
        printf("%-18s failed expected %d, got %d\n", message, expected, actual);
    }
}

// Run with a single instruction
Tac *si(Function *function, int label, int operation, Value *dst, Value *src1, Value *src2) {
    Tac *tac;

    start_ir();
    i(label, operation, dst, src1, src2);
    finish_ir(function);

    return tac;
}

void test_instrsel_constant_bitshift(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_BSHL v << c
    si(function, 0, IR_BSHL, vsz(3, TYPE_CHAR), vsz(1, TYPE_CHAR), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_INT), vsz(1, TYPE_INT), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(1));
    assert_target_op("lsl         r2x, r1x, 1");

    // IR_ASHR v << c
    si(function, 0, IR_ASHR, vsz(3, TYPE_CHAR), vsz(1, TYPE_CHAR), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_INT), vsz(1, TYPE_INT), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(1));
    assert_target_op("asr         r2x, r1x, 1");

    // IR_BSHR v << c
    si(function, 0, IR_BSHR, vusz(3, TYPE_CHAR), vusz(1, TYPE_CHAR), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_INT), vusz(1, TYPE_INT), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_LONG), vusz(1, TYPE_LONG), c(1));
    assert_target_op("lsr         r2x, r1x, 1");
}

// Immediates in add/sub operations can be encoded in the instruction if.
// the constant is 12-bit immediate, optionally shifted left by 12 bits.
void test_instrsel_add_sub_constant(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_ADD v1 + v1 + c
    si(function, 0, IR_ADD, vsz(3, TYPE_CHAR),   vsz(1, TYPE_CHAR),   ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_SHORT),  vsz(1, TYPE_SHORT),  ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_INT),    vsz(1, TYPE_INT),    ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_LONG),   vsz(1, TYPE_LONG),   ci(1)); assert_target_op("add         r2x, r1x, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(1)); assert_target_op("add         r2x, r1x, 1");

    // IR_SUB v1 + v1 + c
    si(function, 0, IR_SUB, vsz(3, TYPE_CHAR),   vsz(1, TYPE_CHAR),   ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_SHORT),  vsz(1, TYPE_SHORT),  ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_INT),    vsz(1, TYPE_INT),    ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_LONG),   vsz(1, TYPE_LONG),   ci(1)); assert_target_op("sub         r2x, r1x, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(1)); assert_target_op("sub         r2x, r1x, 1");

    // Check encodable constants
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(4095));        assert_target_op("add         r2x, r1x, 4095");     // 0xfff
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(4096));        assert_target_op("add         r2x, r1x, 4096");     // 0x001 << 12
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(0x00800000));  assert_target_op("add         r2x, r1x, 8388608");  // 0x800 << 12
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(0x00fff000));  assert_target_op("add         r2x, r1x, 16773120"); // 0xfff << 12

    // Not encodable constant, it has to get loaded in a register
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(0x00ffffff));
    assert_long(AARCH64_OP_MOV_INT_CST, ir_start->operation.id);
    ir_start = ir_start->next;
    assert_target_op("add         r2x, r1x, r3x");
}

#define CHECK_32_BIT_LOGICAL_IMMEDIATE(expected_result, value) assert_int(expected_result, is_logical_immediate(value, 1), # value);
#define CHECK_64_BIT_LOGICAL_IMMEDIATE(expected_result, value) assert_int(expected_result, is_logical_immediate(value, 0), # value);

int test_is_logical_immediate_32_bit() {
    // All zeros or all ones
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000000U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0xffffffffU);

    // Simple patterns
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00000001U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00000003U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x000000ffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00000fffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00ffffffU);

    // High bit set
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x80000000U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xf0000000U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xffff0000U);

    // Repeated 2 bit
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x55555555U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xaaaaaaaaU);

    // Repeated 4 bit
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x11111111U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x33333333U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x0f0f0f0fU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xf0f0f0f0U);

    // Repeated 8 bit
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x1f1f1f1fU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xf8f8f8f8U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xff00ff00U);

    // Repeated 16 bit
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00ff00ffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xff0000ffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x0fff0fffU);

    // Wrapped
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xc0000001U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xe0000003U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xf000000fU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x80000001U);

    // Common masks
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xffff0000U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00ffffffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xffffff00U);

    // I think not
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000123U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00001234U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0xdeadbeefU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x13579bdfU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x12345678U);

    // Non contiguous
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000005U); // 0101
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000009U); // 1001
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000015U); // 10101

    // Tricky
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x81818181U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x01010101U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x03030303U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x7f7f7f7fU);

    // Failures
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x01020304U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x80402010U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x11223344U);
}

int test_is_logical_immediate_64_bit() {
    // All zeros or all ones
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0xffffffffffffffffULL);

    // Simple runs of ones
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000000000000001ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000000000000003ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x000000000000000fULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00000000000000ffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000000000000fffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00000000ffffffffULL);

    // High-bit runs
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x8000000000000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xf000000000000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xffff000000000000ULL);

    // // Sepeated 2-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x5555555555555555ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xaaaaaaaaaaaaaaaaULL);

    // // Repeated 4-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x1111111111111111ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x3333333333333333ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0f0f0f0f0f0f0f0fULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xf0f0f0f0f0f0f0f0ULL);

    // // Repeated 8-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x1f1f1f1f1f1f1f1fULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xf8f8f8f8f8f8f8f8ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xff00ff00ff00ff00ULL);

    // // Repeated 16-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00ff00ff00ff00ffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xff0000ffff0000ffULL);

    // // Repeated 32-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffff0000ffffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xffff0000ffff0000ULL);

    // // Full 64-bit masks
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00000000ffffffffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xffffffff00000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffffffff0000ULL);

    // // Wrapped runs (rotation required)
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xc000000000000001ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xe000000000000003ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xf00000000000000fULL);

    // Failures
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000123ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000001234ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x00000000deadbeefULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0123456789abcdefULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x13579bdf2468ace0ULL);

    // Almost right but not contiguous
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000005ULL); // 0101
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000009ULL); // 1001
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000015ULL); // 10101

    // Random sparse masks
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0001000100010001ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x8001000180010001ULL);

    // Common compiler-generated masks
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x000000000000ffffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00000000ffff0000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffff00000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00ffffff00000000ULL);
}

void test_instrsel_logical_instruction_with_constant(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // Test case where a constant cannot be encoded and must be moved into a register first
    si(function, 0, IR_BOR, vsz(3, TYPE_INT), vsz(1, TYPE_INT), ci(5));
    assert_long(AARCH64_OP_MOV_INT_CST, ir_start->operation.id);
    ir_start = ir_start->next;
    assert_target_op("orr         r2w, r1w, r3w");

    // binary or
    // signed v | c
    si(function, 0, IR_BOR, vsz(3, TYPE_CHAR),  vsz(1, TYPE_CHAR),  c(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vsz(3, TYPE_INT),   vsz(1, TYPE_INT),   c(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vsz(3, TYPE_LONG),  vsz(1, TYPE_LONG),  c(15)); assert_target_op("orr         r2x, r1x, 15");

    // unsigned v | c
    si(function, 0, IR_BOR, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  uc(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), uc(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   uc(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(15)); assert_target_op("orr         r2x, r1x, 15");

    // binary and
    // signed v & c
    si(function, 0, IR_BAND, vsz(3, TYPE_CHAR),  vsz(1, TYPE_CHAR),  c(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vsz(3, TYPE_INT),   vsz(1, TYPE_INT),   c(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vsz(3, TYPE_LONG),  vsz(1, TYPE_LONG),  c(15)); assert_target_op("and         r2x, r1x, 15");

    // unsigned v & c
    si(function, 0, IR_BAND, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  uc(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), uc(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   uc(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(15)); assert_target_op("and         r2x, r1x, 15");

    // exclusive or
    // signed v ^ c
    si(function, 0, IR_XOR, vsz(3, TYPE_CHAR),  vsz(1, TYPE_CHAR),  c(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vsz(3, TYPE_INT),   vsz(1, TYPE_INT),   c(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vsz(3, TYPE_LONG),  vsz(1, TYPE_LONG),  c(15)); assert_target_op("eor         r2x, r1x, 15");

    // unsigned v ^ c
    si(function, 0, IR_XOR, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  uc(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), uc(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   uc(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(15)); assert_target_op("eor         r2x, r1x, 15");
}

// Test storing an integer constant to a long in the stack.
// This ensures the rules are setup such that the constant isn't first loaded in a 32-bit register,
// then sign extended to 64-bit, then stored.
void test_constant_store_to_stack(void) {
    si(function, 0, IR_MOVE, Ssz(-1, TYPE_LONG), ci(1), 0);
    assert_long(AARCH64_OP_MOV_INT_CST, ir_start->operation.id);
    assert_long(TYPE_INT, ir_start->src1->type->type); // It starts of as an 32-bit integer ...
    assert_long(3, ir_start->src1->target_size);
    assert_long(4, ir_start->dst->target_size);  // ... and becomes 64-bit
    ir_start = ir_start->next;
    assert_target_op("str         r1x, [sp, 8]");
}

void test_int_cmp_with_conditional_jmp(Function *function, int cmp_operation, int jmp_operation, char *jmp_instruction) {
    char *template;

    start_ir();
    i(0, cmp_operation,  v(3), v(1), v(2));
    i(0, jmp_operation,  0,    v(3), l(1));
    i(1, IR_NOP,         0,    0,    0   );
    finish_ir(function);
    wasprintf(&template, "%-11s .L1", jmp_instruction);
    assert_target_op("cmp         r1x, r2x");
    assert_target_op(template);
}

void test_cmp_with_assignment(Function *function, int cmp_operation, char *set_instruction) {
    char *template;

    start_ir();
    i(0, cmp_operation, vsz(3, TYPE_INT), v(1), v(2));
    finish_ir(function);
    assert_target_op("cmp         r1x, r2x");
    assert_target_op(set_instruction);
}

void test_instrsel_conditionals() {
    long l;

    l = 4294967296;
    // JZ                                                              JNZ
    test_int_cmp_with_conditional_jmp(function, IR_EQ, IR_JNZ, "beq"); test_int_cmp_with_conditional_jmp(function, IR_EQ, IR_JZ, "bne");
    test_int_cmp_with_conditional_jmp(function, IR_NE, IR_JNZ, "bne"); test_int_cmp_with_conditional_jmp(function, IR_NE, IR_JZ, "beq");
    test_int_cmp_with_conditional_jmp(function, IR_LT, IR_JNZ, "blt"); test_int_cmp_with_conditional_jmp(function, IR_LT, IR_JZ, "bge");
    test_int_cmp_with_conditional_jmp(function, IR_GT, IR_JNZ, "bgt"); test_int_cmp_with_conditional_jmp(function, IR_GT, IR_JZ, "ble");
    test_int_cmp_with_conditional_jmp(function, IR_LE, IR_JNZ, "ble"); test_int_cmp_with_conditional_jmp(function, IR_LE, IR_JZ, "bgt");
    test_int_cmp_with_conditional_jmp(function, IR_GE, IR_JNZ, "bge"); test_int_cmp_with_conditional_jmp(function, IR_GE, IR_JZ, "blt");

    // TODO aarch64
    // test_fp_cmp_with_conditional_jmp(function, IR_LT, IR_JNZ, "ja" ); test_fp_cmp_with_conditional_jmp(function, IR_LT, IR_JZ, "jbe");
    // test_fp_cmp_with_conditional_jmp(function, IR_GT, IR_JNZ, "ja" ); test_fp_cmp_with_conditional_jmp(function, IR_GT, IR_JZ, "jbe");
    // test_fp_cmp_with_conditional_jmp(function, IR_LE, IR_JNZ, "jae"); test_fp_cmp_with_conditional_jmp(function, IR_LE, IR_JZ, "jb" );
    // test_fp_cmp_with_conditional_jmp(function, IR_GE, IR_JNZ, "jae"); test_fp_cmp_with_conditional_jmp(function, IR_GE, IR_JZ, "jb" );

    // Conditional assignment with 2 registers
    test_cmp_with_assignment(function, IR_EQ, "cset        r3w, eq");
    test_cmp_with_assignment(function, IR_NE, "cset        r3w, ne");
    test_cmp_with_assignment(function, IR_LT, "cset        r3w, lt");
    test_cmp_with_assignment(function, IR_GT, "cset        r3w, gt");
    test_cmp_with_assignment(function, IR_LE, "cset        r3w, le");
    test_cmp_with_assignment(function, IR_GE, "cset        r3w, ge");
}

int main() {
    int verbose;

    verbose = 0;
    failures = 0;

    init_memory_management_for_translation_unit();

    function = new_function("test");
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    opt_optimize_arithmetic_operations = 1;
    string_literals = malloc(MAX_STRING_LITERALS);

    init_allocate_registers();
    init_instruction_selection_rules();

    if (verbose) printf("Running instrsel test_instrsel_constant_bitshift\n");                  test_instrsel_constant_bitshift();
    if (verbose) printf("Running instrsel test_instrsel_add_sub_constant\n");                   test_instrsel_add_sub_constant();
    if (verbose) printf("Running instrsel test_is_logical_immediate_32_bit\n");                 test_is_logical_immediate_32_bit();
    if (verbose) printf("Running instrsel test_is_logical_immediate_64_bit\n");                 test_is_logical_immediate_64_bit();
    if (verbose) printf("Running instrsel test_instrsel_logical_instruction_with_constant\n");  test_instrsel_logical_instruction_with_constant();
    if (verbose) printf("Running instrsel test_constant_store_to_stack\n");                     test_constant_store_to_stack();
    if (verbose) printf("Running instrsel test_instrsel_conditionals\n");                       test_instrsel_conditionals();


    if (failures) {
        printf("%d tests failed\n", failures);
        exit(1);
    }
    printf("All tests succeeded\n");
}
