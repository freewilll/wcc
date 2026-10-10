#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

static void test_assignment_from_constant(void) {
    __int128 si;

    si = 1;                assert_int128(0,  1,   si, "signed int128 in vreg assignment from int");
    si = 2U;               assert_int128(0,  2,   si, "signed int128 in vreg assignment from unsigned int");
    si = 3L;               assert_int128(0,  3,   si, "signed int128 in vreg assignment from long");
    si = 4UL;              assert_int128(0,  4,   si, "signed int128 in vreg assignment from unsigned long");
    si = -42;              assert_int128(-1, -42, si, "signed int128 in vreg assignment from negative int");
    si = (int) 0xffffffff; assert_int128(-1, -1,  si, "signed int128 in vreg assignment from casted unsigned int");

    unsigned __int128 ui;
    ui = 1;                assert_int128(0,  1,   ui, "unsigned int128 in vreg assignment from int");
    ui = 2U;               assert_int128(0,  2,   ui, "unsigned int128 in vreg assignment from unsigned int");
    ui = 3L;               assert_int128(0,  3,   ui, "unsigned int128 in vreg assignment from long");
    ui = 4UL;              assert_int128(0,  4,   ui, "unsigned int128 in vreg assignment from unsigned long");
    ui = -42;              assert_int128(-1, -42, ui, "unsigned int128 in vreg assignment from negative int");
    ui = (int) 0xffffffff; assert_int128(-1, -1,  ui, "unsigned int128 in vreg assignment from casted unsigned int");

    // This tests casting of signed int to an unsigned __int128
    ui = (int) 0xffffffff;
    assert_int128(-1, -1, ui, "unsigned int128 from (int) 0xffffffff");
}

static void test_stack_assignment_from_constant(void) {
    struct s { __int128 i, j; } s;

    s.i = 1;                assert_int128(0,  1,    s. i, "int128 in stack assignment from int");
    s.i = 2U;               assert_int128(0,  2,    s. i, "int128 in stack assignment from unsigned int");
    s.i = 3L;               assert_int128(0,  3,    s. i, "int128 in stack assignment from long");
    s.i = 4UL;              assert_int128(0,  4,    s. i, "int128 in stack assignment from unsigned long");
    s.i = -42;              assert_int128(-1, -42,  s. i, "int128 in stack assignment from negative int");
    s.i = (int) 0xffffffff; assert_int128(-1, -1,   s. i, "int128 in stack assignment from casted unsigned int");

    s.i = -1;
    assert_int128(-1, -1,  s. i, "int128 i is -1");

    s.j = 1;                assert_int128(0,  1,    s. j, "int128 in stack with offset assignment from int");
    s.j = 2U;               assert_int128(0,  2,    s. j, "int128 in stack with offset assignment from unsigned int");
    s.j = 3L;               assert_int128(0,  3,    s. j, "int128 in stack with offset assignment from long");
    s.j = 4UL;              assert_int128(0,  4,    s. j, "int128 in stack with offset assignment from unsigned long");
    s.j = -42;              assert_int128(-1, -42,  s. j, "int128 in stack with offset assignment from negative int");
    s.j = (int) 0xffffffff; assert_int128(-1, -1,   s. j, "int128 in stack with offset assignment from casted unsigned int");

    // Ensure s.i is still -1
    s.j = 42;
    assert_int128(-1, -1,  s. i, "int128 i is -1");

    // Ensure s.j is still 442 after assigning to s.i
    s.i = 0;
    assert_int128(0, 42,   s. j, "int128 j is 42");
}

static void test_assignment_from_variable(void) {
    __int128 j;

    signed char  c = 1; j = c; assert_int128(0, 1,  j, "int128 assignment from char in variable");
    signed short s = 1; j = s; assert_int128(0, 1,  j, "int128 assignment from short in variable");
    signed int   i = 1; j = i; assert_int128(0, 1,  j, "int128 assignment from int in variable");
    signed long  l = 1; j = l; assert_int128(0, 1,  j, "int128 assignment from long in variable");

    c = -1; j = c; assert_int128(-1, -1,  j, "int128 assignment from negative char in variable");
    s = -1; j = s; assert_int128(-1, -1,  j, "int128 assignment from negative short in variable");
    i = -1; j = i; assert_int128(-1, -1,  j, "int128 assignment from negative int in variable");
    l = -1; j = l; assert_int128(-1, -1,  j, "int128 assignment from negative long in variable");

    unsigned char  uc = 1; j = uc; assert_int128(0, 1,  j, "int128 assignment from unsigned char in variable");
    unsigned short us = 1; j = us; assert_int128(0, 1,  j, "int128 assignment from unsigned short in variable");
    unsigned int   ui = 1; j = ui; assert_int128(0, 1,  j, "int128 assignment from unsigned int in variable");
    unsigned long  ul = 1; j = ul; assert_int128(0, 1,  j, "int128 assignment from unsigned long in variable");
}

static void test_stack_assignment_from_variable(void) {
    struct s { __int128 i, j; } st;

    st.i = MAKE_SINT128(1, 2); assert_int128(1, 2,  st.i, "int128 in stack assignment from int128 in variable 1");
    st.j = MAKE_SINT128(3, 4); assert_int128(3, 4,  st.j, "int128 in stack assignment from int128 in variable 2");
                               assert_int128(1, 2,  st.i, "int128 in stack assignment from int128 in variable 3");
    st.i = MAKE_SINT128(5, 6); assert_int128(5, 6,  st.i, "int128 in stack assignment from int128 in variable 4");
                               assert_int128(3, 4,  st.j, "int128 in stack assignment from int128 in variable 5");

    signed char  c = 1; st.j = c; assert_int128(0, 1,  st.j, "int128 in stack assignment from char in variable");
    signed short s = 1; st.j = s; assert_int128(0, 1,  st.j, "int128 in stack assignment from short in variable");
    signed int   i = 1; st.j = i; assert_int128(0, 1,  st.j, "int128 in stack assignment from int in variable");
    signed long  l = 1; st.j = l; assert_int128(0, 1,  st.j, "int128 in stack assignment from long in variable");

    c = -1; st.j = c; assert_int128(-1, -1,  st.j, "int128 in stack assignment from negative char in variable");
    s = -1; st.j = s; assert_int128(-1, -1,  st.j, "int128 in stack assignment from negative short in variable");
    i = -1; st.j = i; assert_int128(-1, -1,  st.j, "int128 in stack assignment from negative int in variable");
    l = -1; st.j = l; assert_int128(-1, -1,  st.j, "int128 in stack assignment from negative long in variable");

    unsigned char  uc = 1; st.j = uc; assert_int128(0, 1,  st.j, "int128 in stack assignment from unsigned char in variable");
    unsigned short us = 1; st.j = us; assert_int128(0, 1,  st.j, "int128 in stack assignment from unsigned short in variable");
    unsigned int   ui = 1; st.j = ui; assert_int128(0, 1,  st.j, "int128 in stack assignment from unsigned int in variable");
    unsigned long  ul = 1; st.j = ul; assert_int128(0, 1,  st.j, "int128 in stack assignment from unsigned long in variable");
}

