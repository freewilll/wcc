#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

float gf, gf1, gf2, *gpf;
double gd, gd1, gd2, *gpd;

char  gc;
short gs;
int   gi;
long  gl;
unsigned char  guc;
unsigned short gus;
unsigned int   gui;
unsigned long  gul;

struct s1 { int i; float f; double d; };
struct sf { float f; int i; };
struct sd { double d; int i; };

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
    float f1 = 2;               assert_float(2.0f, f1, "float constant assignment from int");
    float f2 = 2.0f;            assert_float(2.0f, f2, "float constant assignment from float");
    float f3 = 2.0;             assert_float(2.0f, f3, "float constant assignment from double");
    float f4 = 2.0l;            assert_float(2.0f, f4, "float constant assignment from LD");
    double d1 = 3;              assert_double(3.0, d1, "double constant assignment from int");
    double d2 = 3.0f;           assert_double(3.0, d2, "double constant assignment from float");
    double d3 = 3.0;            assert_double(3.0, d3, "double constant assignment from double");
    double d4 = 3.0l;           assert_double(3.0, d4, "double constant assignment from LD");
    long double ld1 = 4;        assert_ld_string(ld1, "4.00000", "constant assignment int    -> ld");
    long double ld2 = 4.0f;     assert_ld_string(ld2, "4.00000", "constant assignment float  -> ld");
    long double ld3 = 4.0;      assert_ld_string(ld3, "4.00000", "constant assignment double -> ld");
    long double ld4 = 4.0l;     assert_ld_string(ld4, "4.00000", "constant assignment ld     -> ld");
}

void test_assignment() {
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
}

void test_conversion_sse_cst_to_int() {
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
}

void test_conversion_sse_to_int() {
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
}

void test_conversion_sse_cst_to_long_double() {
    float f;
    double d;
    long double ld;

    f = 1.1f; ld = f; assert_long_double(1.1, ld, "sse float constant  -> ld");
    d = 2.1f; ld = d; assert_long_double(2.1, ld, "sse double constant -> ld");
}

int test_conversion_sse_to_long_double() {
    float f;
    double d;

    // SSE in register to long double
    f = 1.1; assert_long_double(1.1, (long double) f,"float  in register to long double");
    d = 2.1; assert_long_double(2.1, (long double) d,"double in register to long double");

    // SSE in memory to long double
    gf = 1.1; assert_long_double(1.1, (long double) gf,"float  in register to long double");
    gd = 2.1; assert_long_double(2.1, (long double) gd,"double in register to long double");
}

void test_conversion_int_to_sse() {
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
}

void test_conversion_long_double_to_sse() {
    long double ld;
    float f;
    double d;

    // To register
    ld = 1.1; f = ld; assert_float( 1.1, f, "long double to float  in register");
    ld = 2.1; d = ld; assert_double(2.1, d, "long double to double in register");

    // To memory
    ld = 1.1; gf = ld; assert_float( 1.1, gf, "long double to float  in memory");
    ld = 2.1; gd = ld; assert_double(2.1, gd, "long double to double in memory");
}

void test_spilling() {
    // All four types are spilled, due to the double function call

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
    assert_float      (1.0f, 1,    "int    -> float");
    assert_float      (2.0f, 2.0f, "float  -> float");
    assert_float      (3.0f, 3.0,  "double -> float");
    assert_float      (4.0f, 4.0l, "ld     -> float");
    assert_double     (1.0,  1,    "int    -> double");
    assert_double     (2.0,  2.0f, "float  -> double");
    assert_double     (3.0,  3.0,  "double -> double");
    assert_double     (4.0,  4.0l, "ld     -> double");
    assert_long_double(1.0l, 1,    "int    -> long double");
    assert_long_double(2.0l, 2.0f, "float  -> long double");
    assert_long_double(3.0l, 3.0,  "double -> long double");
    assert_long_double(4.0l, 4.0l, "ld     -> long double");
}

void test_constant_arithmetic_combinations() {
    // Test all combinations of int, float, double and long double constant addition
    assert_int        (2,   1    + 1,    "1    + 1,  ");
    assert_float      (2.1, 1    + 1.1f, "1    + 1.1f");
    assert_float      (2.1, 1    + 1.1,  "1    + 1.1,");
    assert_long_double(2.1, 1    + 1.1l, "1    + 1.1l");
    assert_float      (2.1, 1.1f + 1,    "1.1f + 1   ");
    assert_float      (2.2, 1.1f + 1.1f, "1.1f + 1.1f");
    assert_float      (2.2, 1.1f + 1.1,  "1.1f + 1.1,");
    assert_long_double(2.2, 1.1f + 1.1l, "1.1f + 1.1l");
    assert_float      (2.1, 1.1  + 1,    "1.1  + 1   ");
    assert_float      (2.2, 1.1  + 1.1f, "1.1  + 1.1f");
    assert_float      (2.2, 1.1  + 1.1,  "1.1  + 1.1,");
    assert_long_double(2.2, 1.1  + 1.1l, "1.1  + 1.1l");
    assert_long_double(2.1, 1.1l + 1,    "1.1l + 1   ");
    assert_long_double(2.2, 1.1l + 1.1f, "1.1l + 1.1f");
    assert_long_double(2.2, 1.1l + 1.1,  "1.1l + 1.1,");
    assert_long_double(2.2, 1.1l + 1.1l, "1.1l + 1.1l");
}

