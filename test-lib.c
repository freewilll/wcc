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