// Test int128 on the stack
static void test_stack_allocation(void) {
    __int128 i1 = (((__int128) 1) << 64) | 2;
    __int128 i2 = (((__int128) 1) << 64) | 2;
    &i1;
    &i2;

    // Ensure &i1 and &i2 don't overlap
    int distance = (void *) &i1 - (void *) &i2;
    if (distance < 0) distance = -distance;
    assert_int(1, distance >= 16, "Stack offsets of 1 and 2 are at least 16 bytes apart");
}

static void test_truncations(void) {
    // int128 in a vreg
    signed __int128 i;
    unsigned __int128 ui;
    long l;
    signed char c;

    i = (__int128) (3L << 63);  l =  i;  assert_long(1L << 63, l, "conversion of large int128 to long");
    i = -1;                     l =  i;  assert_long(-1,       l, "conversion of -1 int128 to long");
    ui = (__int128) (3L << 63); l =  ui; assert_long(1L << 63, l, "conversion of unsigned int128 to long");

    i = -1;
    c = i;
    assert_int(-1, c, "conversion of -1 int128 to char");

    // int128 in the stack
    struct s {
        signed __int128 i;
        unsigned __int128 ui;
    } st;

    st.i = (__int128) (3L << 63);  l =  st.i;  assert_long(1L << 63, l, "conversion of large int128 to long");
    st.i = -1;                     l =  st.i;  assert_long(-1,       l, "conversion of -1 int128 to long");
    st.ui = (__int128) (3L << 63); l =  st.ui; assert_long(1L << 63, l, "conversion of unsigned int128 to long");

    i = -1;
    c = st.i;
    assert_int(-1, c, "conversion of -1 int128 to char");

    // char in the stack
    signed char c2;
    &c2; // Force c2 onto the stack
    c2 = st.i;
    assert_int(-1, c2, "conversion of -1 int128 to char in stack");
}

static void test_bit_shifts_with_register_and_constant(void_and_register) {
    __int128 i;

    // Right with constant
    i = (__int128) 1 << 96;
    assert_int128(1L << 32, 0,  i >>  0, "1 << 96 >> 0");
    assert_int128(1L << 31, 0,  i >>  1, "1 << 96 >> 1");
    assert_int128(1L << 0,  0,  i >> 32, "1 << 96 >> 32");
    assert_int128(0, 1L <<  63, i >> 33, "1 << 96 >> 33");
    assert_int128(0, 1L <<  33, i >> 63, "1 << 96 >> 63");
    assert_int128(0, 1L <<  32, i >> 64, "1 << 96 >> 64");
    assert_int128(0, 1L <<  31, i >> 65, "1 << 96 >> 65");
    assert_int128(0, 1L <<  30, i >> 66, "1 << 96 >> 66");
    assert_int128(0, 1L <<  26, i >> 70, "1 << 96 >> 70");
    assert_int128(0, 1L <<  22, i >> 74, "1 << 96 >> 74");
    assert_int128(0, 1L <<  12, i >> 84, "1 << 96 >> 84");
    assert_int128(0, 1L <<  2,  i >> 94, "1 << 96 >> 94");
    assert_int128(0, 1L <<  1,  i >> 95, "1 << 96 >> 96");
    assert_int128(0, 1L <<  0,  i >> 96, "1 << 96 >> 96");

    i = -16;
    assert_int128(-1, -16, i >> 0, "-16 >> 0");
    assert_int128(-1, -8,  i >> 1, "-16 >> 1");
    assert_int128(-1, -4,  i >> 2, "-16 >> 2");

    i = ((__int128) (-2) << 64);
    assert_int128(-2, 0,         i >> 0, "-big >> 0"); // 11...110 - 00000...00
    assert_int128(-1, 0,         i >> 1, "-big >> 1"); // 11...111 - 00000...00
    assert_int128(-1, 1L << 63,  i >> 2, "-big >> 2"); // 11...111 - 10000...00
    assert_int128(-1, 3L << 62,  i >> 3, "-big >> 3"); // 11...111 - 11000...00
    assert_int128(-1, 7L << 61,  i >> 4, "-big >> 4"); // 11...111 - 11100...00

    // Unsigned right with constant
    unsigned __int128 ui = ((__int128) 3 << 126);
    assert_int128(3L << 62, 0,  ui >>  0, "unsigned big >> 0");  // 11000...00 - 0000..00
    assert_int128(3L << 61, 0,  ui >>  1, "unsigned big >> 1");  // 01100...00 - 0000..00
    assert_int128(3,        0,  ui >> 62, "unsigned big >> 62"); // 0000..0011 - 0000..00
    assert_int128(1, 1L <<  63, ui >> 63, "unsigned big >> 63"); // 0000..0001 - 1000..00
    assert_int128(0, 3L <<  62, ui >> 64, "unsigned big >> 64"); // 0000..0000 - 1100..00
    assert_int128(0, 3L <<  61, ui >> 65, "unsigned big >> 65"); // 0000..0000 - 0110..00

    // Left with constant
    i  = (__int128) 1;
    assert_int128(0, 1L <<  0,  i <<  0, "1 << 0");
    assert_int128(0, 1L <<  1,  i <<  1, "1 << 1");
    assert_int128(0, 1L <<  31, i << 31, "1 << 31");
    assert_int128(0, 1L <<  32, i << 32, "1 << 32");
    assert_int128(0, 1L <<  63, i << 63, "1 << 63");
    assert_int128(1L << 0,  0,  i << 64, "1 << 64");
    assert_int128(1L << 1,  0,  i << 65, "1 << 65");
    assert_int128(1L << 2,  0,  i << 66, "1 << 66");
    assert_int128(1L << 6,  0,  i << 70, "1 << 70");
    assert_int128(1L << 10, 0,  i << 74, "1 << 74");
    assert_int128(1L << 20, 0,  i << 84, "1 << 84");
    assert_int128(1L << 30, 0,  i << 94, "1 << 94");
    assert_int128(1L << 31, 0,  i << 95, "1 << 96");
    assert_int128(1L << 32, 0,  i << 96, "1 << 96");

    i = 0xf1f2f3f4;
    assert_int128(0x000000000000f1f2L, 0xf3f4000000000000L, i << 48, "0xf1f2f3f4 << 48");

    // Left with register. One case is enough since the implementation is the same as the constant case
    i = (__int128) 1;
    __int128 r = 1;
    assert_int128(0, 1L << 63, r << 63, "1 in register << 63");
}

