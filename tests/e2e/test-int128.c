#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

#define ASSERT_INT128(expected_low, expected_high, got, message) \
    assert_long(expected_low, (long) (got),   message " low"); \
    assert_long(expected_high, (got) >> 64,  message " high")

static void test_assignment_from_constant(void) {
    __int128 i;

    i = 1;                ASSERT_INT128(1, 0, i,    "int128 assignment from int");
    i = 2U;               ASSERT_INT128(2, 0, i,    "int128 assignment from unsigned int");
    i = 3L;               ASSERT_INT128(3, 0, i,    "int128 assignment from long");
    i = 4UL;              ASSERT_INT128(4, 0, i,    "int128 assignment from unsigned long");
    i = -42;              ASSERT_INT128(-42, -1, i, "int128 assignment from negative int");
    i = (int) 0xffffffff; ASSERT_INT128(-1, -1, i,  "int128 assignment from casted unsigned int");
}

static void test_assignment_from_variable(void) {
    __int128 j;

    char  c = 1; j = c; ASSERT_INT128(1, 0, j, "int128 assignment from char in variable");
    short s = 1; j = s; ASSERT_INT128(1, 0, j, "int128 assignment from short in variable");
    int   i = 1; j = i; ASSERT_INT128(1, 0, j, "int128 assignment from int in variable");
    long  l = 1; j = l; ASSERT_INT128(1, 0, j, "int128 assignment from long in variable");

    c = -1; j = c; ASSERT_INT128(-1, -1, j, "int128 assignment from negative char in variable");
    s = -1; j = s; ASSERT_INT128(-1, -1, j, "int128 assignment from negative short in variable");
    i = -1; j = i; ASSERT_INT128(-1, -1, j, "int128 assignment from negative int in variable");
    l = -1; j = l; ASSERT_INT128(-1, -1, j, "int128 assignment from negative long in variable");

    unsigned char  uc = 1; j = uc; ASSERT_INT128(1, 0, j, "int128 assignment from unsigned char in variable");
    unsigned short us = 1; j = us; ASSERT_INT128(1, 0, j, "int128 assignment from unsigned short in variable");
    unsigned int   ui = 1; j = ui; ASSERT_INT128(1, 0, j, "int128 assignment from unsigned int in variable");
    unsigned long  ul = 1; j = ul; ASSERT_INT128(1, 0, j, "int128 assignment from unsigned long in variable");
}

static void test_truncations(void) {
    __int128 i;
    unsigned __int128 ui;
    long l;

    i = (__int128) (3L << 63);  l =  i;  assert_long(1L << 63, l, "conversion of large int128 to long");
    i = -1;                     l =  i;  assert_long(-1, l, "conversion of -1 int128 to long");
    ui = (__int128) (3L << 63); l =  ui; assert_long(1L << 63, l, "conversion of unsigned int128 to long");

    i = -1;                     char c = i;  assert_int(-1, c, "conversion of -1 int128 to char");
}

