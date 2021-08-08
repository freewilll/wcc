#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

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

void assert_float(float expected, float actual, char *message) {
    float diff = expected - actual;
    if (diff < 0) diff = -diff;
    if (diff > 0.000001) {
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
    double diff = expected - actual;
    if (diff < 0) diff = -diff;
    if (diff > 0.000001) {
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
    long double diff = expected - actual;
    if (diff < 0) diff = -diff;
    if (diff > 0.000001) {
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

// fwip move to main test code when float/double params are implemented
void test_floats_function_call(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9) {
    #ifdef FLOAT
    assert_float(1.0, f1, "test_floats_function_call 1");
    assert_float(2.0, f2, "test_floats_function_call 2");
    assert_float(3.0, f3, "test_floats_function_call 3");
    assert_float(4.0, f4, "test_floats_function_call 4");
    assert_float(5.0, f5, "test_floats_function_call 5");
    assert_float(6.0, f6, "test_floats_function_call 6");
    assert_float(7.0, f7, "test_floats_function_call 7");
    assert_float(8.0, f8, "test_floats_function_call 8");
    assert_float(9.0, f9, "test_floats_function_call 9");
    #endif
}

void test_doubles_function_call(double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8, double d9) {
    assert_double(1.0, d1, "test_doubles_function_call 1");
    assert_double(2.0, d2, "test_doubles_function_call 2");
    assert_double(3.0, d3, "test_doubles_function_call 3");
    assert_double(4.0, d4, "test_doubles_function_call 4");
    assert_double(5.0, d5, "test_doubles_function_call 5");
    assert_double(6.0, d6, "test_doubles_function_call 6");
    assert_double(7.0, d7, "test_doubles_function_call 7");
    assert_double(8.0, d8, "test_doubles_function_call 8");
    assert_double(9.0, d9, "test_doubles_function_call 9");
}