static void test_bit_shifts_with_register_and_register(void_and_register) {
    __int128 i;
    int c;

    // Right with register
    i = (__int128) 1 << 96;
    c =  0; assert_int128(1L << 32, 0,  i >> c, "1 << 96 >> 0  in register");
    c =  1; assert_int128(1L << 31, 0,  i >> c, "1 << 96 >> 1  in register");
    c = 32; assert_int128(1L << 0,  0,  i >> c, "1 << 96 >> 32 in register");
    c = 33; assert_int128(0, 1L <<  63, i >> c, "1 << 96 >> 33 in register");
    c = 63; assert_int128(0, 1L <<  33, i >> c, "1 << 96 >> 63 in register");
    c = 64; assert_int128(0, 1L <<  32, i >> c, "1 << 96 >> 64 in register");
    c = 65; assert_int128(0, 1L <<  31, i >> c, "1 << 96 >> 65 in register");
    c = 66; assert_int128(0, 1L <<  30, i >> c, "1 << 96 >> 66 in register");
    c = 70; assert_int128(0, 1L <<  26, i >> c, "1 << 96 >> 70 in register");
    c = 74; assert_int128(0, 1L <<  22, i >> c, "1 << 96 >> 74 in register");
    c = 84; assert_int128(0, 1L <<  12, i >> c, "1 << 96 >> 84 in register");
    c = 94; assert_int128(0, 1L <<  2,  i >> c, "1 << 96 >> 94 in register");
    c = 95; assert_int128(0, 1L <<  1,  i >> c, "1 << 96 >> 96 in register");
    c = 96; assert_int128(0, 1L <<  0,  i >> c, "1 << 96 >> 96 in register");

    i = -16;
    c = 0; assert_int128(-1, -16, i >> c, "-16 >> 0 in register");
    c = 1; assert_int128(-1, -8,  i >> c, "-16 >> 1 in register");
    c = 2; assert_int128(-1, -4,  i >> c, "-16 >> 2 in register");

    i = ((__int128) (-2) << 64);
    c = 0; assert_int128(-2, 0,         i >> c, "-big >> 0 In register"); // 11...110 - 00000...00
    c = 1; assert_int128(-1, 0,         i >> c, "-big >> 1 In register"); // 11...111 - 00000...00
    c = 2; assert_int128(-1, 1L << 63,  i >> c, "-big >> 2 In register"); // 11...111 - 10000...00
    c = 3; assert_int128(-1, 3L << 62,  i >> c, "-big >> 3 In register"); // 11...111 - 11000...00
    c = 4; assert_int128(-1, 7L << 61,  i >> c, "-big >> 4 In register"); // 11...111 - 11100...00

    // Unsigned right with register
    unsigned __int128 ui = ((__int128) 3 << 126);
    c =  0; assert_int128(3L << 62, 0,  ui >> c, "unsigned big >> 0  in register");  // 11000...00 - 0000..00
    c =  1; assert_int128(3L << 61, 0,  ui >> c, "unsigned big >> 1  in register");  // 01100...00 - 0000..00
    c = 62; assert_int128(3,        0,  ui >> c, "unsigned big >> 62 in register"); // 0000..0011 - 0000..00
    c = 63; assert_int128(1, 1L <<  63, ui >> c, "unsigned big >> 63 in register"); // 0000..0001 - 1000..00
    c = 64; assert_int128(0, 3L <<  62, ui >> c, "unsigned big >> 64 in register"); // 0000..0000 - 1100..00
    c = 65; assert_int128(0, 3L <<  61, ui >> c, "unsigned big >> 65 in register"); // 0000..0000 - 0110..00

    // Left with register
    i  = (__int128) 1;
    c =  0; assert_int128(0, 1L <<  0,  i << c, "1 << 0  in register");
    c =  1; assert_int128(0, 1L <<  1,  i << c, "1 << 1  in register");
    c = 31; assert_int128(0, 1L <<  31, i << c, "1 << 31 in register");
    c = 32; assert_int128(0, 1L <<  32, i << c, "1 << 32 in register");
    c = 63; assert_int128(0, 1L <<  63, i << c, "1 << 63 in register");
    c = 64; assert_int128(1L << 0,  0,  i << c, "1 << 64 in register");
    c = 65; assert_int128(1L << 1,  0,  i << c, "1 << 65 in register");
    c = 66; assert_int128(1L << 2,  0,  i << c, "1 << 66 in register");
    c = 70; assert_int128(1L << 6,  0,  i << c, "1 << 70 in register");
    c = 74; assert_int128(1L << 10, 0,  i << c, "1 << 74 in register");
    c = 84; assert_int128(1L << 20, 0,  i << c, "1 << 84 in register");
    c = 94; assert_int128(1L << 30, 0,  i << c, "1 << 94 in register");
    c = 95; assert_int128(1L << 31, 0,  i << c, "1 << 96 in register");
    c = 96; assert_int128(1L << 32, 0,  i << c, "1 << 96 in register");

    i = 0xf1f2f3f4;
    c = 48; assert_int128(0x000000000000f1f2L, 0xf3f4000000000000L, i << c, "0xf1f2f3f4 << 48 in register");

    // Left with register. One case is enough since the implementation is the same as the constant case
    i = (__int128) 1;
    __int128 r = 1;
    c = 63;
    assert_int128(0, 1L << 63, r << c, "1 in register << 63 in register");

    // Right with unsigned register
    unsigned int uc;
    ui = (unsigned __int128) 1 << 96;
    uc =  0; assert_int128(1L << 32, 0,  ui >> uc, "unsigned 1 << 96 >> 0 in unsigned register");
}

// This test isn't exhaustive, since the most of it is already handled by the above tests.
static void test_bit_shifts_with_constant_and_register(void_and_register) {
    __uint128_t ui;
    int c;

    c = 1;  ui = ((__uint128_t) 1) << c; assert_int128(0,2, ui, "Constant (int128) 1 << 1  in register");
    c = 65; ui = ((__uint128_t) 1) << c; assert_int128(2,0, ui, "Constant (int128) 1 << 65 in register");
}

static void test_binary_or(void) {
    __int128 i, j;

    i = 1;                  assert_int128(0, 3,  i | 3, "(1 in r) | 2");
    i = (__int128) 1 << 64; assert_int128(1, 2,  i | 2, "(1<<64 in r) | 2");

    i = (__int128) 1;       j = (__int128) 2;       assert_int128(0, 3,  i | j, "1     | 2");
    i = (__int128) 1 << 64; j = (__int128) 2;       assert_int128(1, 2,  i | j, "1<<64 | 2");
    i = (__int128) 1;       j = (__int128) 2 << 64; assert_int128(2, 1,  i | j, "1     | 2<<64");
    i = (__int128) 1 << 64; j = (__int128) 2 << 64; assert_int128(3, 0,  i | j, "1<<64 | 2<<64");

    unsigned __int128 ui, uj;
    ui = (unsigned __int128) 1 << 64; uj = (unsigned __int128) 2 << 64; assert_int128(3, 0,  ui | uj, "unsigned 1<<64 | 2<<64");
}

static void test_binary_and(void) {
    __int128 i, j;

    i = 15;                  assert_int128(0, 5,  i & 5,                  "(15 in r) & 5");
    i = (__int128) 15 << 64; assert_int128(5, 0,  i & (__int128) 5 << 64, "(15<<64 in r) & 5");

    i = (__int128) 3;       j = (__int128) 7;       assert_int128(0, 3,  i & j, "3     & 7");
    i = (__int128) 3 << 64; j = (__int128) 7;       assert_int128(0, 0,  i & j, "3<<64 & 7");
    i = (__int128) 3;       j = (__int128) 7 << 64; assert_int128(0, 0,  i & j, "3     & 7<<64");
    i = (__int128) 3 << 64; j = (__int128) 7 << 64; assert_int128(3, 0,  i & j, "3<<64 & 7<<64");

    unsigned __int128 ui, uj;
    ui = (unsigned __int128) 3 << 64; uj = (unsigned __int128) 7 << 64; assert_int128(3, 0,  ui & uj, "unsigned 3<<64 & 7<<64");
}