static void test_bit_shifts(void) {
    __int128 i;

    // Right with constant
    i = (__int128) 1 << 96;
    ASSERT_INT128(0,        1L << 32, i >>  0, "1 << 96 >> 0");
    ASSERT_INT128(0,        1L << 31, i >>  1, "1 << 96 >> 1");
    ASSERT_INT128(0,        1L << 0,  i >> 32, "1 << 96 >> 32");
    ASSERT_INT128(1L << 63, 0,        i >> 33, "1 << 96 >> 33");
    ASSERT_INT128(1L << 33, 0,        i >> 63, "1 << 96 >> 63");
    ASSERT_INT128(1L << 32, 0,        i >> 64, "1 << 96 >> 64");
    ASSERT_INT128(1L << 31, 0,        i >> 65, "1 << 96 >> 65");
    ASSERT_INT128(1L << 30, 0,        i >> 66, "1 << 96 >> 66");
    ASSERT_INT128(1L << 26, 0,        i >> 70, "1 << 96 >> 70");
    ASSERT_INT128(1L << 22, 0,        i >> 74, "1 << 96 >> 74");
    ASSERT_INT128(1L << 12, 0,        i >> 84, "1 << 96 >> 84");
    ASSERT_INT128(1L << 2,  0,        i >> 94, "1 << 96 >> 94");
    ASSERT_INT128(1L << 1,  0,        i >> 95, "1 << 96 >> 96");
    ASSERT_INT128(1L << 0,  0,        i >> 96, "1 << 96 >> 96");

    i = -16;
    ASSERT_INT128(-16, -1, i >> 0, "-16 >> 0");
    ASSERT_INT128(-8,  -1, i >> 1, "-16 >> 1");
    ASSERT_INT128(-4,  -1, i >> 2, "-16 >> 2");

    i = ((__int128) (-2) << 64);
    ASSERT_INT128(0, -2,        i >> 0, "-big >> 0"); // 11...110 - 00000...00
    ASSERT_INT128(0, -1,        i >> 1, "-big >> 1"); // 11...111 - 00000...00
    ASSERT_INT128(1L << 63, -1, i >> 2, "-big >> 2"); // 11...111 - 10000...00
    ASSERT_INT128(3L << 62, -1, i >> 3, "-big >> 3"); // 11...111 - 11000...00
    ASSERT_INT128(7L << 61, -1, i >> 4, "-big >> 4"); // 11...111 - 11100...00

    // Unsigned right with constant
    unsigned __int128 ui = ((__int128) 3 << 126);
    ASSERT_INT128(0,        3L << 62, ui >>  0, "unsigned big >> 0");  // 11000...00 - 0000..00
    ASSERT_INT128(0,        3L << 61, ui >>  1, "unsigned big >> 1");  // 01100...00 - 0000..00
    ASSERT_INT128(0,        3,        ui >> 62, "unsigned big >> 62"); // 0000..0011 - 0000..00
    ASSERT_INT128(1L << 63, 1,        ui >> 63, "unsigned big >> 63"); // 0000..0001 - 1000..00
    ASSERT_INT128(3L << 62, 0,        ui >> 64, "unsigned big >> 64"); // 0000..0000 - 1100..00
    ASSERT_INT128(3L << 61, 0,        ui >> 65, "unsigned big >> 65"); // 0000..0000 - 0110..00

    // Left with constant
    i  = (__int128) 1;
    ASSERT_INT128(1L << 0,  0,        i <<  0, "1 << 0");
    ASSERT_INT128(1L << 1,  0,        i <<  1, "1 << 1");
    ASSERT_INT128(1L << 31, 0,        i << 31, "1 << 31");
    ASSERT_INT128(1L << 32, 0,        i << 32, "1 << 32");
    ASSERT_INT128(1L << 63, 0,        i << 63, "1 << 63");
    ASSERT_INT128(0,        1L << 0,  i << 64, "1 << 64");
    ASSERT_INT128(0,        1L << 1,  i << 65, "1 << 65");
    ASSERT_INT128(0,        1L << 2,  i << 66, "1 << 66");
    ASSERT_INT128(0,        1L << 6,  i << 70, "1 << 70");
    ASSERT_INT128(0,        1L << 10, i << 74, "1 << 74");
    ASSERT_INT128(0,        1L << 20, i << 84, "1 << 84");
    ASSERT_INT128(0,        1L << 30, i << 94, "1 << 94");
    ASSERT_INT128(0,        1L << 31, i << 95, "1 << 96");
    ASSERT_INT128(0,        1L << 32, i << 96, "1 << 96");

    i = 0xf1f2f3f4;
    ASSERT_INT128(0xf3f4000000000000L, 0x000000000000f1f2L, i << 48, "0xf1f2f3f4 << 48");

    // Left with register. One case is enough since the implementation is the same as the constant case
    i = (__int128) 1;
    __int128 r = 1;
    ASSERT_INT128(1L << 63, 0,        r << 63, "1 in register << 63");
}

static void test_binary_or(void) {
    __int128 i, j;

    i = 1;                  ASSERT_INT128(3, 0, i | 3, "(1 in r) | 2");
    i = (__int128) 1 << 64; ASSERT_INT128(2, 1, i | 2, "(1<<64 in r) | 2");

    i = (__int128) 1;       j = (__int128) 2;       ASSERT_INT128(3, 0, i | j, "1     | 2");
    i = (__int128) 1 << 64; j = (__int128) 2;       ASSERT_INT128(2, 1, i | j, "1<<64 | 2");
    i = (__int128) 1;       j = (__int128) 2 << 64; ASSERT_INT128(1, 2, i | j, "1     | 2<<64");
    i = (__int128) 1 << 64; j = (__int128) 2 << 64; ASSERT_INT128(0, 3, i | j, "1<<64 | 2<<64");

    unsigned __int128 ui, uj;
    ui = (unsigned __int128) 1 << 64; uj = (unsigned __int128) 2 << 64; ASSERT_INT128(0, 3, ui | uj, "unsigned 1<<64 | 2<<64");
}

static void test_binary_and(void) {
    __int128 i, j;

    i = 15;                  ASSERT_INT128(5, 0, i & 5,                  "(15 in r) & 5");
    i = (__int128) 15 << 64; ASSERT_INT128(0, 5, i & (__int128) 5 << 64, "(15<<64 in r) & 5");

    i = (__int128) 3;       j = (__int128) 7;       ASSERT_INT128(3, 0, i & j, "3     & 7");
    i = (__int128) 3 << 64; j = (__int128) 7;       ASSERT_INT128(0, 0, i & j, "3<<64 & 7");
    i = (__int128) 3;       j = (__int128) 7 << 64; ASSERT_INT128(0, 0, i & j, "3     & 7<<64");
    i = (__int128) 3 << 64; j = (__int128) 7 << 64; ASSERT_INT128(0, 3, i & j, "3<<64 & 7<<64");

    unsigned __int128 ui, uj;
    ui = (unsigned __int128) 3 << 64; uj = (unsigned __int128) 7 << 64; ASSERT_INT128(0, 3, ui & uj, "unsigned 3<<64 & 7<<64");
}

