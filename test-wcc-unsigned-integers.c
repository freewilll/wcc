#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

int failures;
int passes;
int verbose;

long gl;

void test_uint_uint_comparison() {
    unsigned int i, j;

    i = -1;
    j = 0;

    assert_int(0, i < j, "i < j");
}

void test_operations() {
    unsigned int i1, i2;

    i1 = 0x7fffffff;
    i2 = i1 + 1;
    assert_int(i2, 0x80000000, "add");

    i1 = 0x40000000;
    i2 = i1  * 3;
    assert_int(i2, 0xc0000000, "mul");

    i1 = 0xaaaaaaaa;
    i2 = i1 | 0x55555555;
    assert_int(i2, 0xffffffff, "or");

    i1 = 0xffffffff;
    i2 = i1 & 0x55555555;
    assert_int(i2, 0x55555555, "and");

    i1 = 0xffffffff;
    i2 = i1 ^ 0x55555555;
    assert_int(i2, 0xaaaaaaaa, "xor");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_uint_uint_comparison();
    test_operations();

    finalize();
}
