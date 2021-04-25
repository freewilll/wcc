#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

int verbose;
int passes;
int failures;

struct s1 {
    int i;
};

struct sl { long  i, j; long  k; };

struct sas {
    long i, j;
};

void test_char_pointer_arithmetic() {
    char *pc;
    int *start;
    pc = malloc(4);
    start = pc;
    *pc++ = 'f';
    *pc++ = 'o';
    *pc++ = 'o';
    *pc++ = 0;
    assert_int(0, strcmp(start, "foo"), "char pointer arithmetic");
}

void test_pointer_pointer_subtraction1() {
    char *c1, *c2;
    short *s1, *s2;
    int *i1, *i2;
    long *l1, *l2;
    struct sl *t1, *t2;

    c1 = (char *)       16; c2 = (char *)      8; assert_int(8, c1 - c2, "Pointer pointer subtraction 1-1");
    s1 = (short *)      16; s2 = (short *)     8; assert_int(4, s1 - s2, "Pointer pointer subtraction 1-2");
    i1 = (int *)        16; i2 = (int *)       8; assert_int(2, i1 - i2, "Pointer pointer subtraction 1-3");
    l1 = (long *)       16; l2 = (long *)      8; assert_int(1, l1 - l2, "Pointer pointer subtraction 1-4");
    t1 = (struct sl *)  56; t2 = (struct sl *) 8; assert_int(2, t1 - t2, "Pointer pointer subtraction 1-5");
}

int pps2() { return 1; }

void t(char *p1, char *p2) {
    int i;

    i = (p1 - p2) + pps2();
    assert_int(2, p1 - p2, "foo");
}

void test_pointer_pointer_subtraction2() {
    int i;

    t(3, 1);
}

void test_dereferenced_pointer_inc_dec() {
    struct s1 *s;

    s = malloc(sizeof(struct s1));
    s->i = 0;
    s->i++;    assert_int(1, s->i, "dereferenced ptr 1");
    ++s->i;    assert_int(2, s->i, "dereferenced ptr 2");
    s->i += 1; assert_int(3, s->i, "dereferenced ptr 3");

    s->i--;    assert_int(2, s->i, "dereferenced ptr 4");
    --s->i;    assert_int(1, s->i, "dereferenced ptr 5");
    s->i -= 1; assert_int(0, s->i, "dereferenced ptr 6");
}

void test_pointer_with_non_constant_non_pointer_addition() {
    int *pi;
    int j;

    pi = malloc(sizeof(int));
    j = 0;
    *(pi + j) = 1;
    assert_int(1, *pi, "Pointer with non-constant non-pointer addition");
}

void test_struct_additions() {
    struct sas *s;

    s = 0;
    s++;

    assert_long(16, (long) s,       "struct additions 1");
    assert_long(32, (long) (s + 1), "struct additions 2");
    assert_long(16, (long) s++,     "struct additions 3");
    assert_long(48, (long) ++s,     "struct additions 4");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_char_pointer_arithmetic();
    test_pointer_pointer_subtraction1();
    test_pointer_pointer_subtraction2();
    test_dereferenced_pointer_inc_dec();
    test_pointer_with_non_constant_non_pointer_addition();
    test_struct_additions();

    finalize();
}