static void test_binary_xor(void) {
    __int128 i, j;

    i = 15;                  assert_int128(0, 10,  i ^ 5,                  "(15 in r) ^ 5");
    i = (__int128) 15 << 64; assert_int128(10, 0,  i ^ (__int128) 5 << 64, "(15<<64 in r) ^ 5");

    i = (__int128) 3;       j = (__int128) 7;       assert_int128(0, 4,  i ^ j, "3     ^ 7");
    i = (__int128) 3 << 64; j = (__int128) 7;       assert_int128(3, 7,  i ^ j, "3<<64 ^ 7");
    i = (__int128) 3;       j = (__int128) 7 << 64; assert_int128(7, 3,  i ^ j, "3     ^ 7<<64");
    i = (__int128) 3 << 64; j = (__int128) 7 << 64; assert_int128(4, 0,  i ^ j, "3<<64 ^ 7<<64");

    unsigned __int128 ui, uj;
    ui = (unsigned __int128) 3 << 64; uj = (unsigned __int128) 7 << 64; assert_int128(4, 0,  ui ^ uj, "unsigned 3<<64 ^ 7<<64");
}

static void test_addition(void) {
    // Signed
    __int128 i, j;

    i = 2;                  j = 3;                  assert_int128(0, 5,  i + j, "int128 2 + 3");
    i = (__int128) 1 << 63; j = (__int128) 1 << 63; assert_int128(1, 0,  i + j, "int128 1<<63 + 1<<63");
    i = (__int128) 2 << 64; j = (__int128) 3 << 64; assert_int128(5, 0,  i + j, "int128 2<<64 + 3<<64");

    // Test +1 with all low bits 1
    i = (unsigned long) -1; // 0x00..00 0xff.ff
    j = 1;
    assert_int128(1, 0,  i + j, "int128 + with carry");

    // Unsigned
    __int128 ui, uj;

    ui = 2;                  uj = 3;                  assert_int128(0, 5,  ui + uj, "unsigned int128 2 + 3");
    ui = (__int128) 1 << 63; uj = (__int128) 1 << 63; assert_int128(1, 0,  ui + uj, "unsigned int128 1<<63 + 1<<63");
    ui = (__int128) 2 << 64; uj = (__int128) 3 << 64; assert_int128(5, 0,  ui + uj, "unsigned int128 2<<64 + 3<<64");

    // Test +1 with all low bits 1
    ui = (unsigned long) -1; // 0x00..00 0xff.ff
    uj = 1;
    assert_int128(1, 0,  ui + uj, "unsigned int128 + with carry");
}

static void test_subtraction(void) {
    // Signed
    __int128 i, j;

    i = 3;                  j = 2;                  assert_int128(0, 1,         i - j, "int128 3 - 2");
    i = (__int128) 1 << 64; j = (__int128) 1 << 63; assert_int128(0, 1L << 63,  i - j, "int128 1<<64 - 1<<63");
    i = (__int128) 1 << 64; j = 1;                  assert_int128(0, -1,        i - j, "int128 1<<64 - 1");

    // Unsigned
    unsigned __int128 ui, uj;

    ui = 3;                  uj = 2;                  assert_int128(0, 1,         ui - uj, "unsigned int128 3 - 2");
    ui = (__int128) 1 << 64; uj = (__int128) 1 << 63; assert_int128(0, 1L << 63,  ui - uj, "unsigned int128 1<<64 - 1<<63");
    ui = (__int128) 1 << 64; uj = 1;                  assert_int128(0, -1,        ui - uj, "unsigned int128 1<<64 - 1");
}

static void test_multiplication(void) {
    __int128 i, j;

    i = (__int128) 2;           j = (__int128) 3;           assert_int128(0, 6,  i * j, "2 * 3");
    i = (__int128) 2 << 64;     j = (__int128) 3;           assert_int128(6, 0,  i * j, "2<<64 * 3");
    i = (__int128) 2;           j = (__int128) 3 << 64;     assert_int128(6, 0,  i * j, "2 * 3<<64");
    i = (__int128) 0x100000001; j = (__int128) 0x200000001; assert_int128(2, 0x300000001L,  i * j, "0x100000001 * 0x200000001");

    i = ((__int128) 0x1ffffffff) << 64 | 0x100000001;
    j = ((__int128) 0x2ffffffff) << 64 | 0x200000001;
    assert_int128(0x200000000L, 0x300000001L,  i * j, "int128 big numbers multiply");

    unsigned __int128 ui, uj;
    ui = ((unsigned __int128) 0x1ffffffff) << 64 | 0x100000001;
    uj = ((unsigned __int128) 0x2ffffffff) << 64 | 0x200000001;
    assert_int128(0x200000000L, 0x300000001L,  ui * uj, "unsigned int128 big numbers multiply");
}

// This tests an odd parser edge case where, since negation isn't implemented,
// a multiple by -1 instruction is added by the parser.
static void test_negation() {
    signed   __int128 si = 2; si = - si; assert_int128(-1, -2, si, "Negation of signed __int128");
    unsigned __int128 ui = 2; ui = - ui; assert_int128(-1, -2, ui, "Negation of unsigned __int128");
}

