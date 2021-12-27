#include "test-shlib.h"
#include "../test-lib.h"

char  c = 1;
short s = 2;
int   i = 3;
long  l = 4;

unsigned char  uc = 5;
unsigned short us = 6;
unsigned int   ui = 7;
unsigned long  ul = 8;

float f = 1.1;
double d = 2.1;
long double ld = 3.1;

int *pi;

struct st st = { 1, 2 };

char ca[2] = {1, 2};
short sa[2] = {1, 2};
int ia[2] = {1, 2};
long la[2] = {1, 2};

int inc_c() { c++; }
int inc_s() { s++; }
int inc_i() { i++; }
int inc_l() { l++; }

int inc_uc() { uc++; }
int inc_us() { us++; }
int inc_ui() { ui++; }
int inc_ul() { ul++; }

int inc_f()  { f++; }
int inc_d()  { d++; }
int inc_ld() { ld++; }

int inc_pi() { pi++; }

int inc_st() { st.i++; st.j++; }
int inc_ia() { ia[0]++; ia[1]++; }

int return_i() { return i; }
int return_i_plus_one() { return i + 1; }

FuncReturningInt fri = &return_i;

// A struct of structs
struct nss {
    struct {int a; int b; } s1;
    struct {int c; int d; } s2;
} v1 = { 1, 2, 3, 4};


void test_address_of() {
    char  *pc = &c; c = 1; assert_int(1, *pc, "&c"); (*pc)++; assert_int(2, c, "&c");
    short *ps = &s; s = 1; assert_int(1, *ps, "&s"); (*ps)++; assert_int(2, s, "&s");
    int   *pi = &i; i = 1; assert_int(1, *pi, "&i"); (*pi)++; assert_int(2, i, "&i");
    long  *pl = &l; l = 1; assert_int(1, *pl, "&l"); (*pl)++; assert_int(2, l, "&l");

    unsigned char  *upc = &uc; uc = 1; assert_int(1, *upc, "&c"); (*upc)++; assert_int(2, uc, "&uc");
    unsigned short *ups = &us; us = 1; assert_int(1, *ups, "&s"); (*ups)++; assert_int(2, us, "&us");
    unsigned int   *upi = &ui; ui = 1; assert_int(1, *upi, "&i"); (*upi)++; assert_int(2, ui, "&ui");
    unsigned long  *upl = &ul; ul = 1; assert_int(1, *upl, "&l"); (*upl)++; assert_int(2, ul, "&ul");

    float  *pf = &f; f = 1.1; assert_float( 1.1, *pf, "&f"); (*pf)++; assert_float( 2.1, f, "&f");
    double *pd = &d; d = 1.1; assert_double(1.1, *pd, "&d"); (*pd)++; assert_double(2.1, d, "&d");
    long double *pld = &ld; ld = 1.1; assert_long_double(1.1, *pld, "&ld"); (*pld)++; assert_long_double(2.1, ld, "&ld");

    ca[0] = 10; ca[1] = 20; assert_int(10, ca[0], "&ca 1"); assert_int(20, ca[1], "&ca 2"); ca[1]++; assert_int(21, ca[1], "&ca 3");
    sa[0] = 11; sa[1] = 21; assert_int(11, sa[0], "&sa 1"); assert_int(21, sa[1], "&sa 2"); sa[1]++; assert_int(22, sa[1], "&sa 3");
    ia[0] = 12; ia[1] = 22; assert_int(12, ia[0], "&ia 1"); assert_int(22, ia[1], "&ia 2"); ia[1]++; assert_int(23, ia[1], "&ia 3");
    la[0] = 13; la[1] = 23; assert_int(13, la[0], "&la 1"); assert_int(23, la[1], "&la 2"); la[1]++; assert_int(24, la[1], "&la 3");

    fa[0] = 14.1; fa[1] = 24.1; assert_float( 14.1, fa[0], "&fa 1"); assert_float( 24.1, fa[1], "&fa 2"); fa[1]++; assert_float( 25.1, fa[1], "&fa 3");
    da[0] = 15.1; da[1] = 25.1; assert_double(15.1, da[0], "&da 1"); assert_double(25.1, da[1], "&da 2"); da[1]++; assert_double(26.1, da[1], "&da 3");

    lda[0] = 16.1; lda[1] = 26.1; assert_long_double(16.1, lda[0], "&lda 1"); assert_long_double(26.1, lda[1], "&lda 2"); lda[1]++; assert_long_double(27.1, lda[1], "&lda 3");

    fri = &return_i;
    i = 1;                    assert_int(1, fri(), "& of function 1");
    fri = &return_i_plus_one; assert_int(2, fri(), "& of function 2");
    fri = 0;

    fri = 1 ? return_i : return_i;
    assert_int(1, fri(), "& of function 3");

    // Nested structs
    assert_int(0,  (void *) &v1.s1   -  (void *) &v1,  "Anonymous s/s 3");
    assert_int(0,  (void *) &v1.s1.a -  (void *) &v1,  "Anonymous s/s 4");
    assert_int(4,  (void *) &v1.s1.b -  (void *) &v1,  "Anonymous s/s 5");
    assert_int(8,  (void *) &v1.s2   -  (void *) &v1,  "Anonymous s/s 6");
    assert_int(8,  (void *) &v1.s2.c -  (void *) &v1,  "Anonymous s/s 7");
    assert_int(12, (void *) &v1.s2.d -  (void *) &v1,  "Anonymous s/s 8");

}
