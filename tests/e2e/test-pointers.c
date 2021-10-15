#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

int g;

char *gpc, gc;
short *gps, gs;
int *gpi, gi;
long *gpl, gl;

unsigned char *gupc, guc;
unsigned short *gups, gus;
unsigned int *gupi, gui;
unsigned long *gupl, gul;

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

void test_pointer_deref_all_types() {
    char *pc;
    short *ps;
    int *pi;
    long *pl;
    unsigned char *upc;
    unsigned short *ups;
    unsigned int *upi;
    unsigned long *upl;

    pc  = malloc(sizeof(char));  upc  = malloc(sizeof(unsigned char));
    ps  = malloc(sizeof(short)); ups  = malloc(sizeof(unsigned short));
    pi  = malloc(sizeof(int));   upi  = malloc(sizeof(unsigned int));
    pl  = malloc(sizeof(long));  upl  = malloc(sizeof(unsigned long));
    gpc = malloc(sizeof(char));  gupc = malloc(sizeof(unsigned char));
    gps = malloc(sizeof(short)); gups = malloc(sizeof(unsigned short));
    gpi = malloc(sizeof(int));   gupi = malloc(sizeof(unsigned int));
    gpl = malloc(sizeof(long));  gupl = malloc(sizeof(unsigned long));

    memset(pc,  -1, sizeof(char));  memset(upc,  -1, sizeof(unsigned char));
    memset(ps,  -1, sizeof(short)); memset(ups,  -1, sizeof(unsigned short));
    memset(pi,  -1, sizeof(int));   memset(upi,  -1, sizeof(unsigned int));
    memset(pl,  -1, sizeof(long));  memset(upl,  -1, sizeof(unsigned long));
    memset(gpc, -1, sizeof(char));  memset(gupc, -1, sizeof(unsigned char));
    memset(gps, -1, sizeof(short)); memset(gups, -1, sizeof(unsigned short));
    memset(gpi, -1, sizeof(int));   memset(gupi, -1, sizeof(unsigned int));
    memset(gpl, -1, sizeof(long));  memset(gupl, -1, sizeof(unsigned long));

    assert_long(-1, *pc,  "*pc");  assert_long(0xff,       *upc,  "*upc");
    assert_long(-1, *ps,  "*ps");  assert_long(0xffff,     *ups,  "*ups");
    assert_long(-1, *pi,  "*pi");  assert_long(0xffffffff, *upi,  "*upi");
    assert_long(-1, *pl,  "*pl");  assert_long(-1,         *upl,  "*upl");
    assert_long(-1, *pc,  "*pc");  assert_long(0xff,       *upc,  "*upc");
    assert_long(-1, *ps,  "*ps");  assert_long(0xffff,     *ups,  "*ups");
    assert_long(-1, *pi,  "*pi");  assert_long(0xffffffff, *upi,  "*upi");
    assert_long(-1, *pl,  "*pl");  assert_long(-1,         *upl,  "*upl");
    assert_long(-1, *gpc, "*gpc"); assert_long(0xff,       *gupc, "*gupc");
    assert_long(-1, *gps, "*gps"); assert_long(0xffff,     *gups, "*gups");
    assert_long(-1, *gpi, "*gpi"); assert_long(0xffffffff, *gupi, "*gupi");
    assert_long(-1, *gpl, "*gpl"); assert_long(-1,         *gupl, "*gupl");
    assert_long(-1, *gpc, "*gpc"); assert_long(0xff,       *gupc, "*gupc");
    assert_long(-1, *gps, "*gps"); assert_long(0xffff,     *gups, "*gups");
    assert_long(-1, *gpi, "*gpi"); assert_long(0xffffffff, *gupi, "*gupi");
    assert_long(-1, *gpl, "*gpl"); assert_long(-1,         *gupl, "*gupl");
}