static void test_binary_xor(void) {
    __int128 i, j;

    i = 15;                  ASSERT_INT128(10, 0, i ^ 5,                  "(15 in r) ^ 5");
    i = (__int128) 15 << 64; ASSERT_INT128(0, 10, i ^ (__int128) 5 << 64, "(15<<64 in r) ^ 5");

    i = (__int128) 3;       j = (__int128) 7;       ASSERT_INT128(4, 0, i ^ j, "3     ^ 7");
    i = (__int128) 3 << 64; j = (__int128) 7;       ASSERT_INT128(7, 3, i ^ j, "3<<64 ^ 7");
    i = (__int128) 3;       j = (__int128) 7 << 64; ASSERT_INT128(3, 7, i ^ j, "3     ^ 7<<64");
    i = (__int128) 3 << 64; j = (__int128) 7 << 64; ASSERT_INT128(0, 4, i ^ j, "3<<64 ^ 7<<64");

    unsigned __int128 ui, uj;
    ui = (unsigned __int128) 3 << 64; uj = (unsigned __int128) 7 << 64; ASSERT_INT128(0, 4, ui ^ uj, "unsigned 3<<64 ^ 7<<64");
}

static void test_addition(void) {
    // Signed
    __int128 i, j;

    i = 2;                  j = 3;                  ASSERT_INT128(5, 0, i + j, "int128 2 + 3");
    i = (__int128) 1 << 63; j = (__int128) 1 << 63; ASSERT_INT128(0, 1, i + j, "int128 1<<63 + 1<<63");
    i = (__int128) 2 << 64; j = (__int128) 3 << 64; ASSERT_INT128(0, 5, i + j, "int128 2<<64 + 3<<64");

    // Test +1 with all low bits 1
    i = (unsigned long) -1; // 0x00..00 0xff.ff
    j = 1;
    ASSERT_INT128(0, 1, i + j, "int128 + with carry");

    // Unsigned
    __int128 ui, uj;

    ui = 2;                  uj = 3;                  ASSERT_INT128(5, 0, ui + uj, "unsigned int128 2 + 3");
    ui = (__int128) 1 << 63; uj = (__int128) 1 << 63; ASSERT_INT128(0, 1, ui + uj, "unsigned int128 1<<63 + 1<<63");
    ui = (__int128) 2 << 64; uj = (__int128) 3 << 64; ASSERT_INT128(0, 5, ui + uj, "unsigned int128 2<<64 + 3<<64");

    // Test +1 with all low bits 1
    ui = (unsigned long) -1; // 0x00..00 0xff.ff
    uj = 1;
    ASSERT_INT128(0, 1, ui + uj, "unsigned int128 + with carry");
}

static void test_subtraction(void) {
    // Signed
    __int128 i, j;

    i = 3;                  j = 2;                  ASSERT_INT128(1,        0, i - j, "int128 3 - 2");
    i = (__int128) 1 << 64; j = (__int128) 1 << 63; ASSERT_INT128(1L << 63, 0, i - j, "int128 1<<64 - 1<<63");
    i = (__int128) 1 << 64; j = 1;                  ASSERT_INT128(-1,       0, i - j, "int128 1<<64 - 1");

    // Unsigned
    unsigned __int128 ui, uj;

    ui = 3;                  uj = 2;                  ASSERT_INT128(1,        0, ui - uj, "unsigned int128 3 - 2");
    ui = (__int128) 1 << 64; uj = (__int128) 1 << 63; ASSERT_INT128(1L << 63, 0, ui - uj, "unsigned int128 1<<64 - 1<<63");
    ui = (__int128) 1 << 64; uj = 1;                  ASSERT_INT128(-1,       0, ui - uj, "unsigned int128 1<<64 - 1");
}

static void test_multiplication(void) {
    __int128 i, j;

    i = (__int128) 2;           j = (__int128) 3;            ASSERT_INT128(6,            0, i * j, "2 * 3");
    i = (__int128) 2 << 64;     j = (__int128) 3;            ASSERT_INT128(0,            6, i * j, "2<<64 * 3");
    i = (__int128) 2;           j = (__int128) 3 << 64;      ASSERT_INT128(0,            6, i * j, "2 * 3<<64");
    i = (__int128) 0x100000001; j = (__int128) 0x200000001;  ASSERT_INT128(0x300000001L, 2, i * j, "0x100000001 * 0x200000001");

    i = ((__int128) 0x1ffffffff) << 64 | 0x100000001;
    j = ((__int128) 0x2ffffffff) << 64 | 0x200000001;
    ASSERT_INT128(0x300000001L, 0x200000000L, i * j, "int128 big numbers multiply");

    unsigned __int128 ui, uj;
    ui = ((unsigned __int128) 0x1ffffffff) << 64 | 0x100000001;
    uj = ((unsigned __int128) 0x2ffffffff) << 64 | 0x200000001;
    ASSERT_INT128(0x300000001L, 0x200000000L, ui * uj, "unsigned int128 big numbers multiply");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv);

    test_assignment_from_constant();
    test_assignment_from_variable();
    test_truncations();
    test_bit_shifts();
    test_binary_or();
    test_binary_and();
    test_binary_xor();
    test_addition();
    test_subtraction();
    test_multiplication();

    finalize();
}
