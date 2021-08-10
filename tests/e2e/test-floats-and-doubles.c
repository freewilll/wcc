#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

float gf1;
double gd1;

char  gc;
short gs;
int   gi;
long  gl;
unsigned char  guc;
unsigned short gus;
unsigned int   gui;
unsigned long  gul;

void assert_ld_string(long double ld, char *expected, char *message) {
    char *buffer = malloc(100);

    sprintf(buffer, "%5.5Lf", ld);

    int matches = !strcmp(buffer, expected);
    if (!matches) printf("Expected \"%s\", got \"%s\"\n", expected, buffer);
    assert_int(1, matches, message);
}

void test_constant_assignment() {
    char *buffer;
    buffer = malloc(16);

    int i1 = 1;                 assert_int(i1, 1, "constant assignment int    -> int");
    int i2 = 1.0f;              assert_int(i2, 1, "constant assignment float  -> int");
    int i3 = 1.0;               assert_int(i3, 1, "constant assignment double -> int");
    int i4 = 1.0l;              assert_int(i4, 1, "constant assignment ld     -> int");
    #ifdef FLOATS
    float f1 = 2;               assert_float(2.0f, f1, "float constant assignment from int");
    float f2 = 2.0f;            assert_float(2.0f, f2, "float constant assignment from float");
    float f3 = 2.0;             assert_float(2.0f, f3, "float constant assignment from double");
    float f4 = 2.0l;            assert_float(2.0f, f4, "float constant assignment from LD");
    double d1 = 3;              assert_double(3.0, d1, "double constant assignment from int");
    double d2 = 3.0f;           assert_double(3.0, d2, "double constant assignment from float");
    double d3 = 3.0;            assert_double(3.0, d3, "double constant assignment from double");
    double d4 = 3.0l;           assert_double(3.0, d4, "double constant assignment from LD");
    #endif
    long double ld1 = 4;        assert_ld_string(ld1, "4.00000", "constant assignment int    -> ld");
    long double ld2 = 4.0f;     assert_ld_string(ld2, "4.00000", "constant assignment float  -> ld");
    long double ld3 = 4.0;      assert_ld_string(ld3, "4.00000", "constant assignment double -> ld");
    long double ld4 = 4.0l;     assert_ld_string(ld4, "4.00000", "constant assignment ld     -> ld");
}

void test_assignment() {
    #ifdef FLOATS
    // In registers. Variables are not reused in this test, otherwise they would get
    // spilled onto the stack due to function calls. Not that there is a problem with
    // that, but this test is meant to test assignment only, not spilling.
    float f1  = 1.0f; float  f2 = f1; assert_float (1.0f, f2, "float -> float assignment");
    float f3  = 2.0f; double d1 = f3; assert_double(2.0,  d1, "float -> double assignment");
    double d2 = 3.0;  float  f4 = d2; assert_float (3.0f, f4, "double -> float assignment");
    double d3 = 4.0;  double d4 = d3; assert_double(4.0,  d4, "double -> double assignment");

    // Arguments on the stack
    float  sf2 = 1.0; &sf2; assert_float( 1.0, sf2, "constant -> memory float  -> float");
    double sd1 = 2.0; &sd1; assert_double(2.0, sd1, "constant -> memory double -> double");

    // Memory -> register
    float  sf5 = 5.0; &sf5; float  sf6 = sf5; assert_double(5.0, sf6, "memory -> register float  -> float");
    float  sf3 = 3.0; &sf3; double sd2 = sf3; assert_double(3.0, sd2, "memory -> register float  -> double");
    double sd3 = 4.0; &sd3; float  sf4 = sd3; assert_float( 4.0, sf4, "memory -> register double -> float");
    double sd4 = 6.0; &sd4; double sd5 = sd4; assert_float( 6.0, sd5, "memory -> register double -> double");

    // Globals
    gf1 = 7.0; assert_float( 7.0, gf1, "global float assignment");
    gd1 = 8.0; assert_double(8.0, gd1, "global double assignment");

    #endif
}

