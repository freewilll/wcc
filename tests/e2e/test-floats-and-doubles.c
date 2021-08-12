#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

float gf, gf1;
double gd, gd1;

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

    // Register -> global
    float f5  = 10.0; gf = f5; assert_float (gf, 10.0, "register -> memory float  -> float");
    float f6  = 11.0; gd = f6; assert_double(gd, 11.0, "register -> memory float  -> double");
    double d5 = 12.0; gf = d5; assert_float (gf, 12.0, "register -> memory double -> float");
    double d6 = 13.0; gd = d6; assert_double(gd, 13.0, "register -> memory double -> double");

    #endif
}

void test_conversion_sse_cst_to_int() {
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

void test_conversion_sse_to_int() {
    #ifdef FLOATS

    float f;
    double d;

    // SSE in register to int in register
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

    // SSE in register to int in memory
    f = 1.1; gc = f; assert_long(1, gc, "float  in register to char  in memory");
    f = 2.1; gs = f; assert_long(2, gs, "float  in register to short in memory");
    f = 3.1; gi = f; assert_long(3, gi, "float  in register to int   in memory");
    f = 4.1; gl = f; assert_long(4, gl, "float  in register to long  in memory");
    d = 5.1; gc = d; assert_long(5, gc, "double in register to char  in memory");
    d = 6.1; gs = d; assert_long(6, gs, "double in register to short in memory");
    d = 7.1; gi = d; assert_long(7, gi, "double in register to int   in memory");
    d = 8.1; gl = d; assert_long(8, gl, "double in register to long  in memory");

    f = 1.1; guc = f; assert_long(1, guc, "float  in register to unsigned char  in memory");
    f = 2.1; gus = f; assert_long(2, gus, "float  in register to unsigned short in memory");
    f = 3.1; gui = f; assert_long(3, gui, "float  in register to unsigned int   in memory");
    f = 4.1; gul = f; assert_long(4, gul, "float  in register to unsigned long  in memory");
    d = 5.1; guc = d; assert_long(5, guc, "double in register to unsigned char  in memory");
    d = 6.1; gus = d; assert_long(6, gus, "double in register to unsigned short in memory");
    d = 7.1; gui = d; assert_long(7, gui, "double in register to unsigned int   in memory");
    d = 8.1; gul = d; assert_long(8, gul, "double in register to unsigned long  in memory");

    // SSE in memory to int in memory
    gf = 1.1; gc = gf; assert_long(1, gc, "float  in register to char  in memory");
    gf = 2.1; gs = gf; assert_long(2, gs, "float  in register to short in memory");
    gf = 3.1; gi = gf; assert_long(3, gi, "float  in register to int   in memory");
    gf = 4.1; gl = gf; assert_long(4, gl, "float  in register to long  in memory");
    gd = 5.1; gc = gd; assert_long(5, gc, "double in register to char  in memory");
    gd = 6.1; gs = gd; assert_long(6, gs, "double in register to short in memory");
    gd = 7.1; gi = gd; assert_long(7, gi, "double in register to int   in memory");
    gd = 8.1; gl = gd; assert_long(8, gl, "double in register to long  in memory");

    gf = 1.1; guc = gf; assert_long(1, guc, "float  in register to unsigned char  in memory");
    gf = 2.1; gus = gf; assert_long(2, gus, "float  in register to unsigned short in memory");
    gf = 3.1; gui = gf; assert_long(3, gui, "float  in register to unsigned int   in memory");
    gf = 4.1; gul = gf; assert_long(4, gul, "float  in register to unsigned long  in memory");
    gd = 5.1; guc = gd; assert_long(5, guc, "double in register to unsigned char  in memory");
    gd = 6.1; gus = gd; assert_long(6, gus, "double in register to unsigned short in memory");
    gd = 7.1; gui = gd; assert_long(7, gui, "double in register to unsigned int   in memory");
    gd = 8.1; gul = gd; assert_long(8, gul, "double in register to unsigned long  in memory");

    // SSE in memory -> int in register
    gf = 1.1; assert_long(1, (char)  gf, "float  on stack to char  in register");
    gf = 2.1; assert_long(2, (short) gf, "float  on stack to short in register");
    gf = 3.1; assert_long(3, (int)   gf, "float  on stack to int   in register");
    gf = 4.1; assert_long(4, (long)  gf, "float  on stack to long  in register");
    gd = 5.1; assert_long(5, (char)  gd, "double on stack to char  in register");
    gd = 6.1; assert_long(6, (short) gd, "double on stack to short in register");
    gd = 7.1; assert_long(7, (int)   gd, "double on stack to int   in register");
    gd = 8.1; assert_long(8, (long)  gd, "double on stack to long  in register");

    gf = 1.1; assert_long(1, (unsigned char)  gf, "float  on stack to unsigned char  in register");
    gf = 2.1; assert_long(2, (unsigned short) gf, "float  on stack to unsigned short in register");
    gf = 3.1; assert_long(3, (unsigned int)   gf, "float  on stack to unsigned int   in register");
    gf = 4.1; assert_long(4, (unsigned long)  gf, "float  on stack to unsigned long  in register");
    gd = 5.1; assert_long(5, (unsigned char)  gd, "double on stack to unsigned char  in register");
    gd = 6.1; assert_long(6, (unsigned short) gd, "double on stack to unsigned short in register");
    gd = 7.1; assert_long(7, (unsigned int)   gd, "double on stack to unsigned int   in register");
    gd = 8.1; assert_long(8, (unsigned long)  gd, "double on stack to unsigned long  in register");

    #endif
}

void test_conversion_sse_cst_to_long_double() {
    #ifdef FLOATS

    float f;
    double d;
    long double ld;

    f = 1.1f; ld = f; assert_long_double(1.1, ld, "sse float constant  -> ld");
    d = 2.1f; ld = d; assert_long_double(2.1, ld, "sse double constant -> ld");

    #endif
}

int test_conversion_sse_to_long_double() {
    #ifdef FLOATS

    float f;
    double d;

    // SSE in register to long double
    f = 1.1; assert_long_double(1.1, (long double) f,"float  in register to long double");
    d = 2.1; assert_long_double(2.1, (long double) d,"double in register to long double");

    // SSE in memory to long double
    gf = 1.1; assert_long_double(1.1, (long double) gf,"float  in register to long double");
    gd = 2.1; assert_long_double(2.1, (long double) gd,"double in register to long double");

    #endif
}

int test_conversion_int_to_sse() {
    #ifdef FLOATS

    float f;
    double d;

    // Int in register to SSE in register
    f = (char)  -1; assert_float(-1.0, f, "char  in register to float in register");
    f = (short) -2; assert_float(-2.0, f, "short in register to float in register");
    f = (int)   -3; assert_float(-3.0, f, "int   in register to float in register");
    f = (long)  -4; assert_float(-4.0, f, "long  in register to float in register");
    d = (char)  -5; assert_float(-5.0, d, "char  in register to double in register");
    d = (short) -6; assert_float(-6.0, d, "short in register to double in register");
    d = (int)   -7; assert_float(-7.0, d, "int   in register to double in register");
    d = (long)  -8; assert_float(-8.0, d, "long  in register to double in register");

    // Without high bit set
    f = (unsigned char)  1; assert_float(1.0, f, "unsigned char  in register to float  in register");
    f = (unsigned short) 2; assert_float(2.0, f, "unsigned short in register to float  in register");
    f = (unsigned int)   3; assert_float(3.0, f, "unsigned int   in register to float  in register");
    f = (unsigned long)  4; assert_float(4.0, f, "unsigned long  in register to float  in register");
    d = (unsigned char)  5; assert_float(5.0, d, "unsigned char  in register to double in register");
    d = (unsigned short) 6; assert_float(6.0, d, "unsigned short in register to double in register");
    d = (unsigned int)   7; assert_float(7.0, d, "unsigned int   in register to double in register");
    d = (unsigned long)  8; assert_float(8.0, d, "unsigned long  in register to double in register");

    // With high bit set
    f = (unsigned char)  -1; assert_float(255.0,                  f, "unsigned char  in register to float  in register");
    f = (unsigned short) -1; assert_float(65535.0,                f, "unsigned short in register to float  in register");
    f = (unsigned int)   -1; assert_float(4294967296.0,           f, "unsigned int   in register to float  in register");
    f = (unsigned long)  -1; assert_float(18446744073709551616.0, f, "unsigned long  in register to float  in register");
    d = (unsigned char)  -1; assert_float(255.0,                  d, "unsigned char  in register to double in register");
    d = (unsigned short) -1; assert_float(65535.0,                d, "unsigned short in register to double in register");
    d = (unsigned int)   -1; assert_float(4294967296.0,           d, "unsigned int   in register to double in register");
    d = (unsigned long)  -1; assert_float(18446744073709551616.0, d, "unsigned long  in register to double in register");

    // Int in register to SSE in memory
    gf = (char)  -1; assert_float(-1.0, gf, "char  in register to float  in memory");
    gf = (short) -2; assert_float(-2.0, gf, "short in register to float  in memory");
    gf = (int)   -3; assert_float(-3.0, gf, "int   in register to float  in memory");
    gf = (long)  -4; assert_float(-4.0, gf, "long  in register to float  in memory");
    gd = (char)  -5; assert_float(-5.0, gd, "char  in register to double in memory");
    gd = (short) -6; assert_float(-6.0, gd, "short in register to double in memory");
    gd = (int)   -7; assert_float(-7.0, gd, "int   in register to double in memory");
    gd = (long)  -8; assert_float(-8.0, gd, "long  in register to double in memory");

    // Int in memory to SSE in register
    gc = -1; f = gc; assert_float(-1.0, f, "char  in memory to float  in memory");
    gs = -2; f = gs; assert_float(-2.0, f, "short in memory to float  in memory");
    gi = -3; f = gi; assert_float(-3.0, f, "int   in memory to float  in memory");
    gl = -4; f = gl; assert_float(-4.0, f, "long  in memory to float  in memory");
    gc = -5; d = gc; assert_float(-5.0, d, "char  in memory to double in memory");
    gs = -6; d = gs; assert_float(-6.0, d, "short in memory to double in memory");
    gi = -7; d = gi; assert_float(-7.0, d, "int   in memory to double in memory");
    gl = -8; d = gl; assert_float(-8.0, d, "long  in memory to double in memory");

    // Int in memory to SSE in memory
    gc = -1; gf = gc; assert_float(-1.0, gf, "char  in memory to float  in memory");
    gs = -2; gf = gs; assert_float(-2.0, gf, "short in memory to float  in memory");
    gi = -3; gf = gi; assert_float(-3.0, gf, "int   in memory to float  in memory");
    gl = -4; gf = gl; assert_float(-4.0, gf, "long  in memory to float  in memory");
    gc = -5; gd = gc; assert_float(-5.0, gd, "char  in memory to double in memory");
    gs = -6; gd = gs; assert_float(-6.0, gd, "short in memory to double in memory");
    gi = -7; gd = gi; assert_float(-7.0, gd, "int   in memory to double in memory");
    gl = -8; gd = gl; assert_float(-8.0, gd, "long  in memory to double in memory");

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

    test_conversion_sse_cst_to_int();
    test_conversion_sse_to_int();
    test_conversion_sse_cst_to_long_double();
    test_conversion_sse_to_long_double();
    test_conversion_int_to_sse();
    test_spilling();
    test_long_double_constant_promotion_in_arithmetic();
    test_constants_in_function_calls();

    finalize();
}
