#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

int verbose;
int passes;
int failures;

int g;

char *gpc, gc;
short *gps, gs;
int *gpi, gi;
long *gpl, gl;

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

void test_pointer_casting_reads() {
    int i;
    char *data;

    data = malloc(8);

    memset(data, -1, 8); *((char  *) data) = 1; assert_long(0xffffffffffffff01, *((long *) data), "char assignment");
    memset(data, -1, 8); *((short *) data) = 1; assert_long(0xffffffffffff0001, *((long *) data), "short assignment");
    memset(data, -1, 8); *((int   *) data) = 1; assert_long(0xffffffff00000001, *((long *) data), "int assignment");
    memset(data, -1, 8); *((long  *) data) = 1; assert_long(0x0000000000000001, *((long *) data), "long assignment");

    memset(data, 1, 8);
    assert_long(0x0000000000000001, *((char  *) data), "char read 2");
    assert_long(0x0000000000000101, *((short *) data), "short read 2");
    assert_long(0x0000000001010101, *((int   *) data), "int read 2");
    assert_long(0x0101010101010101, *((long  *) data), "long read 2");
}

void test_pointer_deref_assign_to_deref() {
    char *pc;
    short *ps;
    int *pi;
    long *pl;

    pc = malloc(sizeof(char));
    ps = malloc(sizeof(short));
    pi = malloc(sizeof(int));
    pl = malloc(sizeof(long));

    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *ps; assert_long(*pc, *pc, "*char = *char");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *ps; assert_long(*pc, *pi, "*char = *short");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *pi; assert_long(*pc, *pi, "*char = *int");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *pi; assert_long(*pc, *pl, "*char = *long");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *ps; assert_long(*ps, *pc, "*short = *char");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *ps; assert_long(*ps, *pi, "*short = *short");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *pi; assert_long(*ps, *pi, "*short = *int");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *pi; assert_long(*ps, *pl, "*short = *long");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *ps; assert_long(*pi, *pc, "*int = *char");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *ps; assert_long(*pi, *pi, "*int = *short");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *pi; assert_long(*pi, *pi, "*int = *int");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *pi; assert_long(*pi, *pl, "*int = *long");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *ps; assert_long(*pl, *pc, "*long = *char");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *ps; assert_long(*pl, *pi, "*long = *short");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *pi; assert_long(*pl, *pi, "*long = *int");
    *pc = 0; *ps = 0; *pi = 0; *pl = 0;  *pl = *pi; assert_long(*pl, *pl, "*long = *long");
}

void test_global_pointer_address_of() {
    gc = 1; gpc = &gc; assert_int(1, *gpc, "Global address of char");
    gs = 2; gps = &gs; assert_int(2, *gps, "Global address of short");
    gi = 3; gpi = &gi; assert_int(3, *gpi, "Global address of int");
    gl = 4; gpl = &gl; assert_int(4, *gpl, "Global address of long");
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
    test_assignment_of_pointer_to_array();
    test_double_dereference1();
    test_double_dereference2();
    test_string_copy();
    test_bracket_lookup();
    test_array_lookup_of_string_literal();
    test_casting();
    test_int_char_interbreeding();
    test_cast_in_function_call();
    test_pointer_casting_reads();
    test_pointer_deref_assign_to_deref();
    test_global_pointer_address_of();

    finalize();
}