static void test_comparisons(void) {
    signed __int128 si, sj;
    unsigned __int128 ui, uj;

    unsigned long v7f = 0x7fffffffffffffffUL;
    unsigned long v80 = 0x8000000000000000UL;
    unsigned long vfe = 0xfffffffffffffffeUL;
    unsigned long vff = 0xffffffffffffffffUL;

    // Signed ==
    // ==
    si = MAKE_SINT128(2, 1); sj = MAKE_SINT128(2, 1); assert_int(1, si == sj, "2..1 == 2..1");
    si = MAKE_SINT128(2, 2); sj = MAKE_SINT128(2, 1); assert_int(0, si == sj, "2..2 == 2..1");
    si = MAKE_SINT128(2, 1); sj = MAKE_SINT128(2, 2); assert_int(0, si == sj, "2..1 == 2..2");
    si = MAKE_SINT128(1, 1); sj = MAKE_SINT128(2, 1); assert_int(0, si == sj, "1..1 == 2..1");
    si = MAKE_SINT128(2, 1); sj = MAKE_SINT128(1, 1); assert_int(0, si == sj, "2..1 == 1..1");

    // Signed !=
    si = MAKE_SINT128(2, 1); sj = MAKE_SINT128(2, 1); assert_int(0, si != sj, "2..1 != 2..1");
    si = MAKE_SINT128(2, 2); sj = MAKE_SINT128(2, 1); assert_int(1, si != sj, "2..2 != 2..1");
    si = MAKE_SINT128(2, 1); sj = MAKE_SINT128(2, 2); assert_int(1, si != sj, "2..1 != 2..2");
    si = MAKE_SINT128(1, 1); sj = MAKE_SINT128(2, 1); assert_int(1, si != sj, "1..1 != 2..1");
    si = MAKE_SINT128(2, 1); sj = MAKE_SINT128(1, 1); assert_int(1, si != sj, "2..1 != 1..1");

    // Unsigned ==
    ui = MAKE_UINT128(2, 1); uj = MAKE_UINT128(2, 1); assert_int(1, ui == uj, "2..1 == 2..1");
    ui = MAKE_UINT128(2, 2); uj = MAKE_UINT128(2, 1); assert_int(0, ui == uj, "2..2 == 2..1");
    ui = MAKE_UINT128(2, 1); uj = MAKE_UINT128(2, 2); assert_int(0, ui == uj, "2..1 == 2..2");
    ui = MAKE_UINT128(1, 1); uj = MAKE_UINT128(2, 1); assert_int(0, ui == uj, "1..1 == 2..1");
    ui = MAKE_UINT128(2, 1); uj = MAKE_UINT128(1, 1); assert_int(0, ui == uj, "2..1 == 1..1");

    // Unsigned !=
    ui = MAKE_UINT128(2, 1); uj = MAKE_UINT128(2, 1); assert_int(0, ui != uj, "2..1 != 2..1");
    ui = MAKE_UINT128(2, 2); uj = MAKE_UINT128(2, 1); assert_int(1, ui != uj, "2..2 != 2..1");
    ui = MAKE_UINT128(2, 1); uj = MAKE_UINT128(2, 2); assert_int(1, ui != uj, "2..1 != 2..2");
    ui = MAKE_UINT128(1, 1); uj = MAKE_UINT128(2, 1); assert_int(1, ui != uj, "1..1 != 2..1");
    ui = MAKE_UINT128(2, 1); uj = MAKE_UINT128(1, 1); assert_int(1, ui != uj, "2..1 != 1..1");

    // Signed <
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(1,   1);   assert_int(0, si < sj, "signed 0101 < 0101");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(1,   2);   assert_int(1, si < sj, "signed 0101 < 0102");
    si = MAKE_SINT128(1,   2);   sj = MAKE_SINT128(1,   1);   assert_int(0, si < sj, "signed 0102 < 0101");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(2,   1);   assert_int(1, si < sj, "signed 0101 < 0201");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(2,   2);   assert_int(1, si < sj, "signed 0101 < 0202");
    si = MAKE_SINT128(1,   2);   sj = MAKE_SINT128(2,   1);   assert_int(1, si < sj, "signed 0102 < 0201");
    si = MAKE_SINT128(2,   1);   sj = MAKE_SINT128(1,   1);   assert_int(0, si < sj, "signed 0201 < 0101");
    si = MAKE_SINT128(2,   1);   sj = MAKE_SINT128(1,   2);   assert_int(0, si < sj, "signed 0201 < 0102");
    si = MAKE_SINT128(2,   2);   sj = MAKE_SINT128(1,   1);   assert_int(0, si < sj, "signed 0202 < 0101");
    si = MAKE_SINT128(1,   0);   sj = MAKE_SINT128(1,   vff); assert_int(1, si < sj, "signed 0100 < 01ff");
    si = MAKE_SINT128(1,   vff); sj = MAKE_SINT128(1,   0);   assert_int(0, si < sj, "signed 01ff < 0100");
    si = MAKE_SINT128(0,   vff); sj = MAKE_SINT128(1,   0);   assert_int(1, si < sj, "signed 01ff < 0100");
    si = MAKE_SINT128(1,   0);   sj = MAKE_SINT128(0,   vff); assert_int(0, si < sj, "signed 0100 < 00ff");
    si = MAKE_SINT128(v7f, vff); sj = MAKE_SINT128(v80, 0);   assert_int(0, si < sj, "signed 7fff < 8000");
    si = MAKE_SINT128(v80, 0);   sj = MAKE_SINT128(v7f, vff); assert_int(1, si < sj, "signed 8000 < 7fff");
    si = MAKE_SINT128(vff, vfe); sj = MAKE_SINT128(vff, vff); assert_int(1, si < sj, "signed fffe < ffff");
    si = MAKE_SINT128(vff, vff); sj = MAKE_SINT128(vff, vff); assert_int(0, si < sj, "signed ffff < ffff");

    // Unsigned <
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(1,   1);   assert_int(0, ui < uj, "unsigned 0101 < 0101");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(1,   2);   assert_int(1, ui < uj, "unsigned 0101 < 0102");
    ui = MAKE_UINT128(1,   2);   uj = MAKE_UINT128(1,   1);   assert_int(0, ui < uj, "unsigned 0102 < 0101");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(2,   1);   assert_int(1, ui < uj, "unsigned 0101 < 0201");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(2,   2);   assert_int(1, ui < uj, "unsigned 0101 < 0202");
    ui = MAKE_UINT128(1,   2);   uj = MAKE_UINT128(2,   1);   assert_int(1, ui < uj, "unsigned 0102 < 0201");
    ui = MAKE_UINT128(2,   1);   uj = MAKE_UINT128(1,   1);   assert_int(0, ui < uj, "unsigned 0201 < 0101");
    ui = MAKE_UINT128(2,   1);   uj = MAKE_UINT128(1,   2);   assert_int(0, ui < uj, "unsigned 0201 < 0102");
    ui = MAKE_UINT128(2,   2);   uj = MAKE_UINT128(1,   1);   assert_int(0, ui < uj, "unsigned 0202 < 0101");
    ui = MAKE_UINT128(1,   0);   uj = MAKE_UINT128(1,   vff); assert_int(1, ui < uj, "unsigned 0100 < 01ff");
    ui = MAKE_UINT128(1,   vff); uj = MAKE_UINT128(1,   0);   assert_int(0, ui < uj, "unsigned 01ff < 0100");
    ui = MAKE_UINT128(0,   vff); uj = MAKE_UINT128(1,   0);   assert_int(1, ui < uj, "unsigned 01ff < 0100");
    ui = MAKE_UINT128(1,   0);   uj = MAKE_UINT128(0,   vff); assert_int(0, ui < uj, "unsigned 0100 < 00ff");
    ui = MAKE_UINT128(v7f, vff); uj = MAKE_UINT128(v80, 0);   assert_int(1, ui < uj, "unsigned 7fff < 8000");
    ui = MAKE_UINT128(v80, 0);   uj = MAKE_UINT128(v7f, vff); assert_int(0, ui < uj, "unsigned 8000 < 7fff");
    ui = MAKE_SINT128(vff, vfe); sj = MAKE_SINT128(vff, vff); assert_int(0, ui < uj, "unsigned fffe < ffff");
    ui = MAKE_UINT128(vff, vff); uj = MAKE_UINT128(vff, vff); assert_int(0, ui < uj, "unsigned ffff < ffff");

    // Signed >=
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(1,   1);   assert_int(1, si >= sj, "signed 0101 >= 0101");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(1,   2);   assert_int(0, si >= sj, "signed 0101 >= 0102");
    si = MAKE_SINT128(1,   2);   sj = MAKE_SINT128(1,   1);   assert_int(1, si >= sj, "signed 0102 >= 0101");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(2,   1);   assert_int(0, si >= sj, "signed 0101 >= 0201");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(2,   2);   assert_int(0, si >= sj, "signed 0101 >= 0202");
    si = MAKE_SINT128(1,   2);   sj = MAKE_SINT128(2,   1);   assert_int(0, si >= sj, "signed 0102 >= 0201");
    si = MAKE_SINT128(2,   1);   sj = MAKE_SINT128(1,   1);   assert_int(1, si >= sj, "signed 0201 >= 0101");
    si = MAKE_SINT128(2,   1);   sj = MAKE_SINT128(1,   2);   assert_int(1, si >= sj, "signed 0201 >= 0102");
    si = MAKE_SINT128(2,   2);   sj = MAKE_SINT128(1,   1);   assert_int(1, si >= sj, "signed 0202 >= 0101");
    si = MAKE_SINT128(1,   0);   sj = MAKE_SINT128(1,   vff); assert_int(0, si >= sj, "signed 0100 >= 01ff");
    si = MAKE_SINT128(1,   vff); sj = MAKE_SINT128(1,   0);   assert_int(1, si >= sj, "signed 01ff >= 0100");
    si = MAKE_SINT128(0,   vff); sj = MAKE_SINT128(1,   0);   assert_int(0, si >= sj, "signed 01ff >= 0100");
    si = MAKE_SINT128(1,   0);   sj = MAKE_SINT128(0,   vff); assert_int(1, si >= sj, "signed 0100 >= 00ff");
    si = MAKE_SINT128(v7f, vff); sj = MAKE_SINT128(v80, 0);   assert_int(1, si >= sj, "signed 7fff >= 8000");
    si = MAKE_SINT128(v80, 0);   sj = MAKE_SINT128(v7f, vff); assert_int(0, si >= sj, "signed 8000 >= 7fff");
    si = MAKE_SINT128(vff, vfe); sj = MAKE_SINT128(vff, vff); assert_int(0, si >= sj, "signed fffe >= ffff");
    si = MAKE_SINT128(vff, vff); sj = MAKE_SINT128(vff, vff); assert_int(1, si >= sj, "signed ffff >= ffff");

    // Unsigned >=
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(1,   1);   assert_int(1, ui >= uj, "unsigned 0101 >= 0101");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(1,   2);   assert_int(0, ui >= uj, "unsigned 0101 >= 0102");
    ui = MAKE_UINT128(1,   2);   uj = MAKE_UINT128(1,   1);   assert_int(1, ui >= uj, "unsigned 0102 >= 0101");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(2,   1);   assert_int(0, ui >= uj, "unsigned 0101 >= 0201");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(2,   2);   assert_int(0, ui >= uj, "unsigned 0101 >= 0202");
    ui = MAKE_UINT128(1,   2);   uj = MAKE_UINT128(2,   1);   assert_int(0, ui >= uj, "unsigned 0102 >= 0201");
    ui = MAKE_UINT128(2,   1);   uj = MAKE_UINT128(1,   1);   assert_int(1, ui >= uj, "unsigned 0201 >= 0101");
    ui = MAKE_UINT128(2,   1);   uj = MAKE_UINT128(1,   2);   assert_int(1, ui >= uj, "unsigned 0201 >= 0102");
    ui = MAKE_UINT128(2,   2);   uj = MAKE_UINT128(1,   1);   assert_int(1, ui >= uj, "unsigned 0202 >= 0101");
    ui = MAKE_UINT128(1,   0);   uj = MAKE_UINT128(1,   vff); assert_int(0, ui >= uj, "unsigned 0100 >= 01ff");
    ui = MAKE_UINT128(1,   vff); uj = MAKE_UINT128(1,   0);   assert_int(1, ui >= uj, "unsigned 01ff >= 0100");
    ui = MAKE_UINT128(0,   vff); uj = MAKE_UINT128(1,   0);   assert_int(0, ui >= uj, "unsigned 01ff >= 0100");
    ui = MAKE_UINT128(1,   0);   uj = MAKE_UINT128(0,   vff); assert_int(1, ui >= uj, "unsigned 0100 >= 00ff");
    ui = MAKE_UINT128(v7f, vff); uj = MAKE_UINT128(v80, 0);   assert_int(0, ui >= uj, "unsigned 7fff >= 8000");
    ui = MAKE_UINT128(v80, 0);   uj = MAKE_UINT128(v7f, vff); assert_int(1, ui >= uj, "unsigned 8000 >= 7fff");
    ui = MAKE_SINT128(vff, vfe); sj = MAKE_SINT128(vff, vff); assert_int(1, ui >= uj, "unsigned fffe >= ffff");
    ui = MAKE_UINT128(vff, vff); uj = MAKE_UINT128(vff, vff); assert_int(1, ui >= uj, "unsigned ffff >= ffff");

    // Signed >
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(1,   1);   assert_int(0, si > sj, "signed 0101 > 0101");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(1,   2);   assert_int(0, si > sj, "signed 0101 > 0102");
    si = MAKE_SINT128(1,   2);   sj = MAKE_SINT128(1,   1);   assert_int(1, si > sj, "signed 0102 > 0101");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(2,   1);   assert_int(0, si > sj, "signed 0101 > 0201");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(2,   2);   assert_int(0, si > sj, "signed 0101 > 0202");
    si = MAKE_SINT128(1,   2);   sj = MAKE_SINT128(2,   1);   assert_int(0, si > sj, "signed 0102 > 0201");
    si = MAKE_SINT128(2,   1);   sj = MAKE_SINT128(1,   1);   assert_int(1, si > sj, "signed 0201 > 0101");
    si = MAKE_SINT128(2,   1);   sj = MAKE_SINT128(1,   2);   assert_int(1, si > sj, "signed 0201 > 0102");
    si = MAKE_SINT128(2,   2);   sj = MAKE_SINT128(1,   1);   assert_int(1, si > sj, "signed 0202 > 0101");
    si = MAKE_SINT128(1,   0);   sj = MAKE_SINT128(1,   vff); assert_int(0, si > sj, "signed 0100 > 01ff");
    si = MAKE_SINT128(1,   vff); sj = MAKE_SINT128(1,   0);   assert_int(1, si > sj, "signed 01ff > 0100");
    si = MAKE_SINT128(0,   vff); sj = MAKE_SINT128(1,   0);   assert_int(0, si > sj, "signed 01ff > 0100");
    si = MAKE_SINT128(1,   0);   sj = MAKE_SINT128(0,   vff); assert_int(1, si > sj, "signed 0100 > 00ff");
    si = MAKE_SINT128(v7f, vff); sj = MAKE_SINT128(v80, 0);   assert_int(1, si > sj, "signed 7fff > 8000");
    si = MAKE_SINT128(v80, 0);   sj = MAKE_SINT128(v7f, vff); assert_int(0, si > sj, "signed 8000 > 7fff");
    si = MAKE_SINT128(vff, vfe); sj = MAKE_SINT128(vff, vff); assert_int(0, si > sj, "signed fffe > ffff");
    si = MAKE_SINT128(vff, vff); sj = MAKE_SINT128(vff, vff); assert_int(0, si > sj, "signed ffff > ffff");

    // Unsigned >
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(1,   1);   assert_int(0, ui > uj, "unsigned 0101 > 0101");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(1,   2);   assert_int(0, ui > uj, "unsigned 0101 > 0102");
    ui = MAKE_UINT128(1,   2);   uj = MAKE_UINT128(1,   1);   assert_int(1, ui > uj, "unsigned 0102 > 0101");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(2,   1);   assert_int(0, ui > uj, "unsigned 0101 > 0201");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(2,   2);   assert_int(0, ui > uj, "unsigned 0101 > 0202");
    ui = MAKE_UINT128(1,   2);   uj = MAKE_UINT128(2,   1);   assert_int(0, ui > uj, "unsigned 0102 > 0201");
    ui = MAKE_UINT128(2,   1);   uj = MAKE_UINT128(1,   1);   assert_int(1, ui > uj, "unsigned 0201 > 0101");
    ui = MAKE_UINT128(2,   1);   uj = MAKE_UINT128(1,   2);   assert_int(1, ui > uj, "unsigned 0201 > 0102");
    ui = MAKE_UINT128(2,   2);   uj = MAKE_UINT128(1,   1);   assert_int(1, ui > uj, "unsigned 0202 > 0101");
    ui = MAKE_UINT128(1,   0);   uj = MAKE_UINT128(1,   vff); assert_int(0, ui > uj, "unsigned 0100 > 01ff");
    ui = MAKE_UINT128(1,   vff); uj = MAKE_UINT128(1,   0);   assert_int(1, ui > uj, "unsigned 01ff > 0100");
    ui = MAKE_UINT128(0,   vff); uj = MAKE_UINT128(1,   0);   assert_int(0, ui > uj, "unsigned 01ff > 0100");
    ui = MAKE_UINT128(1,   0);   uj = MAKE_UINT128(0,   vff); assert_int(1, ui > uj, "unsigned 0100 > 00ff");
    ui = MAKE_UINT128(v7f, vff); uj = MAKE_UINT128(v80, 0);   assert_int(0, ui > uj, "unsigned 7fff > 8000");
    ui = MAKE_UINT128(v80, 0);   uj = MAKE_UINT128(v7f, vff); assert_int(1, ui > uj, "unsigned 8000 > 7fff");
    ui = MAKE_SINT128(vff, vfe); sj = MAKE_SINT128(vff, vff); assert_int(1, ui > uj, "unsigned fffe > ffff");
    ui = MAKE_UINT128(vff, vff); uj = MAKE_UINT128(vff, vff); assert_int(0, ui > uj, "unsigned ffff > ffff");

    // Signed <=
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(1,   1);   assert_int(1, si <= sj, "signed 0101 <= 0101");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(1,   2);   assert_int(1, si <= sj, "signed 0101 <= 0102");
    si = MAKE_SINT128(1,   2);   sj = MAKE_SINT128(1,   1);   assert_int(0, si <= sj, "signed 0102 <= 0101");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(2,   1);   assert_int(1, si <= sj, "signed 0101 <= 0201");
    si = MAKE_SINT128(1,   1);   sj = MAKE_SINT128(2,   2);   assert_int(1, si <= sj, "signed 0101 <= 0202");
    si = MAKE_SINT128(1,   2);   sj = MAKE_SINT128(2,   1);   assert_int(1, si <= sj, "signed 0102 <= 0201");
    si = MAKE_SINT128(2,   1);   sj = MAKE_SINT128(1,   1);   assert_int(0, si <= sj, "signed 0201 <= 0101");
    si = MAKE_SINT128(2,   1);   sj = MAKE_SINT128(1,   2);   assert_int(0, si <= sj, "signed 0201 <= 0102");
    si = MAKE_SINT128(2,   2);   sj = MAKE_SINT128(1,   1);   assert_int(0, si <= sj, "signed 0202 <= 0101");
    si = MAKE_SINT128(1,   0);   sj = MAKE_SINT128(1,   vff); assert_int(1, si <= sj, "signed 0100 <= 01ff");
    si = MAKE_SINT128(1,   vff); sj = MAKE_SINT128(1,   0);   assert_int(0, si <= sj, "signed 01ff <= 0100");
    si = MAKE_SINT128(0,   vff); sj = MAKE_SINT128(1,   0);   assert_int(1, si <= sj, "signed 01ff <= 0100");
    si = MAKE_SINT128(1,   0);   sj = MAKE_SINT128(0,   vff); assert_int(0, si <= sj, "signed 0100 <= 00ff");
    si = MAKE_SINT128(v7f, vff); sj = MAKE_SINT128(v80, 0);   assert_int(0, si <= sj, "signed 7fff <= 8000");
    si = MAKE_SINT128(v80, 0);   sj = MAKE_SINT128(v7f, vff); assert_int(1, si <= sj, "signed 8000 <= 7fff");
    si = MAKE_SINT128(vff, vfe); sj = MAKE_SINT128(vff, vff); assert_int(1, si <= sj, "signed fffe <= ffff");
    si = MAKE_SINT128(vff, vff); sj = MAKE_SINT128(vff, vff); assert_int(1, si <= sj, "signed ffff <= ffff");

    // Unsigned <=
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(1,   1);   assert_int(1, ui <= uj, "unsigned 0101 <= 0101");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(1,   2);   assert_int(1, ui <= uj, "unsigned 0101 <= 0102");
    ui = MAKE_UINT128(1,   2);   uj = MAKE_UINT128(1,   1);   assert_int(0, ui <= uj, "unsigned 0102 <= 0101");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(2,   1);   assert_int(1, ui <= uj, "unsigned 0101 <= 0201");
    ui = MAKE_UINT128(1,   1);   uj = MAKE_UINT128(2,   2);   assert_int(1, ui <= uj, "unsigned 0101 <= 0202");
    ui = MAKE_UINT128(1,   2);   uj = MAKE_UINT128(2,   1);   assert_int(1, ui <= uj, "unsigned 0102 <= 0201");
    ui = MAKE_UINT128(2,   1);   uj = MAKE_UINT128(1,   1);   assert_int(0, ui <= uj, "unsigned 0201 <= 0101");
    ui = MAKE_UINT128(2,   1);   uj = MAKE_UINT128(1,   2);   assert_int(0, ui <= uj, "unsigned 0201 <= 0102");
    ui = MAKE_UINT128(2,   2);   uj = MAKE_UINT128(1,   1);   assert_int(0, ui <= uj, "unsigned 0202 <= 0101");
    ui = MAKE_UINT128(1,   0);   uj = MAKE_UINT128(1,   vff); assert_int(1, ui <= uj, "unsigned 0100 <= 01ff");
    ui = MAKE_UINT128(1,   vff); uj = MAKE_UINT128(1,   0);   assert_int(0, ui <= uj, "unsigned 01ff <= 0100");
    ui = MAKE_UINT128(0,   vff); uj = MAKE_UINT128(1,   0);   assert_int(1, ui <= uj, "unsigned 01ff <= 0100");
    ui = MAKE_UINT128(1,   0);   uj = MAKE_UINT128(0,   vff); assert_int(0, ui <= uj, "unsigned 0100 <= 00ff");
    ui = MAKE_UINT128(v7f, vff); uj = MAKE_UINT128(v80, 0);   assert_int(1, ui <= uj, "unsigned 7fff <= 8000");
    ui = MAKE_UINT128(v80, 0);   uj = MAKE_UINT128(v7f, vff); assert_int(0, ui <= uj, "unsigned 8000 <= 7fff");
    ui = MAKE_SINT128(vff, vfe); sj = MAKE_SINT128(vff, vff); assert_int(0, ui <= uj, "unsigned fffe <= ffff");
    ui = MAKE_UINT128(vff, vff); uj = MAKE_UINT128(vff, vff); assert_int(1, ui <= uj, "unsigned ffff <= ffff");
}