void test_conversion_sse_in_cst_to_int_in_reg() {
    #ifdef FLOATS
    float f;
    double d;

    char  c;
    short s;
    int   i;
    long  l;
    unsigned char  uc;
    unsigned short us;
    unsigned int   ui;
    unsigned long  ul;

    // This tests constant SSE assignment, since all operations are tree-collapsed
    f = 11.0f; c = f; assert_long(11, c, "c = f");
    f = 12.0f; s = f; assert_long(12, s, "s = f");
    f = 13.0f; i = f; assert_long(13, i, "i = f");
    f = 14.0f; l = f; assert_long(14, l, "l = d");
    d = 15.0f; c = d; assert_long(15, c, "c = d");
    d = 16.0f; s = d; assert_long(16, s, "s = d");
    d = 17.0f; i = d; assert_long(17, i, "i = d");
    d = 18.0f; l = d; assert_long(18, l, "l = d");

    f = 21.1f; uc = f; assert_long(21, uc, "uc = f");
    f = 22.1f; us = f; assert_long(22, us, "us = f");
    f = 23.1f; ui = f; assert_long(23, ui, "ui = f");
    f = 24.1f; ul = f; assert_long(24, ul, "ul = d");
    d = 25.1f; uc = d; assert_long(25, uc, "uc = d");
    d = 26.1f; us = d; assert_long(26, us, "us = d");
    d = 27.1f; ui = d; assert_long(27, ui, "ui = d");
    d = 28.1f; ul = d; assert_long(28, ul, "ul = d");

    f = 31.0f; gc = f; assert_long(31, gc, "guc = f");
    f = 32.0f; gs = f; assert_long(32, gs, "gus = f");
    f = 33.0f; gi = f; assert_long(33, gi, "gui = f");
    f = 34.0f; gl = f; assert_long(34, gl, "gul = d");
    d = 35.0f; gc = d; assert_long(35, gc, "guc = d");
    d = 36.0f; gs = d; assert_long(36, gs, "gus = d");
    d = 37.0f; gi = d; assert_long(37, gi, "gui = d");
    d = 38.0f; gl = d; assert_long(38, gl, "gul = d");

    f = 41.1f; guc = f; assert_long(41, guc, "guc = f");
    f = 42.1f; gus = f; assert_long(42, gus, "gus = f");
    f = 43.1f; gui = f; assert_long(43, gui, "gui = f");
    f = 44.1f; gul = f; assert_long(44, gul, "gul = d");
    d = 45.1f; guc = d; assert_long(45, guc, "guc = d");
    d = 46.1f; gus = d; assert_long(46, gus, "gus = d");
    d = 47.1f; gui = d; assert_long(47, gui, "gui = d");
    d = 48.1f; gul = d; assert_long(48, gul, "gul = d");

    #endif
}

void test_conversion_sse_in_reg_to_int_in_reg() {
    #ifdef FLOATS
    float f;
    double d;

    f = 1.1; assert_long(1, (char)  f, "float  in register to char  in register");
    f = 2.1; assert_long(2, (short) f, "float  in register to short in register");
    f = 3.1; assert_long(3, (int)   f, "float  in register to int   in register");
    f = 4.1; assert_long(4, (long)  f, "float  in register to long  in register");
    d = 5.1; assert_long(5, (char)  d, "double in register to char  in register");
    d = 6.1; assert_long(6, (short) d, "double in register to short in register");
    d = 7.1; assert_long(7, (int)   d, "double in register to int   in register");
    d = 8.1; assert_long(8, (long)  d, "double in register to long  in register");

    f = 1.1; assert_long(1, (unsigned char)  f, "float  in register to unsigned char  in register");
    f = 2.1; assert_long(2, (unsigned short) f, "float  in register to unsigned short in register");
    f = 3.1; assert_long(3, (unsigned int)   f, "float  in register to unsigned int   in register");
    f = 4.1; assert_long(4, (unsigned long)  f, "float  in register to unsigned long  in register");
    d = 5.1; assert_long(5, (unsigned char)  d, "double in register to unsigned char  in register");
    d = 6.1; assert_long(6, (unsigned short) d, "double in register to unsigned short in register");
    d = 7.1; assert_long(7, (unsigned int)   d, "double in register to unsigned int   in register");
    d = 8.1; assert_long(8, (unsigned long)  d, "double in register to unsigned long  in register");

    #endif
}

