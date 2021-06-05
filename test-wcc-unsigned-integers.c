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
    unsigned long l1, l2;

    i1 = 0x7fffffff;
    i2 = i1 + 1;
    assert_int(0x80000000, i2, "add");

    i1 = 0x40000000;
    i2 = i1  * 3;
    assert_int(0xc0000000, i2, "mul");

    i1 = 0xaaaaaaaa;
    i2 = i1 | 0x55555555;
    assert_int(0xffffffff, i2, "or");

    i1 = 0xffffffff;
    i2 = i1 & 0x55555555;
    assert_int(0x55555555, i2, "and");

    i1 = 0xffffffff;
    i2 = i1 ^ 0x55555555;
    assert_int(0xaaaaaaaa, i2, "xor");

    // Division & modulo
    i1 = 0xffffffff;
    i2 = 0xfffffff;
    assert_int(0x10, i1  / i2, "unsigned int division");
    assert_int(0xf,  i1  % i2, "unsigned int modulo");

    l1 = 0xffffffffffffffff;
    l2 = 0xfffffffffffffff;
    assert_long(0x10, l1  / l2, "unsigned long division");
    assert_long(0xf,  l1  % l2, "unsigned long modulo");

    assert_long(0x10, 0xffffffffffffffffu / 0xfffffffffffffffu, "unsigned long constant division");
    assert_long(0xf,  0xffffffffffffffffu % 0xfffffffffffffffu, "unsigned long constant modulo");

    // Comparisons
    // Note: the ul is needed since otherwise the parser will add a move to go from (int) 0 -> (unsigned int) 0.
    // This move will be removed in the future, and when done, the ul can be removed.
    assert_int(0, 0xffffffffffffffff < 0ul,  "-1 < 0");
    assert_int(0, 0xffffffffffffffff <= 0ul, "-1 <= 0");
    assert_int(0, 0ul >  0xffffffffffffffff, "0 > -1");
    assert_int(0, 0ul >= 0xffffffffffffffff, "0 >= -1");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_uint_uint_comparison();
    test_operations();

    finalize();
}