static void test_conditional_jmp_cmp_to_zero(void) {
    __int128 si;
    unsigned __int128 ui;
    int r;

    si = MAKE_SINT128(0, 0); r = 0; if (si) r = 1;  assert_int(0, r, "signed int128 if(0)");
    si = MAKE_SINT128(1, 1); r = 0; if (si) r = 1;  assert_int(1, r, "signed int128 if(1)");
    si = MAKE_SINT128(1, 0); r = 0; if (si) r = 1;  assert_int(1, r, "signed int128 if(1 << 64)");
    si = MAKE_SINT128(1, 1); r = 0; if (si) r = 1;  assert_int(1, r, "signed int128 if(1 << 64 | 1)");

    ui = MAKE_UINT128(0, 0); r = 0; if (ui) r = 1;  assert_int(0, r, "unsigned int128 if(0)");
    ui = MAKE_UINT128(1, 1); r = 0; if (ui) r = 1;  assert_int(1, r, "unsigned int128 if(1)");
    ui = MAKE_UINT128(1, 0); r = 0; if (ui) r = 1;  assert_int(1, r, "unsigned int128 if(1 << 64)");
    ui = MAKE_UINT128(1, 1); r = 0; if (ui) r = 1;  assert_int(1, r, "unsigned int128 if(1 << 64 | 1)");
}

