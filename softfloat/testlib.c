#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "softfloat.h"

int failures = 0;

void assert_int(int expected, int actual, char *message) {
    if (expected != actual) {
        printf("%-60s ", message);
        printf("failed, expected %d got %d\n", expected, actual);
        failures++;
    }
}
void assert_long(long expected, long actual, char *message) {
    if (expected != actual) {
        printf("%-60s ", message);
        printf("failed, expected %ld got %ld\n", expected, actual);
        failures++;
    }
}

void assert_float(float expected, float actual, char *message) {
    if (isnan(expected) != isnan(actual)) {
        printf("%-60s ", message);
        printf("failed, expected %f got %f\n", expected, actual);
        failures++;
    }

    long double diff = expected - actual;
    if (diff < 0) diff = -diff;
    if (diff > 0.0001) {
        printf("%-60s ", message);
        printf("failed, expected %f got %f\n", expected, actual);
        failures++;
    }
}

void assert_double(double expected, double actual, char *message) {
    if (isnan(expected) != isnan(actual)) {
        printf("%-60s ", message);
        printf("failed, expected %f got %f\n", expected, actual);
        failures++;
    }

    long double diff = expected - actual;
    if (diff < 0) diff = -diff;
    if (diff > 0.0001) {
        printf("%-60s ", message);
        printf("failed, expected %f got %f\n", expected, actual);
        failures++;
    }
}

void assert_long_double(long double expected, long double actual, char *message) {
    if (isnan(expected) != isnan(actual)) {
        printf("%-60s ", message);
        printf("failed, expected %Lf got %Lf\n", expected, actual);
        failures++;
    }

    long double diff = expected - actual;
    if (diff < 0) diff = -diff;
    if (diff > 0.0001) {
        printf("%-60s ", message);
        printf("failed, expected %Lf got %Lf\n", expected, actual);
        failures++;
    }
}

void assert_ld_string(long double ld, char *expected, char *message) {
    char *buffer = malloc(100);

    sprintf(buffer, "%.8Le", ld);

    int matches = !strcmp(buffer, expected);
    if (!matches) printf("Expected \"%s\", got \"%s\"\n", expected, buffer);
    assert_int(1, matches, message);
}

void print_significand(__uint128_t s)  {
    int sb1 = SIGNIFICAND_BITS - 1;
    __uint128_t mask = (((__uint128_t) 1) << sb1);
    for (int i = sb1; i >= 0; i--) {
        int v = (s & mask) != 0;
        printf("%d", v);

        // Print '|' on 32-bit, 64-bit and 128-bit boundaries
        if (sb1 - i + 1 == binary32_encoding.significand_bits) printf(" ");
        if (sb1 - i + 1 == binary64_encoding.significand_bits) printf(" ");
        if (sb1 - i + 1 == binary128_encoding.significand_bits) printf(" ");
        mask = mask >> 1;
    }
}

void print_fpv(FpValue *fpv) {
    if (fpv->type == TYPE_ZERO || fpv->type == TYPE_SUBNORMAL) {
        printf("%d        0.", fpv->sign);
        print_significand(fpv->significand);
    }
    else if (fpv->type == TYPE_INF) {
        printf("%d        inf", fpv->sign);
        for (int i = 0; i < 111; i++) printf(" ");
    }
    else if (fpv->type == TYPE_NAN) {
        printf("%d %6d N.", fpv->sign, fpv->exponent);
        print_significand(fpv->significand);
    }
    else { // Normal
        printf("%d %6d 1.", fpv->sign, fpv->exponent);
        print_significand(fpv->significand);
    }

    printf("\n");
}

void finish_tests() {
    if (failures) {
        printf("There were %d failures\n", failures);
        exit(1);
    }

    printf("All tests passed\n");
}