void test_conversion_sse_in_reg_to_int_in_stk() {
    #ifdef FLOATS
    float f;
    double d;

    char  c;
    short s;
    int   i;
    long  l;
    unsigned char  uc;
    unsigned short us;
    unsigned int   ui;
    unsigned long  ul;

    &c; &s; &i; &l;

    f = 1.1; c = f; assert_long(1, c, "float  in register to char  in memory");
    f = 2.1; s = f; assert_long(2, s, "float  in register to short in memory");
    f = 3.1; i = f; assert_long(3, i, "float  in register to int   in memory");
    f = 4.1; l = f; assert_long(4, l, "float  in register to long  in memory");
    d = 5.1; c = d; assert_long(5, c, "double in register to char  in memory");
    d = 6.1; s = d; assert_long(6, s, "double in register to short in memory");
    d = 7.1; i = d; assert_long(7, i, "double in register to int   in memory");
    d = 8.1; l = d; assert_long(8, l, "double in register to long  in memory");

    f = 1.1; uc = f; assert_long(1, uc, "float  in register to unsigned char  in memory");
    f = 2.1; us = f; assert_long(2, us, "float  in register to unsigned short in memory");
    f = 3.1; ui = f; assert_long(3, ui, "float  in register to unsigned int   in memory");
    f = 4.1; ul = f; assert_long(4, ul, "float  in register to unsigned long  in memory");
    d = 5.1; uc = d; assert_long(5, uc, "double in register to unsigned char  in memory");
    d = 6.1; us = d; assert_long(6, us, "double in register to unsigned short in memory");
    d = 7.1; ui = d; assert_long(7, ui, "double in register to unsigned int   in memory");
    d = 8.1; ul = d; assert_long(8, ul, "double in register to unsigned long  in memory");

    #endif
}

void test_conversion_sse_in_stk_to_int_in_stk() {
    #ifdef FLOATS
    float f;
    double d;

    char  c;
    short s;
    int   i;
    long  l;
    unsigned char  uc;
    unsigned short us;
    unsigned int   ui;
    unsigned long  ul;

    &f; &d;
    &c; &s; &i; &l;

    f = 1.1; c = f; assert_long(1, c, "float  in register to char  in memory");
    f = 2.1; s = f; assert_long(2, s, "float  in register to short in memory");
    f = 3.1; i = f; assert_long(3, i, "float  in register to int   in memory");
    f = 4.1; l = f; assert_long(4, l, "float  in register to long  in memory");
    d = 5.1; c = d; assert_long(5, c, "double in register to char  in memory");
    d = 6.1; s = d; assert_long(6, s, "double in register to short in memory");
    d = 7.1; i = d; assert_long(7, i, "double in register to int   in memory");
    d = 8.1; l = d; assert_long(8, l, "double in register to long  in memory");

    f = 1.1; uc = f; assert_long(1, uc, "float  in register to unsigned char  in memory");
    f = 2.1; us = f; assert_long(2, us, "float  in register to unsigned short in memory");
    f = 3.1; ui = f; assert_long(3, ui, "float  in register to unsigned int   in memory");
    f = 4.1; ul = f; assert_long(4, ul, "float  in register to unsigned long  in memory");
    d = 5.1; uc = d; assert_long(5, uc, "double in register to unsigned char  in memory");
    d = 6.1; us = d; assert_long(6, us, "double in register to unsigned short in memory");
    d = 7.1; ui = d; assert_long(7, ui, "double in register to unsigned int   in memory");
    d = 8.1; ul = d; assert_long(8, ul, "double in register to unsigned long  in memory");

    #endif
}