void test_pointer_deref_assign_to_deref() {
    char *pc;
    short *ps;
    int *pi;
    long *pl;
    unsigned char *upc;
    unsigned short *ups;
    unsigned int *upi;
    unsigned long *upl;

    pc = malloc(sizeof(char));  upc = malloc(sizeof(char));
    ps = malloc(sizeof(short)); ups = malloc(sizeof(short));
    pi = malloc(sizeof(int));   upi = malloc(sizeof(int));
    pl = malloc(sizeof(long));  upl = malloc(sizeof(long));

    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pc  = 1; *pc  = *pc;  assert_long(*pc,  *pc, "*char = *char");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *ps  = 1; *pc  = *ps;  assert_long(*pc,  *ps, "*char = *short");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pi  = 1; *pc  = *pi;  assert_long(*pc,  *pi, "*char = *int");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pl  = 1; *pc  = *pl;  assert_long(*pc,  *pl, "*char = *long");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pc  = 1; *ps  = *pc;  assert_long(*ps,  *pc, "*short = *char");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *ps  = 1; *ps  = *ps;  assert_long(*ps,  *ps, "*short = *short");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pi  = 1; *ps  = *pi;  assert_long(*ps,  *pi, "*short = *int");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pl  = 1; *ps  = *pl;  assert_long(*ps,  *pl, "*short = *long");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pc  = 1; *pi  = *pc;  assert_long(*pi,  *pc, "*int = *char");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *ps  = 1; *pi  = *ps;  assert_long(*pi,  *ps, "*int = *short");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pi  = 1; *pi  = *pi;  assert_long(*pi,  *pi, "*int = *int");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pl  = 1; *pi  = *pl;  assert_long(*pi,  *pl, "*int = *long");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pc  = 1; *pl  = *pc;  assert_long(*pl,  *pc, "*long = *char");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *ps  = 1; *pl  = *ps;  assert_long(*pl,  *ps, "*long = *short");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pi  = 1; *pl  = *pi;  assert_long(*pl,  *pi, "*long = *int");
    *pc  = 0; *ps  = 0; *pi  = 0; *pl  = 0; *pl  = 1; *pl  = *pl;  assert_long(*pl,  *pl, "*long = *long");

    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upc = 1; *upc = *upc; assert_long(*upc, *upc, "*uchar = *uchar");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *ups = 1; *upc = *ups; assert_long(*upc, *ups, "*uchar = *ushort");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upi = 1; *upc = *upi; assert_long(*upc, *upi, "*uchar = *uint");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upl = 1; *upc = *upl; assert_long(*upc, *upl, "*uchar = *ulong");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upc = 1; *ups = *upc; assert_long(*ups, *upc, "*ushort = *uchar");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *ups = 1; *ups = *ups; assert_long(*ups, *ups, "*ushort = *ushort");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upi = 1; *ups = *upi; assert_long(*ups, *upi, "*ushort = *uint");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upl = 1; *ups = *upl; assert_long(*ups, *upl, "*ushort = *ulong");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upc = 1; *upi = *upc; assert_long(*upi, *upc, "*uint = *uchar");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *ups = 1; *upi = *ups; assert_long(*upi, *ups, "*uint = *ushort");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upi = 1; *upi = *upi; assert_long(*upi, *upi, "*uint = *uint");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upl = 1; *upi = *upl; assert_long(*upi, *upl, "*uint = *ulong");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upc = 1; *upl = *upc; assert_long(*upl, *upc, "*ulong = *uchar");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *ups = 1; *upl = *ups; assert_long(*upl, *ups, "*ulong = *ushort");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upi = 1; *upl = *upi; assert_long(*upl, *upi, "*ulong = *uint");
    *upc = 0; *ups = 0; *upi = 0; *upl = 0; *upl = 1; *upl = *upl; assert_long(*upl, *upl, "*ulong = *ulong");
}

void test_global_pointer_address_of() {
    gc  = 1; gpc  = &gc;  assert_int(1, *gpc,  "Global address of char");
    gs  = 2; gps  = &gs;  assert_int(2, *gps,  "Global address of short");
    gi  = 3; gpi  = &gi;  assert_int(3, *gpi,  "Global address of int");
    gl  = 4; gpl  = &gl;  assert_int(4, *gpl,  "Global address of long");
    guc = 5; gupc = &guc; assert_int(5, *gupc, "Global address of unsigned char");
    gus = 6; gups = &gus; assert_int(6, *gups, "Global address of unsigned short");
    gui = 7; gupi = &gui; assert_int(7, *gupi, "Global address of unsigned int");
    gul = 8; gupl = &gul; assert_int(8, *gupl, "Global address of unsigned long");
}

void test_deref_promotion() {
    int i, *pi;
    long l, *pl;

    pi = malloc(sizeof(int));
    *pi = 1;
    l = *pi;
    assert_int(1, l, "deref l -> i");
}

void test_write_constant_to_pointer() {
    unsigned long *pl = malloc(sizeof(long));
    *pl = 0x7fffffffffffffff;
    assert_long(0x7fffffffffffffff, *pl, "0x7fffffffffffffff assignment to pointer");
}

