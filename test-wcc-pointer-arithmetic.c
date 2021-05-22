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

void test_brutal_pointer_arithmetic() {
    char c;
    char *pc;
    char **ppc;
    int s;
    int *ps;
    int **pps;
    int i;
    int *pi;
    int **ppi;
    int l;
    int *pl;
    int **ppl;

    c = 0;
    pc = 0;
    ppc = 0;
    s = 0;
    ps = 0;
    pps = 0;
    i = 0;
    pi = 0;
    ppi = 0;
    l = 0;
    pl = 0;
    ppl = 0;

    c   = c   + 1; assert_int(1, c,   "ptr arith 1a"); c   = c   + 2; assert_int(3,  c,   "ptr arith 1b"); c   = c   - 3; assert_int(0, c,   "ptr arith 1c"); c   = c   - 1;
    pc  = pc  + 1; assert_int(1, pc,  "ptr arith 2a"); pc  = pc  + 2; assert_int(3,  pc,  "ptr arith 2b"); pc  = pc  - 3; assert_int(0, pc,  "ptr arith 2c"); pc  = pc  - 1;
    ppc = ppc + 1; assert_int(8, ppc, "ptr arith 3a"); ppc = ppc + 2; assert_int(24, ppc, "ptr arith 3b"); ppc = ppc - 3; assert_int(0, ppc, "ptr arith 3c"); ppc = ppc - 1;
    s   = s   + 1; assert_int(1, s,   "ptr arith 4a"); s   = s   + 2; assert_int(3,  s,   "ptr arith 4b"); s   = s   - 3; assert_int(0, s,   "ptr arith 4c"); s   = s   - 1;
    ps  = ps  + 1; assert_int(4, ps,  "ptr arith 5a"); ps  = ps  + 2; assert_int(12, ps,  "ptr arith 5b"); ps  = ps  - 3; assert_int(0, ps,  "ptr arith 5c"); ps  = ps  - 1;
    pps = pps + 1; assert_int(8, pps, "ptr arith 6a"); pps = pps + 2; assert_int(24, pps, "ptr arith 6b"); pps = pps - 3; assert_int(0, pps, "ptr arith 6c"); pps = pps - 1;
    i   = i   + 1; assert_int(1, i,   "ptr arith 7a"); i   = i   + 2; assert_int(3,  i,   "ptr arith 7b"); i   = i   - 3; assert_int(0, i,   "ptr arith 7c"); i   = i   - 1;
    pi  = pi  + 1; assert_int(4, pi,  "ptr arith 8a"); pi  = pi  + 2; assert_int(12, pi,  "ptr arith 8b"); pi  = pi  - 3; assert_int(0, pi,  "ptr arith 8c"); pi  = pi  - 1;
    ppi = ppi + 1; assert_int(8, ppi, "ptr arith 9a"); ppi = ppi + 2; assert_int(24, ppi, "ptr arith 9b"); ppi = ppi - 3; assert_int(0, ppi, "ptr arith 9c"); ppi = ppi - 1;
    l   = l   + 1; assert_int(1, l,   "ptr arith aa"); l   = l   + 2; assert_int(3,  l,   "ptr arith ab"); l   = l   - 3; assert_int(0, l,   "ptr arith ac"); l   = l   - 1;
    pl  = pl  + 1; assert_int(4, pl,  "ptr arith ba"); pl  = pl  + 2; assert_int(12, pl,  "ptr arith bb"); pl  = pl  - 3; assert_int(0, pl,  "ptr arith bc"); pl  = pl  - 1;
    ppl = ppl + 1; assert_int(8, ppl, "ptr arith ca"); ppl = ppl + 2; assert_int(24, ppl, "ptr arith cb"); ppl = ppl - 3; assert_int(0, ppl, "ptr arith cc"); ppl = ppl - 1;
}


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

void test_pointer_int_subtraction() {
    char c, *c1;
    short s, *s1;
    int i, *i1;
    long l, *l1;

    c1 = s1 =  i1 = l1 = 8;
    c = s = i = l = 1;

    assert_long(7, c1 - c, "pc - c"); assert_long(7, c1 - s, "pc - s"); assert_long(7, c1 - i, "pc - i"); assert_long(7, c1 - l, "pc - l");
    assert_long(6, s1 - c, "ps - c"); assert_long(6, s1 - s, "ps - s"); assert_long(6, s1 - i, "ps - i"); assert_long(6, s1 - l, "ps - l");
    assert_long(4, i1 - c, "pi - c"); assert_long(4, i1 - s, "pi - s"); assert_long(4, i1 - i, "pi - i"); assert_long(4, i1 - l, "pi - l");
    assert_long(0, l1 - c, "pl - c"); assert_long(0, l1 - s, "pl - s"); assert_long(0, l1 - i, "pl - i"); assert_long(0, l1 - l, "pl - l");
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

void test_pointer_arithmetic_addition() {
    char c, *c1, *c2;
    short s, *s1, *s2;
    int i, *i1, *i2;
    long l, *l1, *l2;

    c1 = c2 = s1 = s2 = i1 = i2 = l1 = l2 = 0;
    c = s = i = l = 1;

    // pointer + constant
    assert_long(1, c1 + 1, "pc 1");
    assert_long(2, s1 + 1, "ps 1");
    assert_long(4, i1 + 1, "pi 1");
    assert_long(8, l1 + 1, "pl 1");

    // constant + pointer
    assert_long(1, 1 + c1, "1 pc");
    assert_long(2, 1 + s1, "1 ps");
    assert_long(4, 1 + i1, "1 pi");
    assert_long(8, 1 + l1, "1 pl");

    // pointer + integer type
    assert_long(1, c1 + c, "pc c"); assert_long(1, c1 + s, "pc s"); assert_long(1, c1 + i, "pc i"); assert_long(1, c1 + l, "pc l");
    assert_long(2, s1 + c, "ps c"); assert_long(2, s1 + s, "ps s"); assert_long(2, s1 + i, "ps i"); assert_long(2, s1 + l, "ps l");
    assert_long(4, i1 + c, "pi c"); assert_long(4, i1 + s, "pi s"); assert_long(4, i1 + i, "pi i"); assert_long(4, i1 + l, "pi l");
    assert_long(8, l1 + c, "pl c"); assert_long(8, l1 + s, "pl s"); assert_long(8, l1 + i, "pl i"); assert_long(8, l1 + l, "pl l");

    // integer type + pointer
    assert_long(1, c + c1 , "c pc"); assert_long(1, s + c1 , "s pc"); assert_long(1, i + c1 , "i pc"); assert_long(1, l + c1 , "l pc");
    assert_long(2, c + s1 , "c ps"); assert_long(2, s + s1 , "s ps"); assert_long(2, i + s1 , "i ps"); assert_long(2, l + s1 , "l ps");
    assert_long(4, c + i1 , "c pi"); assert_long(4, s + i1 , "s pi"); assert_long(4, i + i1 , "i pi"); assert_long(4, l + i1 , "l pi");
    assert_long(8, c + l1 , "c pl"); assert_long(8, s + l1 , "s pl"); assert_long(8, i + l1 , "i pl"); assert_long(8, l + l1 , "l pl");
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

    test_brutal_pointer_arithmetic();
    test_char_pointer_arithmetic();
    test_pointer_pointer_subtraction1();
    test_pointer_pointer_subtraction2();
    test_pointer_int_subtraction();
    test_dereferenced_pointer_inc_dec();
    test_pointer_with_non_constant_non_pointer_addition();
    test_pointer_arithmetic_addition();
    test_struct_additions();

    finalize();
}