static void test_pointers() {
    // A pointer to an int128 in the stack
    __int128 i = MAKE_UINT128(1, 2);
    __int128 *pi = &i;
    assert_int128(1, 2, *pi, "int128 address and deref");

    // A pointer to an array
    __int128 array[2] = {MAKE_UINT128(1, 2), MAKE_UINT128(3, 4)};
    assert_int128(1, 2, array[0], "int128 array lookup 0");
    assert_int128(3, 4, array[1], "int128 array lookup 1");

    struct s {
        __int128 i, j, k;
    } s = {MAKE_UINT128(1, 2), MAKE_UINT128(3, 4), MAKE_UINT128(5, 6)};
    assert_int128(1, 2, s.i, "int128 in struct lookup 0");
    assert_int128(3, 4, s.j, "int128 in struct lookup 1");
    assert_int128(5, 6, s.k, "int128 in struct lookup 2");

    // Writes to a pointer
    *pi = (__int128) 1;            assert_int128(0,        1,        i, "Write to pointer in register 1 << 0");
    *pi = (__int128) 1 << 32;      assert_int128(0,        1L << 32, i, "Write to pointer in register 1 << 32");
    *pi = (__int128) 1 << 63;      assert_int128(0,        1L << 63, i, "Write to pointer in register 1 << 63");
    *pi = (__int128) 1 << 64;      assert_int128(1,        0,        i, "Write to pointer in register 1 << 64");
    *pi = (__int128) 1 << 96;      assert_int128(1L << 32, 0,        i, "Write to pointer in register 1 << 96");
    *pi = (__int128) 1 << 127;     assert_int128(1L << 63, 0,        i, "Write to pointer in register 1 << 127");
    *pi = (__int128) 1 << 127 | 1; assert_int128(1L << 63, 1,        i, "Write to pointer in register 1 << 127 | 1");

    // Writes to a pointer with an offset
    struct s *ps = &s;
    ps->j  = MAKE_UINT128(1, 0);
    assert_int128(1, 0, ps->j, "Write to pointer with an offset");
}

