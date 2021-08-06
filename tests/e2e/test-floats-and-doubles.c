#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

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
    // In registers. Variables cannot be reused, otherwise they would get spilled onto the stack
    // due to function calls.
    float f1  = 1.0f; float  f2 = f1; assert_float (1.0f, f2, "float -> float assignment");
    float f3  = 2.0f; double d1 = f3; assert_double(2.0,  d1, "float -> double assignment");
    double d2 = 3.0;  float  f4 = d2; assert_float (3.0f, f4, "double -> float assignment");
    double d3 = 4.0;  double d4 = d3; assert_double(4.0,  d4, "double -> double assignment");
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
    test_spilling();
    test_long_double_constant_promotion_in_arithmetic();
    test_constants_in_function_calls();

    finalize();
}