void test_constant_relops() {
    // Float vs int
    assert_int(1, 1.0f == 1, "1.0f == 1");
    assert_int(0, 1.0f != 1, "1.0f != 1");
    assert_int(1, 1.0f <  2, "1.0f <  2");
    assert_int(0, 2.0f <  1, "2.0f <  1");
    assert_int(0, 1.0f >  2, "1.0f >  2");
    assert_int(1, 2.0f >  1, "2.0f >  1");
    assert_int(1, 1.0f <= 2, "1.0f <= 2");
    assert_int(0, 2.0f <= 1, "2.0f <= 1");
    assert_int(0, 1.0f >= 2, "1.0f >= 2");
    assert_int(1, 2.0f >= 1, "2.0f >= 1");
    assert_int(1, 1.0f <= 1, "1.0f <= 1");
    assert_int(1, 1.0f <= 1, "1.0f <= 1");
    assert_int(1, 1.0f >= 1, "1.0f >= 1");
    assert_int(1, 1.0f >= 1, "1.0f >= 1");


    // Floats vs float
    assert_int(1, 1.0f == 1.0f, "1.0f == 1.0f");
    assert_int(0, 1.0f != 1.0f, "1.0f != 1.0f");
    assert_int(1, 1.0f <  2.0f, "1.0f <  2.0f");
    assert_int(0, 2.0f <  1.0f, "2.0f <  1.0f");
    assert_int(0, 1.0f >  2.0f, "1.0f >  2.0f");
    assert_int(1, 2.0f >  1.0f, "2.0f >  1.0f");
    assert_int(1, 1.0f <= 2.0f, "1.0f <= 2.0f");
    assert_int(0, 2.0f <= 1.0f, "2.0f <= 1.0f");
    assert_int(0, 1.0f >= 2.0f, "1.0f >= 2.0f");
    assert_int(1, 2.0f >= 1.0f, "2.0f >= 1.0f");
    assert_int(1, 1.0f <= 1.0f, "1.0f <= 1.0f");
    assert_int(1, 1.0f <= 1.0f, "1.0f <= 1.0f");
    assert_int(1, 1.0f >= 1.0f, "1.0f >= 1.0f");
    assert_int(1, 1.0f >= 1.0f, "1.0f >= 1.0f");

    // Double vs double
    assert_int(1, 1.0 == 1.0, "1.0 == 1.0");
    assert_int(0, 1.0 != 1.0, "1.0 != 1.0");
    assert_int(1, 1.0 <  2.0, "1.0 <  2.0");
    assert_int(0, 2.0 <  1.0, "2.0 <  1.0");
    assert_int(0, 1.0 >  2.0, "1.0 >  2.0");
    assert_int(1, 2.0 >  1.0, "2.0 >  1.0");
    assert_int(1, 1.0 <= 2.0, "1.0 <= 2.0");
    assert_int(0, 2.0 <= 1.0, "2.0 <= 1.0");
    assert_int(0, 1.0 >= 2.0, "1.0 >= 2.0");
    assert_int(1, 2.0 >= 1.0, "2.0 >= 1.0");
    assert_int(1, 1.0 <= 1.0, "1.0 <= 1.0");
    assert_int(1, 1.0 <= 1.0, "1.0 <= 1.0");
    assert_int(1, 1.0 >= 1.0, "1.0 >= 1.0");
    assert_int(1, 1.0 >= 1.0, "1.0 >= 1.0");

    // Float vs double
    assert_int(1, 1.0f == 1.0, "1.0f == 1.0");
    assert_int(0, 1.0f != 1.0, "1.0f != 1.0");
    assert_int(1, 1.0f <  2.0, "1.0f <  2.0");
    assert_int(0, 2.0f <  1.0, "2.0f <  1.0");
    assert_int(0, 1.0f >  2.0, "1.0f >  2.0");
    assert_int(1, 2.0f >  1.0, "2.0f >  1.0");
    assert_int(1, 1.0f <= 2.0, "1.0f <= 2.0");
    assert_int(0, 2.0f <= 1.0, "2.0f <= 1.0");
    assert_int(0, 1.0f >= 2.0, "1.0f >= 2.0");
    assert_int(1, 2.0f >= 1.0, "2.0f >= 1.0");
    assert_int(1, 1.0f <= 1.0, "1.0f <= 1.0");
    assert_int(1, 1.0f <= 1.0, "1.0f <= 1.0");
    assert_int(1, 1.0f >= 1.0, "1.0f >= 1.0");
    assert_int(1, 1.0f >= 1.0, "1.0f >= 1.0");

    // Float vs long double
    assert_int(1, 1.0f == 1.0L, "1.0f == 1.0L");
    assert_int(0, 1.0f != 1.0L, "1.0f != 1.0L");
    assert_int(1, 1.0f <  2.0L, "1.0f <  2.0L");
    assert_int(0, 2.0f <  1.0L, "2.0f <  1.0L");
    assert_int(0, 1.0f >  2.0L, "1.0f >  2.0L");
    assert_int(1, 2.0f >  1.0L, "2.0f >  1.0L");
    assert_int(1, 1.0f <= 2.0L, "1.0f <= 2.0L");
    assert_int(0, 2.0f <= 1.0L, "2.0f <= 1.0L");
    assert_int(0, 1.0f >= 2.0L, "1.0f >= 2.0L");
    assert_int(1, 2.0f >= 1.0L, "2.0f >= 1.0L");
    assert_int(1, 1.0f <= 1.0L, "1.0f <= 1.0L");
    assert_int(1, 1.0f <= 1.0L, "1.0f <= 1.0L");
    assert_int(1, 1.0f >= 1.0L, "1.0f >= 1.0L");
    assert_int(1, 1.0f >= 1.0L, "1.0f >= 1.0L");
}

long double test_arithmetic_cocktail1(int i, float f, double d, long double ld) {
    return i * 1000 + f * 100 + d * 10 + ld;
}

long double test_arithmetic_cocktail2(int i, float f, double d, long double ld) {
    return i + f * 10 + d * 100 + ld * 1000;
}