static void test_constant_promotions() {
    __int128 si;

    // Some sanity tests
    si = 5;
    assert_int128(0, 5, si, "constant -> int128 promotion 1");
    si += 2;
    assert_int128(0, 7, si, "constant -> int128 promotion 2");

    // Test assignment from an unsigned integer constant
    si = 0xffffffffffffffffL;
    assert_int128(0, -1, si, "constant -> int128 promotion 5");

    // Test assignment through a cast from an unsigned integer constant
    si = (__int128) 0xffffffffffffffffL;
    assert_int128(0, -1, si, "constant -> int128 promotion 6");

    // Test addition through a cast from an unsigned integer constant
    si = 7;
    si += (__int128) 0xffffffffffffffffL;
    assert_int128(1, 6, si, "constant -> int128 promotion 3");

    // Test addition with an unsigned integer constant
    si = 7;
    si += 0xffffffffffffffffL;
    assert_int128(1, 6, si, "constant -> int128 promotion 4");
}

void test_sign(__int128 expected) {
    ASSERT_INT128(0, -1, expected, "Ensure -1L isn't sign extended");
}

// Test bug where a unsigned long 0xffffffffffffffff would be sign extended to an int128
// instead of kept at 0xffffffffffffffff.
int test_signedness_function_call_bug(void) {
    test_sign(0xffffffffffffffff);
}

// Test a bug where an used low/high int128 vreg was
// not counted in the total and reused
__int128 vab0(__int128 i) {
    return i + 1;
}

void vab1(long i, long j) {
    assert_long(43, i, "int128 vreg counting bug 1");
    assert_long(43, j, "int128 vreg counting bug 2");
}

void vab2(long i) {
    assert_long(43, i, "int128 vreg counting bug 3");
}

void test_register_vreg_counting_bug() {
    __int128 si = 42;

    vab1(vab0(si), vab0(si));
    vab2(vab0(si));
    vab2(vab0(si));
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv);

    test_assignment_from_constant();
    test_stack_assignment_from_constant();
    test_assignment_from_variable();
    test_stack_assignment_from_variable();
    test_stack_allocation();
    test_truncations();
    test_bit_shifts_with_register_and_constant();
    test_bit_shifts_with_register_and_register();
    test_bit_shifts_with_constant_and_register();
    test_binary_or();
    test_binary_and();
    test_binary_xor();
    test_addition();
    test_subtraction();
    test_multiplication();
    test_negation();
    test_comparisons();
    test_conditional_jmp_cmp_to_zero();
    test_pointers();
    test_constant_promotions();
    test_signedness_function_call_bug();
    test_register_vreg_counting_bug();

    finalize();
}