void test_scaled_short_pointer_indirects() {
    long i;
    unsigned long ui;
    short *ps;
    unsigned short *pus;

    ps = malloc(sizeof(short) * 2);
    pus = malloc(sizeof(unsigned short) * 2);
    memset(ps, -1, sizeof(short) * 2);
    memset(pus, -1, sizeof(unsigned short) * 2);

    i = ui = 1;
    assert_long(-1,     ps[i],   "short pointer indirect from i");
    assert_long(0xffff, pus[i],  "ushort pointer indirect from i");
    assert_long(-1,     ps[ui],  "short pointer indirect from ui");
    assert_long(0xffff, pus[ui], "ushort pointer indirect from ui");
}

void test_scaled_int_pointer_indirects() {
    long i;
    unsigned long ui;
    int *pi;
    unsigned int *pui;

    pi = malloc(sizeof(int) * 2);
    pui = malloc(sizeof(unsigned int) * 2);
    memset(pi, -1, sizeof(int) * 2);
    memset(pui, -1, sizeof(unsigned int) * 2);

    i = ui = 1;
    assert_long(-1,         pi[i],   "int pointer indirect from i");
    assert_long(0xffffffff, pui[i],  "uint pointer indirect from i");
    assert_long(-1,         pi[ui],  "int pointer indirect from ui");
    assert_long(0xffffffff, pui[ui], "uint pointer indirect from ui");
}

void test_scaled_long_pointer_indirects() {
    long i;
    unsigned long ui;
    long *pl;
    unsigned long *pul;

    pl = malloc(sizeof(long) * 2);
    pul = malloc(sizeof(unsigned long) * 2);
    memset(pl, -1, sizeof(long) * 2);
    memset(pul, -1, sizeof(unsigned long) * 2);

    i = ui = 1;
    assert_long(-1, pl[i],   "long pointer indirect from i");
    assert_long(-1, pul[i],  "ulong pointer indirect from i");
    assert_long(-1, pl[ui],  "long pointer indirect from ui");
    assert_long(-1, pul[ui], "ulong pointer indirect from ui");
}

void test_scaled_indirects() {
    // These tests don't look special at a glance, but the generated instructions
    // are complex.
    test_scaled_short_pointer_indirects();
    test_scaled_int_pointer_indirects();
    test_scaled_long_pointer_indirects();
}

int test_null_pointer() {
    int *pi;

    pi = 0; assert_int(1, pi == 0,          "*pi == 0");         assert_int(1, 0          == pi, "0 == pi");
    pi = 0; assert_int(1, pi == 0L,         "pi == 0L");         assert_int(1, 0L         == pi, "0L == pi");
    pi = 0; assert_int(1, pi == (void *) 0, "pi == (void *) 0"); assert_int(1, (void *) 0 == pi, "(void *) 0 == pi");
    pi = 0; assert_int(0, pi == (void *) 1, "pi == (void *) 1"); assert_int(0, (void *) 1 == pi, "(void *) 1 == pi");

    pi = 1; assert_int(0, pi == 0,          "*pi == 0");         assert_int(0, 0          == pi, "0 == pi");
    pi = 1; assert_int(0, pi == 0L,         "pi == 0L");         assert_int(0, 0L         == pi, "0L == pi");
    pi = 1; assert_int(0, pi == (void *) 0, "pi == (void *) 0"); assert_int(0, (void *) 0 == pi, "(void *) 0 == pi");
    pi = 1; assert_int(1, pi == (void *) 1, "pi == (void *) 1"); assert_int(1, (void *) 1 == pi, "(void *) 1 == pi");

    pi = 0; assert_int(0, pi != 0,          "*pi == 0");         assert_int(0, 0          != pi, "0 == pi");
    pi = 0; assert_int(0, pi != 0L,         "pi == 0L");         assert_int(0, 0L         != pi, "0L == pi");
    pi = 0; assert_int(0, pi != (void *) 0, "pi == (void *) 0"); assert_int(0, (void *) 0 != pi, "(void *) 0 == pi");
    pi = 0; assert_int(1, pi != (void *) 1, "pi == (void *) 1"); assert_int(1, (void *) 1 != pi, "(void *) 1 == pi");

    pi = 1; assert_int(1, pi != 0,          "*pi == 0");         assert_int(1, 0          != pi, "0 == pi");
    pi = 1; assert_int(1, pi != 0L,         "pi == 0L");         assert_int(1, 0L         != pi, "0L == pi");
    pi = 1; assert_int(1, pi != (void *) 0, "pi == (void *) 0"); assert_int(1, (void *) 0 != pi, "(void *) 0 == pi");
    pi = 1; assert_int(0, pi != (void *) 1, "pi == (void *) 1"); assert_int(0, (void *) 1 != pi, "(void *) 1 == pi");
}