void test_arithmetic() {
    // These tests are complimented by the arithmetic torture tests

    float f1, f2, f3;
    double d1, d2;

    f1 = 1.1; d1 = 1.1;
    f2 = 3.2; d2 = 1.1;
    gf = 2.1; gd = 2.1;

    // Unary -
    assert_float(-1.2, -1.2L, "Float unary minus constant");
    assert_float(-1.1, -f1,   "Float unary minus local");

    // Some elaborate combinations for addition
    assert_float(2.3, f1   + 1.2f, "Float addition local-constant");
    assert_float(2.3, 1.2f + f1,   "Float addition constant-local");
    assert_float(3.3, gf   + 1.2f, "Float addition global-constant");
    assert_float(3.3, 1.2f + gf,   "Float addition constant-global");
    assert_float(4.3, f1   + f2,   "Float addition local-local");
    assert_float(3.2, f1   + gf,   "Float addition local-global");
    assert_float(3.2, gf   + f1,   "Float addition global-local");
    assert_float(4.2, gf   + gf,   "Float addition global-global");

    // A local and global are equivalent in terms of backend so it suffices to only test with locals

    // Subtraction
    assert_float(-0.1, f1   - 1.2f, "Float subtraction local-constant");
    assert_float(0.1,  1.2f - f1,   "Float subtraction constant-local");
    assert_float(-2.1, f1   - f2,   "Float subtraction local-local");

    // Multiplication
    assert_float(1.32, f1   * 1.2f, "Float multiplication local-constant");
    assert_float(1.32, 1.2f * f1,   "Float multiplication constant-local");
    assert_float(3.52, f1   * f2,   "Float multiplication local-local");

    // Division
    f1 = 10.0L;
    f2 = 20.0L;
    assert_float(2.0, f1   / 5.0f, "Float division local-constant");
    assert_float(0.5, 5.0f / f1,   "Float division constant-local");
    assert_float(0.5, f1   / f2,   "Float division local-local");

    // Combinations
    f1 = 10.1L;
    f2 = 0.50L;
    f3 = 3.0L;
    assert_float(1199.072144, -f1 * (f2 + f3) * (f1 + f2) * (f2 - f1) / f3, "Float combination");

    assert_long_double(4.6l, 1 + 1.1f + 1.2 + 1.3l, "constant int + float + double + long double");
    assert_double     (3.3,  1 + 1.1f + 1.2,        "constant int + float + double");

    assert_long_double(1234.0, test_arithmetic_cocktail1(1, 2, 3, 4), "Arithmetic cocktail 1");
    assert_long_double(4321.0, test_arithmetic_cocktail2(1, 2, 3, 4), "Arithmetic cocktail 2");
}

