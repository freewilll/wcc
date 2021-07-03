#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

#ifdef FLOATS
long double gld;

void test_assignment() {
    long double ld1, ld2;

    char *buffer = malloc(100);

    ld1 = 1.1;
    ld2 = ld1;
    sprintf(buffer, "%5.5Lf", ld2);
    assert_int(0, strcmp(buffer, "1.10000"), "Long double assignment local-local");

    gld = ld1;
    sprintf(buffer, "%5.5Lf", gld);
    assert_int(0, strcmp(buffer, "1.10000"), "Long double assignment local-global");

    gld = 1.2;
    ld1 = gld;
    sprintf(buffer, "%5.5Lf", ld1);
    assert_int(0, strcmp(buffer, "1.20000"), "Long double assignment global-local");

    gld = 1.3;
    ld1 = ld2 = gld;
    sprintf(buffer, "%5.5Lf %5.5Lf", ld1, ld2);
    assert_int(0, strcmp(buffer, "1.30000 1.30000"), "Long double assignment global-local-local");
}

void test_arithmetic() {
    long double ld1, ld2;

    char *buffer = malloc(100);

    ld1 = 1.1;
    ld2 = 3.2;
    gld = 2.1;

    // Some elaborate combinations for addition
    sprintf(buffer, "%5.5Lf", ld1 + 1.2); assert_int(0, strcmp(buffer, "2.30000"), "Long double addition local-constant");
    sprintf(buffer, "%5.5Lf", 1.2 + ld1); assert_int(0, strcmp(buffer, "2.30000"), "Long double addition constant-local");
    sprintf(buffer, "%5.5Lf", gld + 1.2); assert_int(0, strcmp(buffer, "3.30000"), "Long double addition global-constant");
    sprintf(buffer, "%5.5Lf", 1.2 + gld); assert_int(0, strcmp(buffer, "3.30000"), "Long double addition constant-global");
    sprintf(buffer, "%5.5Lf", ld1 + ld2); assert_int(0, strcmp(buffer, "4.30000"), "Long double addition local-local");
    sprintf(buffer, "%5.5Lf", ld1 + gld); assert_int(0, strcmp(buffer, "3.20000"), "Long double addition local-global");
    sprintf(buffer, "%5.5Lf", gld + ld1); assert_int(0, strcmp(buffer, "3.20000"), "Long double addition global-local");
    sprintf(buffer, "%5.5Lf", gld + gld); assert_int(0, strcmp(buffer, "4.20000"), "Long double addition global-global");

    // A local and global are equivalent in terms of backend so it suffices to only test with locals

    // Subtraction
    sprintf(buffer, "%5.5Lf", ld1 - 1.2); assert_int(0, strcmp(buffer, "-0.10000"), "Long double subtraction local-constant");
    sprintf(buffer, "%5.5Lf", 1.2 - ld1); assert_int(0, strcmp(buffer, "0.10000"),  "Long double subtraction constant-local");
    sprintf(buffer, "%5.5Lf", ld1 - ld2); assert_int(0, strcmp(buffer, "-2.10000"), "Long double subtraction local-local");

    // Multiplication
    sprintf(buffer, "%5.5Lf", ld1 * 1.2); assert_int(0, strcmp(buffer, "1.32000"), "Long double multiplication local-constant");
    sprintf(buffer, "%5.5Lf", 1.2 * ld1); assert_int(0, strcmp(buffer, "1.32000"), "Long double multiplication constant-local");
    sprintf(buffer, "%5.5Lf", ld1 * ld2); assert_int(0, strcmp(buffer, "3.52000"), "Long double multiplication local-local");

    // Division
    ld1 = 10.0L;
    ld2 = 20.0L;
    sprintf(buffer, "%5.5Lf", ld1 / 5.0); assert_int(0, strcmp(buffer, "2.00000"), "Long double division local-constant");
    sprintf(buffer, "%5.5Lf", 5.0 / ld1); assert_int(0, strcmp(buffer, "0.50000"),  "Long double division constant-local");
    sprintf(buffer, "%5.5Lf", ld1 / ld2); assert_int(0, strcmp(buffer, "0.50000"), "Long double division local-local");
}

#endif

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    #ifdef FLOATS
    test_assignment();
    test_arithmetic();
    #endif

    finalize();
}
