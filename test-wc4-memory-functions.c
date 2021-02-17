#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

int verbose;
int passes;
int failures;

void test_malloc() {
    int *pi;

    pi = malloc(32);
    *pi = 1;
    *++pi = 2;
    *++pi = -1; // Gets overwritten
    *pi++ = 3;
    *pi = 4;

    assert_int(1, *(pi - 3), "malloc 1");
    assert_int(2, *(pi - 2), "malloc 2");
    assert_int(3, *(pi - 1), "malloc 3");
    assert_int(4, *pi,       "malloc 4");
}


void test_free() {
    int *pi;
    pi = malloc(17);
    free(pi);
}

void test_mem_functions() {
    long *pi1, *pi2;

    pi1 = malloc(32);
    pi2 = malloc(32);
    memset(pi1, 0, 32);
    assert_int(0, pi1[0], "memory functions 1");
    assert_int(0, pi1[3], "memory functions 2");
    memset(pi1, -1, 32);
    assert_int(-1, pi1[0], "memory functions 2");
    assert_int(-1, pi1[3], "memory functions 3");

    assert_int(255, memcmp(pi1, pi2, 32), "memory functions 4");

    // gcc's strcmp is builtin and returns different numbers than the
    // std library's. Only the sign is the same. Hence, we have to use <
    // and > instead of ==.
    assert_int(1, strcmp("foo", "foo") == 0, "memory functions 5");
    assert_int(1, strcmp("foo", "aaa")  > 0, "memory functions 6");
    assert_int(1, strcmp("foo", "ggg")  < 0, "memory functions 7");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_malloc();
    test_free();
    test_mem_functions();

    finalize();
}