void test_comparison_assignment() {
    int i;
    float f1, f2;
    double d1, d2;
    long double ld;

    // Register - register
    f1 = 1.0; f2 = 1.0; assert_int(1, f1 == f2, "f1 1.0 == f2 1.0");
    f1 = 1.0; f2 = 2.0; assert_int(0, f1 == f2, "f1 1.0 == f2 2.0");
    f1 = 1.0; f2 = 1.0; assert_int(0, f1 != f2, "f1 1.0 != f2 1.0");
    f1 = 1.0; f2 = 2.0; assert_int(1, f1 != f2, "f1 1.0 != f2 2.0");
    f1 = 1.0; f2 = 1.0; assert_int(0, f1  < f2, "f1 1.0  < f2 1.0");
    f1 = 1.0; f2 = 2.0; assert_int(1, f1  < f2, "f1 1.0  < f2 2.0");
    f1 = 1.0; f2 = 1.0; assert_int(0, f1  > f2, "f1 1.0  > f2 1.0");
    f1 = 1.0; f2 = 2.0; assert_int(0, f1  > f2, "f1 1.0  > f2 2.0");
    f1 = 1.0; f2 = 1.0; assert_int(1, f1 <= f2, "f1 1.0 <= f2 1.0");
    f1 = 1.0; f2 = 2.0; assert_int(1, f1 <= f2, "f1 1.0 <= f2 2.0");
    f1 = 1.0; f2 = 1.0; assert_int(1, f1 >= f2, "f1 1.0 >= f2 1.0");
    f1 = 1.0; f2 = 2.0; assert_int(0, f1 >= f2, "f1 1.0 >= f2 2.0");

    d1 = 1.0; d2 = 1.0; assert_int(1, d1 == d2, "d1 1.0 == d2 1.0");
    d1 = 1.0; d2 = 2.0; assert_int(0, d1 == d2, "d1 1.0 == d2 2.0");
    d1 = 1.0; d2 = 1.0; assert_int(0, d1 != d2, "d1 1.0 != d2 1.0");
    d1 = 1.0; d2 = 2.0; assert_int(1, d1 != d2, "d1 1.0 != d2 2.0");
    d1 = 1.0; d2 = 1.0; assert_int(0, d1  < d2, "d1 1.0  < d2 1.0");
    d1 = 1.0; d2 = 2.0; assert_int(1, d1  < d2, "d1 1.0  < d2 2.0");
    d1 = 1.0; d2 = 1.0; assert_int(0, d1  > d2, "d1 1.0  > d2 1.0");
    d1 = 1.0; d2 = 2.0; assert_int(0, d1  > d2, "d1 1.0  > d2 2.0");
    d1 = 1.0; d2 = 1.0; assert_int(1, d1 <= d2, "d1 1.0 <= d2 1.0");
    d1 = 1.0; d2 = 2.0; assert_int(1, d1 <= d2, "d1 1.0 <= d2 2.0");
    d1 = 1.0; d2 = 1.0; assert_int(1, d1 >= d2, "d1 1.0 >= d2 1.0");
    d1 = 1.0; d2 = 2.0; assert_int(0, d1 >= d2, "d1 1.0 >= d2 2.0");

    // Constant - register
    f2 = 1.0; assert_int(1, 1.0f == f2, "cst 1.0 == f2 1.0");
    f2 = 2.0; assert_int(0, 1.0f == f2, "cst 1.0 == f2 2.0");
    f2 = 1.0; assert_int(0, 1.0f != f2, "cst 1.0 != f2 1.0");
    f2 = 2.0; assert_int(1, 1.0f != f2, "cst 1.0 != f2 2.0");
    f2 = 1.0; assert_int(0, 1.0f  < f2, "cst 1.0  < f2 1.0");
    f2 = 2.0; assert_int(1, 1.0f  < f2, "cst 1.0  < f2 2.0");
    f2 = 1.0; assert_int(0, 1.0f  > f2, "cst 1.0  > f2 1.0");
    f2 = 2.0; assert_int(0, 1.0f  > f2, "cst 1.0  > f2 2.0");
    f2 = 1.0; assert_int(1, 1.0f <= f2, "cst 1.0 <= f2 1.0");
    f2 = 2.0; assert_int(1, 1.0f <= f2, "cst 1.0 <= f2 2.0");
    f2 = 1.0; assert_int(1, 1.0f >= f2, "cst 1.0 >= f2 1.0");
    f2 = 2.0; assert_int(0, 1.0f >= f2, "cst 1.0 >= f2 2.0");

    d2 = 1.0; assert_int(1, 1.0  == d2, "cst 1.0 == d2 1.0");
    d2 = 2.0; assert_int(0, 1.0  == d2, "cst 1.0 == d2 2.0");
    d2 = 1.0; assert_int(0, 1.0  != d2, "cst 1.0 != d2 1.0");
    d2 = 2.0; assert_int(1, 1.0  != d2, "cst 1.0 != d2 2.0");
    d2 = 1.0; assert_int(0, 1.0   < d2, "cst 1.0  < d2 1.0");
    d2 = 2.0; assert_int(1, 1.0   < d2, "cst 1.0  < d2 2.0");
    d2 = 1.0; assert_int(0, 1.0   > d2, "cst 1.0  > d2 1.0");
    d2 = 2.0; assert_int(0, 1.0   > d2, "cst 1.0  > d2 2.0");
    d2 = 1.0; assert_int(1, 1.0  <= d2, "cst 1.0 <= d2 1.0");
    d2 = 2.0; assert_int(1, 1.0  <= d2, "cst 1.0 <= d2 2.0");
    d2 = 1.0; assert_int(1, 1.0  >= d2, "cst 1.0 >= d2 1.0");
    d2 = 2.0; assert_int(0, 1.0  >= d2, "cst 1.0 >= d2 2.0");

    // Register - constant
    f1 = 1.0; assert_int(1, f1 == 1.0f, "f1 1.0 == cst 1.0");
    f1 = 1.0; assert_int(0, f1 == 2.0f, "f1 1.0 == cst 2.0");
    f1 = 1.0; assert_int(0, f1 != 1.0f, "f1 1.0 != cst 1.0");
    f1 = 1.0; assert_int(1, f1 != 2.0f, "f1 1.0 != cst 2.0");
    f1 = 1.0; assert_int(0, f1  < 1.0f, "f1 1.0  < cst 1.0");
    f1 = 1.0; assert_int(1, f1  < 2.0f, "f1 1.0  < cst 2.0");
    f1 = 1.0; assert_int(0, f1  > 1.0f, "f1 1.0  > cst 1.0");
    f1 = 1.0; assert_int(0, f1  > 2.0f, "f1 1.0  > cst 2.0");
    f1 = 1.0; assert_int(1, f1 <= 1.0f, "f1 1.0 <= cst 1.0");
    f1 = 1.0; assert_int(1, f1 <= 2.0f, "f1 1.0 <= cst 2.0");
    f1 = 1.0; assert_int(1, f1 >= 1.0f, "f1 1.0 >= cst 1.0");
    f1 = 1.0; assert_int(0, f1 >= 2.0f, "f1 1.0 >= cst 2.0");

    d1 = 1.0; assert_int(1, d1 == 1.0, "d1 1.0 == cst 1.0");
    d1 = 1.0; assert_int(0, d1 == 2.0, "d1 1.0 == cst 2.0");
    d1 = 1.0; assert_int(0, d1 != 1.0, "d1 1.0 != cst 1.0");
    d1 = 1.0; assert_int(1, d1 != 2.0, "d1 1.0 != cst 2.0");
    d1 = 1.0; assert_int(0, d1  < 1.0, "d1 1.0  < cst 1.0");
    d1 = 1.0; assert_int(1, d1  < 2.0, "d1 1.0  < cst 2.0");
    d1 = 1.0; assert_int(0, d1  > 1.0, "d1 1.0  > cst 1.0");
    d1 = 1.0; assert_int(0, d1  > 2.0, "d1 1.0  > cst 2.0");
    d1 = 1.0; assert_int(1, d1 <= 1.0, "d1 1.0 <= cst 1.0");
    d1 = 1.0; assert_int(1, d1 <= 2.0, "d1 1.0 <= cst 2.0");
    d1 = 1.0; assert_int(1, d1 >= 1.0, "d1 1.0 >= cst 1.0");
    d1 = 1.0; assert_int(0, d1 >= 2.0, "d1 1.0 >= cst 2.0");

    // Constant - global
    gf2 = 1.0; assert_int(1, 1.0f == gf2, "cst 1.0 == gf2 1.0");
    gf2 = 2.0; assert_int(0, 1.0f == gf2, "cst 1.0 == gf2 2.0");
    gf2 = 1.0; assert_int(0, 1.0f != gf2, "cst 1.0 != gf2 1.0");
    gf2 = 2.0; assert_int(1, 1.0f != gf2, "cst 1.0 != gf2 2.0");
    gf2 = 1.0; assert_int(0, 1.0f  < gf2, "cst 1.0  < gf2 1.0");
    gf2 = 2.0; assert_int(1, 1.0f  < gf2, "cst 1.0  < gf2 2.0");
    gf2 = 1.0; assert_int(0, 1.0f  > gf2, "cst 1.0  > gf2 1.0");
    gf2 = 2.0; assert_int(0, 1.0f  > gf2, "cst 1.0  > gf2 2.0");
    gf2 = 1.0; assert_int(1, 1.0f <= gf2, "cst 1.0 <= gf2 1.0");
    gf2 = 2.0; assert_int(1, 1.0f <= gf2, "cst 1.0 <= gf2 2.0");
    gf2 = 1.0; assert_int(1, 1.0f >= gf2, "cst 1.0 >= gf2 1.0");
    gf2 = 2.0; assert_int(0, 1.0f >= gf2, "cst 1.0 >= gf2 2.0");

    gd2 = 1.0; assert_int(1, 1.0  == gd2, "cst 1.0 == gd2 1.0");
    gd2 = 2.0; assert_int(0, 1.0  == gd2, "cst 1.0 == gd2 2.0");
    gd2 = 1.0; assert_int(0, 1.0  != gd2, "cst 1.0 != gd2 1.0");
    gd2 = 2.0; assert_int(1, 1.0  != gd2, "cst 1.0 != gd2 2.0");
    gd2 = 1.0; assert_int(0, 1.0   < gd2, "cst 1.0  < gd2 1.0");
    gd2 = 2.0; assert_int(1, 1.0   < gd2, "cst 1.0  < gd2 2.0");
    gd2 = 1.0; assert_int(0, 1.0   > gd2, "cst 1.0  > gd2 1.0");
    gd2 = 2.0; assert_int(0, 1.0   > gd2, "cst 1.0  > gd2 2.0");
    gd2 = 1.0; assert_int(1, 1.0  <= gd2, "cst 1.0 <= gd2 1.0");
    gd2 = 2.0; assert_int(1, 1.0  <= gd2, "cst 1.0 <= gd2 2.0");
    gd2 = 1.0; assert_int(1, 1.0  >= gd2, "cst 1.0 >= gd2 1.0");
    gd2 = 2.0; assert_int(0, 1.0  >= gd2, "cst 1.0 >= gd2 2.0");

    // Global - constant
    gf1 = 1.0; assert_int(1, gf1 == 1.0f, "gf1 1.0 == cst 1.0");
    gf1 = 1.0; assert_int(0, gf1 == 2.0f, "gf1 1.0 == cst 2.0");
    gf1 = 1.0; assert_int(0, gf1 != 1.0f, "gf1 1.0 != cst 1.0");
    gf1 = 1.0; assert_int(1, gf1 != 2.0f, "gf1 1.0 != cst 2.0");
    gf1 = 1.0; assert_int(0, gf1  < 1.0f, "gf1 1.0  < cst 1.0");
    gf1 = 1.0; assert_int(1, gf1  < 2.0f, "gf1 1.0  < cst 2.0");
    gf1 = 1.0; assert_int(0, gf1  > 1.0f, "gf1 1.0  > cst 1.0");
    gf1 = 1.0; assert_int(0, gf1  > 2.0f, "gf1 1.0  > cst 2.0");
    gf1 = 1.0; assert_int(1, gf1 <= 1.0f, "gf1 1.0 <= cst 1.0");
    gf1 = 1.0; assert_int(1, gf1 <= 2.0f, "gf1 1.0 <= cst 2.0");
    gf1 = 1.0; assert_int(1, gf1 >= 1.0f, "gf1 1.0 >= cst 1.0");
    gf1 = 1.0; assert_int(0, gf1 >= 2.0f, "gf1 1.0 >= cst 2.0");

    gd1 = 1.0; assert_int(1, gd1 == 1.0, "gd1 1.0 == cst 1.0");
    gd1 = 1.0; assert_int(0, gd1 == 2.0, "gd1 1.0 == cst 2.0");
    gd1 = 1.0; assert_int(0, gd1 != 1.0, "gd1 1.0 != cst 1.0");
    gd1 = 1.0; assert_int(1, gd1 != 2.0, "gd1 1.0 != cst 2.0");
    gd1 = 1.0; assert_int(0, gd1  < 1.0, "gd1 1.0  < cst 1.0");
    gd1 = 1.0; assert_int(1, gd1  < 2.0, "gd1 1.0  < cst 2.0");
    gd1 = 1.0; assert_int(0, gd1  > 1.0, "gd1 1.0  > cst 1.0");
    gd1 = 1.0; assert_int(0, gd1  > 2.0, "gd1 1.0  > cst 2.0");
    gd1 = 1.0; assert_int(1, gd1 <= 1.0, "gd1 1.0 <= cst 1.0");
    gd1 = 1.0; assert_int(1, gd1 <= 2.0, "gd1 1.0 <= cst 2.0");
    gd1 = 1.0; assert_int(1, gd1 >= 1.0, "gd1 1.0 >= cst 1.0");
    gd1 = 1.0; assert_int(0, gd1 >= 2.0, "gd1 1.0 >= cst 2.0");

    // Global - global
    gf1 = 1.0; gf2 = 1.0; assert_int(1, gf1 == gf2, "gf1 1.0 == gf2 1.0");
    gf1 = 1.0; gf2 = 2.0; assert_int(0, gf1 == gf2, "gf1 1.0 == gf2 2.0");
    gf1 = 1.0; gf2 = 1.0; assert_int(0, gf1 != gf2, "gf1 1.0 != gf2 1.0");
    gf1 = 1.0; gf2 = 2.0; assert_int(1, gf1 != gf2, "gf1 1.0 != gf2 2.0");
    gf1 = 1.0; gf2 = 1.0; assert_int(0, gf1  < gf2, "gf1 1.0  < gf2 1.0");
    gf1 = 1.0; gf2 = 2.0; assert_int(1, gf1  < gf2, "gf1 1.0  < gf2 2.0");
    gf1 = 1.0; gf2 = 1.0; assert_int(0, gf1  > gf2, "gf1 1.0  > gf2 1.0");
    gf1 = 1.0; gf2 = 2.0; assert_int(0, gf1  > gf2, "gf1 1.0  > gf2 2.0");
    gf1 = 1.0; gf2 = 1.0; assert_int(1, gf1 <= gf2, "gf1 1.0 <= gf2 1.0");
    gf1 = 1.0; gf2 = 2.0; assert_int(1, gf1 <= gf2, "gf1 1.0 <= gf2 2.0");
    gf1 = 1.0; gf2 = 1.0; assert_int(1, gf1 >= gf2, "gf1 1.0 >= gf2 1.0");
    gf1 = 1.0; gf2 = 2.0; assert_int(0, gf1 >= gf2, "gf1 1.0 >= gf2 2.0");

    gd1 = 1.0; gd2 = 1.0; assert_int(1, gd1 == gd2, "gd1 1.0 == gd2 1.0");
    gd1 = 1.0; gd2 = 2.0; assert_int(0, gd1 == gd2, "gd1 1.0 == gd2 2.0");
    gd1 = 1.0; gd2 = 1.0; assert_int(0, gd1 != gd2, "gd1 1.0 != gd2 1.0");
    gd1 = 1.0; gd2 = 2.0; assert_int(1, gd1 != gd2, "gd1 1.0 != gd2 2.0");
    gd1 = 1.0; gd2 = 1.0; assert_int(0, gd1  < gd2, "gd1 1.0  < gd2 1.0");
    gd1 = 1.0; gd2 = 2.0; assert_int(1, gd1  < gd2, "gd1 1.0  < gd2 2.0");
    gd1 = 1.0; gd2 = 1.0; assert_int(0, gd1  > gd2, "gd1 1.0  > gd2 1.0");
    gd1 = 1.0; gd2 = 2.0; assert_int(0, gd1  > gd2, "gd1 1.0  > gd2 2.0");
    gd1 = 1.0; gd2 = 1.0; assert_int(1, gd1 <= gd2, "gd1 1.0 <= gd2 1.0");
    gd1 = 1.0; gd2 = 2.0; assert_int(1, gd1 <= gd2, "gd1 1.0 <= gd2 2.0");
    gd1 = 1.0; gd2 = 1.0; assert_int(1, gd1 >= gd2, "gd1 1.0 >= gd2 1.0");
    gd1 = 1.0; gd2 = 2.0; assert_int(0, gd1 >= gd2, "gd1 1.0 >= gd2 2.0");

    // Ternary
    f1 = 1.0;
    f2 = 2.0;
    assert_int(2, f1 < f2 ? 2 : 3, "1.0 < 2.0 ternary true");
    assert_int(3, f2 < f1 ? 2 : 3, "1.0 < 2.0 ternary false");

    // Some crazy nan tests
    float nan1, nan2;
    *((int *) &nan1) = -1;
    *((int *) &nan2) = -1;

    assert_int(0, nan1 == nan2, "nan-nan equality");
    assert_int(1, nan1 != nan2, "nan-nan inequality");

    // If one of the operands is a nan, then the comparison must return false
    assert_int(0, nan1 <  nan2, "nan1 < nan2");
    assert_int(0, nan1 >  nan2, "nan1 > nan2");
    assert_int(0, nan1 <= nan2, "nan1 <= nan2");
    assert_int(0, nan1 >= nan2, "nan1 >= nan2");
    assert_int(0, 0.0  <  nan1, "0.0 < nan1");
    assert_int(0, 0.0  >  nan1, "0.0 > nan1");
    assert_int(0, 0.0  >= nan1, "0.0 >= nan1");
    assert_int(0, 0.0  <= nan1, "0.0 <= nan1");
    assert_int(0, nan1 <  0.0,  "nan1 < 0.0");
    assert_int(0, nan1 >  0.0,  "nan1 > 0.0");
    assert_int(0, nan1 >= 0.0,  "nan1 >=0.0");
    assert_int(0, nan1 <= 0.0,  "nan1 <=0.0");

    // Combinations of types
    i = 1; f1 = 2.0; assert_int(1, i < f1, "i < f1");
    f1 = 1; d1 = 2.0; assert_int(1, f1 < d1, "f1 < d1");
    d1 = 1; ld = 2.0; assert_int(1, d1 < ld, "d1 < ld");
}

