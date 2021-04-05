#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

int verbose;
int passes;
int failures;

int g;

void test_pointer_to_int1() {
    // Special case of pi being in a register
    int i;
    int *pi;

    g = 0;
    pi = &g;
    *pi = 1;
    assert_int(1, g, "pointer to int 1-1");
}

void test_pointer_to_int2() {
    // Special case of pi being on the stack, due to use of &pi
    // **ppi is only assigned to once

    int i;
    int *pi;
    int **ppi;

    g = 0;
    pi = &g;
    ppi = &pi;
    *pi = 1;
    assert_int(1, g, "pointer to int 2-1");
    assert_int(1, *pi, "pointer to int 2-2");
    assert_int(1, **ppi, "pointer to int 2-3");

    **ppi = 2;
    assert_int(2, g,     "pointer to int 2-4");
    assert_int(2, **ppi, "pointer to int 3-5");
}

void test_pointer_to_int3() {
    // Special case of pi being on the stack, due to use of &pi
    // **ppi is assigned to twice, so not tree merged.

    int i;
    int *pi;
    int **ppi;

    g = 0;
    pi = &g;
    ppi = &pi;
    *pi = 1;
    assert_int(1, g, "pointer to int 3-1");

    **ppi = 2;
    assert_int(2, g,     "pointer to int 3-2");
    assert_int(2, **ppi, "pointer to int 3-3");

    pi = &i;
    *pi = 3;
    assert_int(3, i, "pointer to int 3-4");
    **ppi = 4;
    assert_int(4, i, "pointer to int 3-5");
}

void test_pointer_to_int4() {
    // Assignment to j without reuse

    int i, j, *pi;

    i = 1;
    pi = &i;
    *pi = 2;
    j = *pi;
}

void test_pointer_to_int5() {
    // Sign extend to l

    int i, *pi;
    long l;

    i = 1;
    pi = &i;
    *pi = 2;
    l = *pi;
    assert_int(2, l, "pointer to int 4-1");
}

void test_pointer_to_int6() {
    // Assignment to j with reuse

    int i, j, *pi;

    i = 1;
    pi = &i;
    *pi = 2;
    j = *pi;

    assert_int(2, i,   "pointer to int 5-1");
    assert_int(2, *pi, "pointer to int 5-2");
    assert_int(2, j,   "pointer to int 5-3");
}

void test_pointer_to_char() {
    char *pc;

    pc = "foo";
    assert_int('f', *pc, "pointer to char 1"); *pc++;
    assert_int('o', *pc, "pointer to char 1"); *pc++;
    assert_int('o', *pc, "pointer to char 1"); *pc++;
}


void test_pointer_arithmetic() {
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

int *aopta() {
    return malloc(sizeof(int));
}

void test_assignment_of_pointer_to_array() {
    int **ppi;
    int j;

    ppi = malloc(sizeof(int *));
    j = 0;
    ppi[j] = aopta();

    *(ppi[j]) = 1;
    assert_int(1, **ppi, "Pointer with non-constant non-pointer addition 1");

    **(ppi + j) = 2;
    assert_int(2, **ppi, "Pointer with non-constant non-pointer addition 2");
}

void test_double_dereference1() {
    char **ppc;
    char *pc;

    ppc = malloc(sizeof(char *));
    *ppc = "foo";
    assert_string("foo", *ppc, "Stuff");
}

void test_double_dereference2() {
    char **ppc;
    char *pc;

    ppc = malloc(sizeof(char *));
    pc = "bar";
    *ppc = pc;
    assert_string("bar", *ppc, "Stuff");
    pc = pc;
}

void test_string_copy() {
    char *src;
    char *dst;
    char *osrc;
    char *odst;
    int i;

    src = "foo";
    osrc = src;
    dst = malloc(4);
    odst = dst;
    while (*dst++ = *src++); // The coolest c code
    assert_int(0, strcmp(osrc, odst), "string copy");
}

void test_bracket_lookup() {
    long *pi;
    long *opi;
    int i;
    pi = malloc(3 * sizeof(long));
    opi = pi;
    pi[0] = 1;
    pi[1] = 2;
    pi = pi + 3;
    pi[-1] = 3;

    assert_int(1, *opi,       "[] 1");
    assert_int(2, *(opi + 1), "[] 2");
    assert_int(3, *(opi + 2), "[] 3");

    pi = opi;
    i = 0; pi[i] = 2;
    i = 1; pi[i] = 3;
    assert_int(2, *pi,       "[] 4");
    assert_int(3, *(pi + 1), "[] 5");
}

void test_array_lookup_of_string_literal() {
    assert_string("foobar", &"foobar"[0], "array lookup of string literal 1");
    assert_string("bar",    &"foobar"[3], "array lookup of string literal 2");
}

void test_casting() {
    int *pi;
    int *pj;
    pi = 0;
    pj = pi + 1;
    assert_int(4, (long) pj - (long) pi, "casting 1");
    pj = (int *) (((char *) pi) + 1);
    assert_int(1, (long) pj - (long) pi, "casting 2");
}

void test_int_char_interbreeding() {
    int i;
    char *c;
    c = (char *) &i;
    i = 1 + 256 * 2 + 256 * 256 + 3 + 256 * 256 * 256 * 4;
    assert_int(4, *c, "int char interbreeding 1");
    assert_int(67174916, i, "int char interbreeding 2");
    *c = 5;
    assert_int(67174917, i, "int char interbreeding 2");
}

// Test precedence error combined with expression parser not stopping at unknown tokens
void test_cast_in_function_call() {
    char *pi;
    pi = "foo";
    assert_int(0, strcmp((char *) pi, "foo"), "cast in function call");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_pointer_to_int1();
    test_pointer_to_int2();
    test_pointer_to_int3();
    test_pointer_to_int4();
    test_pointer_to_int5();
    test_pointer_to_int6();
    test_pointer_to_char();
    test_pointer_arithmetic();
    test_assignment_of_pointer_to_array();
    test_double_dereference1();
    test_double_dereference2();
    test_string_copy();
    test_bracket_lookup();
    test_array_lookup_of_string_literal();
    test_casting();
    test_int_char_interbreeding();
    test_cast_in_function_call();

    finalize();
}