void assert_pi(int expected, int *pi, char *message) {
    assert_int(expected, *pi, message);
}

void taofp_int(int pi1, int pi2, int pi3, int pi4, int pi5, int pi6, int pi7) {
    assert_int(1, pi1, "taofp_int pi1 == 1"); assert_pi(1, &pi1, "pi1 == 1 in call");
    assert_int(2, pi2, "taofp_int pi2 == 2"); assert_pi(2, &pi2, "pi2 == 2 in call");
    assert_int(3, pi3, "taofp_int pi3 == 3"); assert_pi(3, &pi3, "pi3 == 3 in call");
    assert_int(4, pi4, "taofp_int pi4 == 4"); assert_pi(4, &pi4, "pi4 == 4 in call");
    assert_int(5, pi5, "taofp_int pi5 == 5"); assert_pi(5, &pi5, "pi5 == 5 in call");
    assert_int(6, pi6, "taofp_int pi6 == 6"); assert_pi(6, &pi6, "pi6 == 6 in call");
    assert_int(7, pi7, "taofp_int pi7 == 7"); assert_pi(7, &pi7, "pi7 == 7 in call"); // & of a pushed param
}

void assert_pld(long double expected, long double *pld, char *message) {
    assert_int(expected, *pld, message);
}

void taofp_long_double(long double pld1, long double pld2, long double pld3, long double pld4, long double pld5, long double pld6, long double pld7) {
    assert_long_double(1.1, pld1, "taofp_int pld1 == 1"); assert_pld(1, &pld1, "pld1 == 1 in call");
    assert_long_double(2.1, pld2, "taofp_int pld2 == 2"); assert_pld(2, &pld2, "pld2 == 2 in call");
    assert_long_double(3.1, pld3, "taofp_int pld3 == 3"); assert_pld(3, &pld3, "pld3 == 3 in call");
    assert_long_double(4.1, pld4, "taofp_int pld4 == 4"); assert_pld(4, &pld4, "pld4 == 4 in call");
    assert_long_double(5.1, pld5, "taofp_int pld5 == 5"); assert_pld(5, &pld5, "pld5 == 5 in call");
    assert_long_double(6.1, pld6, "taofp_int pld6 == 6"); assert_pld(6, &pld6, "pld6 == 6 in call");
    assert_long_double(7.1, pld7, "taofp_int pld7 == 7"); assert_pld(7, &pld7, "pld7 == 7 in call"); // & of a pushed param
}

struct s { int i; };

void taofp_cocktail_in_registers(char c, short s, int i, long l, float f, double d, struct s *st) {
    // Test function parameters in registers that are forced in the stack due to &
    &c; &s; &i; &l; &f; &d; &s;

    assert_int(1, c, "&c c");
    assert_int(2, s, "&s s");
    assert_int(3, i, "&i i");
    assert_int(4, l, "&l l");
    assert_float(5.1, f, "&f f");
    assert_float(6.1, d, "&d d");
    assert_int(7, st->i, "&st st");
}

void taofp_cocktail_in_stack(
    int i1, int i2, int i3, int i4, int i5, int i6, // int regs
    float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8,  // sse regs
    char c, short s, int i, long l, float f, double d, struct s *st) {
    // Test function parameters in the stack that are kept in the stack due to &
    &c; &s; &i; &l; &f; &d; &s;

    assert_int(1, c, "&c c");
    assert_int(2, s, "&s s");
    assert_int(3, i, "&i i");
    assert_int(4, l, "&l l");
    assert_float(5.1, f, "&f f");
    assert_float(6.1, d, "&d d");
    assert_int(7, st->i, "&st st");
}

int test_address_of_function_parameters() {
    taofp_int(1, 2, 3, 4, 5, 6, 7);
    taofp_long_double(1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1);

    char c = 1;
    short s = 2;
    int i = 3;
    long l = 4;
    float f = 5.1;
    double d = 6.1;
    struct s *st = malloc(sizeof(struct s)); st->i = 7;

    taofp_cocktail_in_registers(c, s, i, l, f, d, st);
    taofp_cocktail_in_stack(
        0, 0, 0, 0, 0, 0, // int regs
        0, 0, 0, 0, 0, 0, 0, 0, // sse regs
        c, s, i, l, f, d, st);
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
    test_pointer_deref_all_types();
    test_pointer_deref_assign_to_deref();
    test_global_pointer_address_of();
    test_deref_promotion();
    test_write_constant_to_pointer();
    test_scaled_indirects();
    test_null_pointer();
    test_address_of_function_parameters();

    finalize();
}
