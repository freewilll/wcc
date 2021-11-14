#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

extern int verbose;
extern int passes;
extern int failures;

void assert_int(int expected, int actual, char *message) {
    if (expected != actual) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %d got %d\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

void assert_long(long expected, long actual, char *message) {
    if (expected != actual) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %ld got %ld\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

int float_eq(float expected, float actual) {
    if (isnan(expected) != isnan(actual)) return 0;

    float diff = expected - actual;
    if (diff < 0) diff = -diff;
    return diff <= 0.0001;
}

void assert_float(float expected, float actual, char *message) {
    int eq = float_eq(expected, actual);

    if (!float_eq(expected, actual)) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %f got %f\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

void assert_double(double expected, double actual, char *message) {
    if (isnan(expected) != isnan(actual)) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %f got %f\n", expected, actual);
        return;
    }

    double diff = expected - actual;
    if (diff < 0) diff = -diff;
    if (diff > 0.0001) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %f got %f\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

void assert_long_double(long double expected, long double actual, char *message) {
    if (isnan(expected) != isnan(actual)) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %Lf got %Lf\n", expected, actual);
        return;
    }

    long double diff = expected - actual;
    if (diff < 0) diff = -diff;
    if (diff > 0.0001) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %Lf got %Lf\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

void assert_string(char *expected, char *actual, char *message) {
    if (strcmp(expected, actual)) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected \"%s\" got \"%s\"\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

void finalize() {
    if (failures == 0) {
        printf("All %d tests passed\n", passes + failures);
    }
    else {
        printf("%d out of %d tests failed\n", failures, passes + failures);
        exit(1);
    }
}

void parse_args(int argc, char **argv, int *verbose) {
    int help;

    help = *verbose = 0;

    argc--;
    argv++;
    while (argc > 0 && *argv[0] == '-') {
             if (argc > 0 && !memcmp(argv[0], "-h",   3)) { help = 0;     argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-v",   2)) { *verbose = 1; argc--; argv++; }
        else {
            printf("Unknown parameter %s\n", argv[0]);
            exit(1);
        }
    }

    if (help) {
        printf("Usage: test-wcc [-v]\n\n");
        printf("Flags\n");
        printf("-v      Verbose mode; show all tests\n");
        printf("-h      Help\n");
        exit(1);
    }
}
