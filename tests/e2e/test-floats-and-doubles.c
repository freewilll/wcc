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
    int i1 = 1;                 assert_int(i1, 1, "constant assignment int    -> int");
    int i2 = 1.0f;              assert_int(i2, 1, "constant assignment float  -> int");
    int i3 = 1.0;               assert_int(i3, 1, "constant assignment double -> int");
    int i4 = 1.0l;              assert_int(i4, 1, "constant assignment ld     -> int");
    float f1 = 2;               // fwip: add assertions
    float f2 = 2.0f;            // fwip: add assertions
    float f3 = 2.0;             // fwip: add assertions
    float f4 = 2.0l;            // fwip: add assertions
    double d1 = 3;              // fwip: add assertions
    double d2 = 3.0f;           // fwip: add assertions
    double d3 = 3.0;            // fwip: add assertions
    double d4 = 3.0l;           // fwip: add assertions
    long double ld1 = 4;        assert_ld_string(ld1, "4.00000", "constant assignment int    -> ld");
    long double ld2 = 4.0f;     assert_ld_string(ld2, "4.00000", "constant assignment float  -> ld");
    long double ld3 = 4.0;      assert_ld_string(ld3, "4.00000", "constant assignment double -> ld");
    long double ld4 = 4.0l;     assert_ld_string(ld4, "4.00000", "constant assignment ld     -> ld");
}

void test_long_double_constant_assignment() {
    long double ld;

    ld = 1.0f; assert_ld_string(ld, "1.00000", "constant assignment to long double 1.0");
    ld = 1.0;  assert_ld_string(ld, "1.00000", "constant assignment to long double 1.0");
}

void test_long_double_constant_promotion_in_arithmetic() {
    long double ld;

    ld = 1.0;
    assert_ld_string(ld * 1.0f, "1.00000", "constant float promotion to long double in arithmetic");
    assert_ld_string(ld * 1.0,  "1.00000", "constant double promotion to long double in arithmetic");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_constant_assignment();
    test_long_double_constant_assignment();
    test_long_double_constant_promotion_in_arithmetic();

    finalize();
}