void test_conversion_sse_in_stk_to_int_in_reg() {
    #ifdef FLOATS
    float f;
    double d;

    &f; // Force them into the stack
    &d; // Force them into the stack

    // SSE on stack -> int in register
    f = 1.1; assert_long(1, (char)  f, "float  on stack to char  in register");
    f = 2.1; assert_long(2, (short) f, "float  on stack to short in register");
    f = 3.1; assert_long(3, (int)   f, "float  on stack to int   in register");
    f = 4.1; assert_long(4, (long)  f, "float  on stack to long  in register");
    d = 5.1; assert_long(5, (char)  d, "double on stack to char  in register");
    d = 6.1; assert_long(6, (short) d, "double on stack to short in register");
    d = 7.1; assert_long(7, (int)   d, "double on stack to int   in register");
    d = 8.1; assert_long(8, (long)  d, "double on stack to long  in register");

    f = 1.1; assert_long(1, (unsigned char)  f, "float  on stack to unsigned char  in register");
    f = 2.1; assert_long(2, (unsigned short) f, "float  on stack to unsigned short in register");
    f = 3.1; assert_long(3, (unsigned int)   f, "float  on stack to unsigned int   in register");
    f = 4.1; assert_long(4, (unsigned long)  f, "float  on stack to unsigned long  in register");
    d = 5.1; assert_long(5, (unsigned char)  d, "double on stack to unsigned char  in register");
    d = 6.1; assert_long(6, (unsigned short) d, "double on stack to unsigned short in register");
    d = 7.1; assert_long(7, (unsigned int)   d, "double on stack to unsigned int   in register");
    d = 8.1; assert_long(8, (unsigned long)  d, "double on stack to unsigned long  in register");

    #endif
}

void test_conversion_sse_to_long_double() {
    #ifdef FLOATS
    float f;
    double d;
    long double ld;

    f = 1.1f; ld = f; assert_long_double(1.1, ld, "sse float constant  -> ld");
    d = 2.1f; ld = d; assert_long_double(2.1, ld, "sse double constant -> ld");
    #endif
}

void test_spilling() {
    // All four types are spilled, due to the double function call

    #ifdef FLOATS
    char *buffer;
    buffer = malloc(128);

    float sf1 = 1.0;
    float sf2 = 2.0;
    double sd1 = 3.0;
    double sd2 = 4.0;

    sprintf(buffer, "%f %f %f %f", sf1, sf2, sd1, sd2);
    assert_int(0, strcmp(buffer, "1.000000 2.000000 3.000000 4.000000"), "Spilling 1");
    sprintf(buffer, "%f %f %f %f", sf1, sf2, sd1, sd2);
    assert_int(0, strcmp(buffer, "1.000000 2.000000 3.000000 4.000000"), "Spilling 1");
    #endif
}

void test_long_double_constant_promotion_in_arithmetic() {
    long double ld;

    ld = 1.0;
    assert_ld_string(ld * 1.0f, "1.00000", "constant float promotion to long double in arithmetic");
    assert_ld_string(ld * 1.0,  "1.00000", "constant double promotion to long double in arithmetic");
}

void test_constants_in_function_calls() {
    // Test function calls with different typed constants
    assert_int        (1,    1,    "int    -> int");
    assert_int        (2,    2.0f, "float  -> int");
    assert_int        (3,    3.0,  "double -> int");
    assert_int        (4,    4.0l, "ld     -> int");
    #ifdef FLOATS
    assert_float      (1.0f, 1,    "int    -> float");
    assert_float      (2.0f, 2.0f, "float  -> float");
    assert_float      (3.0f, 3.0,  "double -> float");
    assert_float      (4.0f, 4.0l, "ld     -> float");
    assert_double     (1.0,  1,    "int    -> double");
    assert_double     (2.0,  2.0f, "float  -> double");
    assert_double     (3.0,  3.0,  "double -> double");
    assert_double     (4.0,  4.0l, "ld     -> double");
    #endif
    assert_long_double(1.0l, 1,    "int    -> long double");
    assert_long_double(2.0l, 2.0f, "float  -> long double");
    assert_long_double(3.0l, 3.0,  "double -> long double");
    assert_long_double(4.0l, 4.0l, "ld     -> long double");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_constant_assignment();
    test_assignment();
    test_conversion_sse_in_cst_to_int_in_reg();
    test_conversion_sse_in_reg_to_int_in_reg();
    test_conversion_sse_in_stk_to_int_in_reg();
    test_conversion_sse_in_reg_to_int_in_stk();
    test_conversion_sse_in_stk_to_int_in_stk();
    test_conversion_sse_to_long_double();
    test_spilling();
    test_long_double_constant_promotion_in_arithmetic();
    test_constants_in_function_calls();

    finalize();
}