void test_comparison_conditional_jump() {
    float f1, f2;
    double d1, d2;

    f1 = 1.0;
    f2 = 2.0;
    if (f1 < f2)  assert_int(1, 1, "1.0 < 2.0 true case");  else assert_int(1, 0, "1.0 < 2.0 false case");
    if (f1 > f2)  assert_int(1, 0, "1.0 < 2.0 true case");  else assert_int(1, 1, "1.0 < 2.0 false case");
    if (f1 <= f2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (f1 >= f2) assert_int(1, 0, "1.0 <= 2.0 true case"); else assert_int(1, 1, "1.0 <= 2.0 false case");

    f2 = 1.0;
    if (f1 <= f2) assert_int(1, 1, "1.0 <= 1.0 true case"); else assert_int(1, 0, "1.0 <= 1.0 false case");
    if (f1 >= f2) assert_int(1, 1, "1.0 <= 1.0 true case"); else assert_int(1, 0, "1.0 <= 1.0 false case");

    f2 = 2.0;
    if (f1 >= f2) assert_int(1, 0, "1.0 >= 2.0 true case"); else assert_int(1, 1, "1.0 >= 2.0 false case");
    if (f1 <= f2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (f1 <= f2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (f1 >= f2) assert_int(1, 0, "1.0 >= 2.0 true case"); else assert_int(1, 1, "1.0 >= 2.0 false case");

    d1 = 1.0;
    d2 = 2.0;
    if (d1 < d2)  assert_int(1, 1, "1.0 < 2.0 true case");  else assert_int(1, 0, "1.0 < 2.0 false case");
    if (d1 > d2)  assert_int(1, 0, "1.0 < 2.0 true case");  else assert_int(1, 1, "1.0 < 2.0 false case");
    if (d1 <= d2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (d1 >= d2) assert_int(1, 0, "1.0 <= 2.0 true case"); else assert_int(1, 1, "1.0 <= 2.0 false case");

    d2 = 1.0;
    if (d1 <= d2) assert_int(1, 1, "1.0 <= 1.0 true case"); else assert_int(1, 0, "1.0 <= 1.0 false case");
    if (d1 >= d2) assert_int(1, 1, "1.0 <= 1.0 true case"); else assert_int(1, 0, "1.0 <= 1.0 false case");

    d2 = 2.0;
    if (d1 >= d2) assert_int(1, 0, "1.0 >= 2.0 true case"); else assert_int(1, 1, "1.0 >= 2.0 false case");
    if (d1 <= d2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (d1 <= d2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (d1 >= d2) assert_int(1, 0, "1.0 >= 2.0 true case"); else assert_int(1, 1, "1.0 >= 2.0 false case");

    if (f1 < 2.0)  assert_int(1, 1, "1.0 < cst 2.0 true case");  else assert_int(1, 0, "1.0 < cst 2.0 false case");
    if (2.0 > f1)  assert_int(1, 1, "cst 1.0 < 2.0 true case");  else assert_int(1, 0, "cst 1.0 < 2.0 false case");

    // Some crazy nan tests
    float nan1, nan2;
    *((int *) &nan1) = -1;
    *((int *) &nan2) = -1;

    if (nan1 ==  nan2) assert_int(0, 1, "nan comparison nan1 == nan2 true case"); else assert_int(1, 1, "nan comparison nan1 == nan2 false case");
    if (nan1 !=  nan2) assert_int(1, 1, "nan comparison nan1 != nan2 true case"); else assert_int(0, 1, "nan comparison nan1 != nan2 false case");

    // If one of the operands is a nan, then the comparison must return false
    if (nan1 <  nan2) assert_int(0, 1, "nan comparison nan1 <  nan2 true case"); else assert_int(1, 1, "nan comparison nan1 <  nan2 false case");
    if (nan1 >  nan2) assert_int(0, 1, "nan comparison nan1 >  nan2 true case"); else assert_int(1, 1, "nan comparison nan1 >  nan2 false case");
    if (nan1 <= nan2) assert_int(0, 1, "nan comparison nan1 <= nan2 true case"); else assert_int(1, 1, "nan comparison nan1 <= nan2 false case");
    if (nan1 >= nan2) assert_int(0, 1, "nan comparison nan1 >= nan2 true case"); else assert_int(1, 1, "nan comparison nan1 >= nan2 false case");
    if (0.0  <  nan1) assert_int(0, 1, "nan comparison 0.0  <  nan1 true case"); else assert_int(1, 1, "nan comparison 0.0  <  nan1 false case");
    if (0.0  >  nan1) assert_int(0, 1, "nan comparison 0.0  >  nan1 true case"); else assert_int(1, 1, "nan comparison 0.0  >  nan1 false case");
    if (0.0  >= nan1) assert_int(0, 1, "nan comparison 0.0  >= nan1 true case"); else assert_int(1, 1, "nan comparison 0.0  >= nan1 false case");
    if (0.0  <= nan1) assert_int(0, 1, "nan comparison 0.0  <= nan1 true case"); else assert_int(1, 1, "nan comparison 0.0  <= nan1 false case");
    if (nan1 <  0.0 ) assert_int(0, 1, "nan comparison nan1 <  0.0  true case"); else assert_int(1, 1, "nan comparison nan1 <  0.0  false case");
    if (nan1 >  0.0 ) assert_int(0, 1, "nan comparison nan1 >  0.0  true case"); else assert_int(1, 1, "nan comparison nan1 >  0.0  false case");
    if (nan1 >= 0.0 ) assert_int(0, 1, "nan comparison nan1 >= 0.0  true case"); else assert_int(1, 1, "nan comparison nan1 >= 0.0  false case");
    if (nan1 <= 0.0 ) assert_int(0, 1, "nan comparison nan1 <= 0.0  true case"); else assert_int(1, 1, "nan comparison nan1 <= 0.0  false case");
}

void test_jz_jnz() {
    int i;

    float fzero = 0.0f;
    float fone = 1.0f;

    float fnan;
    *((int *) &fnan) = -1;

    i = 1; assert_int(0, fzero && i++, "float zero && 1 result"); assert_int(1, i, "float zero && 1 i");
    i = 1; assert_int(1, fzero || i++, "float zero || 0 result"); assert_int(2, i, "float zero || 0 i");
    i = 1; assert_int(1, fone  && i++, "float one  && 1 result"); assert_int(2, i, "float one  && 1 i");
    i = 1; assert_int(1, fone  || i++, "float one  || 0 result"); assert_int(1, i, "float one  || 0 i");
    i = 1; assert_int(1, fnan  && i++, "float nan  && 1 result"); assert_int(2, i, "float nan  && 1 i");
    i = 1; assert_int(1, fnan  || i++, "float nan  || 0 result"); assert_int(1, i, "float nan  || 0 i");

    double dzero = 0.0;
    double done = 1.0;

    double dnan;
    *((long *) &dnan) = -1;

    i = 1; assert_int(0, dzero && i++, "double zero && 1 result"); assert_int(1, i, "double zero && 1 i");
    i = 1; assert_int(1, dzero || i++, "double zero || 0 result"); assert_int(2, i, "double zero || 0 i");
    i = 1; assert_int(1, done  && i++, "double one  && 1 result"); assert_int(2, i, "double one  && 1 i");
    i = 1; assert_int(1, done  || i++, "double one  || 0 result"); assert_int(1, i, "double one  || 0 i");
    i = 1; assert_int(1, dnan  && i++, "double nan  && 1 result"); assert_int(2, i, "double nan  && 1 i");
    i = 1; assert_int(1, dnan  || i++, "double nan  || 0 result"); assert_int(1, i, "double nan  || 0 i");
}

void test_pointers() {
    float f, *pf;
    double d, *pd;
    char *buffer = malloc(100);

    f = 1.1;
    d = 2.1;

    // Make a nan, for the hell of it
    *((int *) &f) = -1;
    sprintf(buffer, "%f", f);
    assert_int(0, strcmp(buffer, "-nan"), "-nan");

    f = 1.1;
    gf = 2.1;
    d = 3.1;
    gd = 4.1;

    // Local pointer to local
    pf = &f; pd = &d;
    assert_float (1.1f, *pf,     "Local -> local float deref");
    assert_float (2.1f, *pf + 1, "Local -> local float deref + 1");
    assert_double(3.1f, *pd,     "Local -> local double deref");
    assert_double(4.1f, *pd + 1, "Local -> local double deref + 1");

    // Local pointer to global
    pf = &gf; pd = &gd;
    assert_float (2.1, *pf, "Local -> global float deref");
    assert_double(4.1, *pd, "Local -> global double deref");

    // Global pointer to local
    gpf = &f; gpd = &d;
    assert_float (1.1, *gpf, "Global -> local float deref");
    assert_double(3.1, *gpd, "Global -> local double deref");

    // Global pointer to global
    gpf = &gf; gpd = &gd;
    assert_float (2.1, *gpf, "Global -> global float deref");
    assert_double(4.1, *gpd, "Global -> global double deref");

    // Assignments to dereferenced pointers from a constant
    pf  = &f; *pf  = 1.1; assert_float(1.1, *pf,      "*pf");
              *pf  = 2.1; assert_float(3.1, *pf + 1," *pf + 1");
    gpf = &f; *gpf = 3.1; assert_float(3.1, *gpf,     "*gpf");

    pd  = &d; *pd  = 7.1; assert_float(7.1, *pd,      "*pd");
              *pd  = 8.1; assert_float(9.1, *pd + 1," *pd + 1");
    gpd = &d; *gpd = 9.1; assert_float(9.1, *gpd,     "*gpd");

    // Assignments to dereferenced pointers from a local/global
    pf = &f; gpf = &gf;
    *pf  = gf; assert_float(2.1, *pf,  "*pf = gf");
    *gpf = d;  assert_float(9.1, *gpf, "*gpf = d");

    pd = &d; gpd = &gd;
    *pd  = gd; assert_double(4.1, *pd,  "*pd = gd");
    *gpd = f;  assert_double(2.1, *gpd, "*gpd = f");

    assert_float(3.1,  *pf  + 1, "*pf  + 1");
    assert_float(10.1, *gpf + 1, "*gpf + 1");

    assert_double(5.1, *pd  + 1, "*pd  + 1");
    assert_double(3.1, *gpd + 1, "*gpd + 1");

    float f2, *pf2;
    pf2 = &f2;
    *pf2 = *pf;  assert_float(2.1, *pf2, "*pf2 = *pf");
    *pf2 = *gpf; assert_float(9.1, *pf2, "*pf2 = *gpf");

    double d2, *pd2;
    pd2 = &d2;
    *pd2 = *pf;  assert_double(2.1, *pd2, "*pd2 = *pf");
    *pd2 = *gpd; assert_double(2.1, *pd2, "*pd2 = *gpd");

    // Mixed dereference
    pd2 = &d2; pf2 = &f2;
    d2 = 12.1; f = *pd2; assert_float(12.1, f, "*f = pd2");
    f2 = 13.1; d = *pf2; assert_float(13.1, d, "*d = pf2");

    // Malloc tests
    pf  = malloc(sizeof(float)); *pf  = 5.1L; assert_float(5.1, *pf,  "*pf from malloc");
    gpf = malloc(sizeof(float)); *gpf = 6.1L; assert_float(6.1, *gpf, "*gpf from malloc");

    float *pf3 = malloc(sizeof(float));
    *pf = 1.1;
    *pf2 = 1.2;
    *pf3 = *pf + *pf2; assert_float(2.3,  *pf3, "*f3 = *pf + *pf2");
    *pf3 = *pf - *pf2; assert_float(-0.1, *pf3, "*f3 = *pf - *pf2");

    // Double dereference
    float **ppf;
    f = 1.1L;
    pf = &f;
    ppf = &pf;
    assert_float(1.1, *pf  , "*pf");
    assert_float(1.1, **ppf, "**ppld");

    double **ppd;
    d = 2.1L;
    pd = &d;
    ppd = &pd;
    assert_float(2.1, *pd  , "*pd");
    assert_float(2.1, **ppd, "**ppd");

    // Assignment of constant to memory
    gpf = 0;
    gpf = 0ul;
    gpd = 0;
    gpd = 0ul;

}

// The following code is the fast inverse square root implementation from Quake III Arena,
// stripped of C preprocessor directives, but including the exact original comment text
// See https://en.wikipedia.org/wiki/Fast_inverse_square_root
float Q_rsqrt( float number )
{
    long i;
    float x2, y;
    float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    i  = * ( long * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
    y  = * ( float * ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//  y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

    return y;
}

void test_test_carmacks_inverse_square_root() {
    assert_float(0.332953, Q_rsqrt(9), "Carmack's inverse square root");
}

void test_pointer_arithmetic() {
    float *pf = malloc(2 * sizeof(float));
    float *pf1 = pf + 1;

    pf[0] = 1.1;
    pf[1] = 2.1;
    int i = 1;
    assert_float(1.1, pf[0],      "pf[0]");
    assert_float(2.1, pf[1],      "pf[1]");
    assert_float(2.1, pf[i],      "pf[i]");
    assert_float(2.1, *(pf + 1),  "*(pf + 1)");
    assert_float(2.1, *(1 + pf),  "*(1 + pf)");
    assert_float(1.1, *(pf1 - 1), "*(pf1 - 1)");

    assert_int(1, pf1 - pf, "pf1 - pf");

    double *pd = malloc(2 * sizeof(double));
    double *pd1 = pd + 1;

    pd[0] = 1.1;
    pd[1] = 2.1;
    assert_double(1.1, pd[0],      "pd[0]");
    assert_double(2.1, pd[1],      "pd[1]");
    assert_double(2.1, pd[i],      "pd[i]");
    assert_double(2.1, *(pd + 1),  "*(pd + 1)");
    assert_double(2.1, *(1 + pd),  "*(1 + pd)");
    assert_double(1.1, *(pd1 - 1), "*(pd1 - 1)");

    assert_int(1, pd1 - pd, "pd1 - pd");
}

void test_pointer_casting() {
    struct s1* s1 = malloc(sizeof(struct s1));

    s1->f = 1.1; assert_int(1066192077, *((int *) &s1->f), "float to int cast");
    s1->d = 1.1; assert_int(-1717986918, *((int *) &s1->d), "double to long cast");
}

void test_structs() {
    struct s1 *s1 = malloc(sizeof(struct s1));
    struct sf *sf = malloc(sizeof(struct sf));
    struct sd *sd = malloc(sizeof(struct sd));

    s1->f = 1.1; sf->f = s1->f; assert_float(1.1, sf->f, "sf->f");
    sf->f = 2.1; s1->f = sf->f; assert_float(2.1, s1->f, "s1->f");

    s1->d = 1.1; sd->d = s1->d; assert_double(1.1, sd->d, "sd->d");
    sd->d = 2.1; s1->d = sd->d; assert_double(2.1, s1->d, "s1->d");
}

void test_inc_dec() {
    float f = 1.0;

    ++f; assert_float(2.0, f, "Float prefix ++");
    --f; assert_float(1.0, f, "Float prefix --");
    f++; assert_float(2.0, f, "Float postfix ++");
    f--; assert_float(1.0, f, "Float postfix --");

    gf = 1.0;
    ++gf; assert_float(2.0, gf, "Global float prefix ++");
    --gf; assert_float(1.0, gf, "Global float prefix --");
    gf++; assert_float(2.0, gf, "Global float postfix ++");
    gf--; assert_float(1.0, gf, "Global float postfix --");

    double d = 1.0;

    ++d; assert_double(2.0, d, "Double prefix ++");
    --d; assert_double(1.0, d, "Double prefix --");
    d++; assert_double(2.0, d, "Double postfix ++");
    d--; assert_double(1.0, d, "Double postfix --");

    gd = 2.0;
    ++gd; assert_double(3.0, gd, "Global double prefix ++");
    --gd; assert_double(2.0, gd, "Global double prefix --");
    gd++; assert_double(3.0, gd, "Global double postfix ++");
    gd--; assert_double(2.0, gd, "Global double postfix --");
}

// Test bug where a 4 bytes were allocated on the stack instead of 8 when converting
// a long double to a double
int test_stack_smashing_bug() {
    int i, j; &i; &j;
    long l, m; &l; &m;

    i = j = l = m = -1;

    long double ld = 1.0;
    double d = ld; &d;
    assert_int(-1, i, "LD -> SSE conversion allocation bug 1");
    assert_int(-1, j, "LD -> SSE conversion allocation bug 2");
    assert_long(-1, l, "LD -> SSE conversion allocation bug 3");
    assert_long(-1, m, "LD -> SSE conversion allocation bug 4");
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
    test_conversion_long_double_to_sse();
    test_spilling();
    test_long_double_constant_promotion_in_arithmetic();
    test_constants_in_function_calls();
    test_constant_arithmetic_combinations();
    test_constant_relops();
    test_arithmetic();
    test_comparison_assignment();
    test_comparison_conditional_jump();
    test_jz_jnz();
    test_pointers();
    test_test_carmacks_inverse_square_root();
    test_pointer_arithmetic();
    test_pointer_casting();
    test_structs();
    test_inc_dec();
    test_stack_smashing_bug();

    finalize();
}
