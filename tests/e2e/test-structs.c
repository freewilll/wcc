#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

struct s1 {
    int i;
};

struct s2 {
    int    i1;
    long   l1;
    int    i2;
    short  s1;
};

struct ns1 {
    int i, j;
    struct s1 *s1;
};

struct s1 *gs1;

struct ns2 {
    struct ns1 *s;
};

enum {A, B};
enum {C=2, D};
enum {E=-2, F};

struct sc { char  i, j; char  k; };
struct ss { short i, j; short k; };
struct si { int   i, j; int   k; };
struct sl { long  i, j; long  k; };
struct sc* gsc;
struct ss* gss;
struct si* gsi;
struct sl* gsl;

struct sc1  { char        c1;                 };
struct sc2  { char        c1; char c2;        };
struct ss1  { short       c1;                 };
struct ss2  { short       c1; short s1;       };
struct si1  { int         c1;                 };
struct si2  { int         c1; int i1;         };
struct sl1  { long        c1;                 };
struct sl2  { long        c1; long l1;        };
struct sf1  { float       c1;                 };
struct sf2  { float       c1; float f1;       };
struct sd1  { double      c1;                 };
struct sd2  { double      c1; double d1;      };
struct sld1 { long double c1;                 };
struct sld2 { long double c1; long double l1; };

struct cc  { char c1; char         c2; };
struct cs  { char c1; short        s1; };
struct ci  { char c1; int          i1; };
struct cl  { char c1; long         l1; };
struct cf  { char c1; float        f1; };
struct cd  { char c1; double       d1; };
struct cld { char c1; long double ld1; };

struct ccc  { char c1; char         c2; char c3; };
struct csc  { char c1; short        c2; char c3; };
struct cic  { char c1; int          c2; char c3; };
struct clc  { char c1; long         c2; char c3; };
struct cfc  { char c1; float        c2; char c3; };
struct cdc  { char c1; double       c2; char c3; };
struct cldc { char c1; long double  c2; char c3; };

struct                              pss1 { int i; char c; int j; };
struct __attribute__ ((__packed__)) pss2 { int i; char c; int j; };
struct __attribute__ ((packed))     pss3 { int i; char c; int j; };

struct is1 {
    struct is2* s1;
    struct is2* s2;
};

struct is2 {
    int i;
};

struct smals {
    char **s;
};

struct sddp1 {
    int i;
};

struct sddp2 {
    struct sddp3 *s3;
};

struct sddp3 {
    int i;
    int end;
};

struct sddp4{
    int i;
    int j;
};

struct sddp5 {
    struct sddp4 *sddp4;
};

struct frps {
    int i, j;
};

struct fpsas {
    int i, j;
};

struct sas {
    long i, j;
};

struct scs {
    int i;
};

struct cfs {
    char           c;
    short          s;
    int            i;
    long           l;
    unsigned char  uc;
    unsigned short us;
    unsigned int   ui;
    unsigned long  ul;
};

struct opi {
    char c, *pc;
    short s, *ps;
    int i, *pi;
    long l, *pl;

    unsigned char uc, *puc;
    unsigned short us, *pus;
    unsigned int ui ,*pui;
    unsigned long ul, *pul;
};

void test_simple_struct() {
    struct sc *lsc1, *lsc2;
    struct ss *lss1, *lss2;
    struct si *lsi1, *lsi2;
    struct sl *lsl1, *lsl2;

    lsc1 = malloc(sizeof(struct sc)); lsc2 = malloc(sizeof(struct sc)); gsc  = malloc(sizeof(struct sc));
    lss1 = malloc(sizeof(struct ss)); lss2 = malloc(sizeof(struct ss)); gss  = malloc(sizeof(struct ss));
    lsi1 = malloc(sizeof(struct si)); lsi2 = malloc(sizeof(struct si)); gsi  = malloc(sizeof(struct si));
    lsl1 = malloc(sizeof(struct sl)); lsl2 = malloc(sizeof(struct sl)); gsl  = malloc(sizeof(struct sl));

    (*lsc1).i = 1; (*lsc1).j = 2; (*lsc2).i = 3; (*lsc2).j = 4;
    (*lss1).i = 1; (*lss1).j = 2; (*lss2).i = 3; (*lss2).j = 4;
    (*lsi1).i = 1; (*lsi1).j = 2; (*lsi2).i = 3; (*lsi2).j = 4;
    (*lsl1).i = 1; (*lsl1).j = 2; (*lsl2).i = 3; (*lsl2).j = 4;

    assert_int(1, (*lsc1).i, "SS c1"); assert_int(2, (*lsc1).j, "SS c1"); assert_int(3, (*lsc2).i, "SS c1"); assert_int(4, (*lsc2).j, "SS c1");
    assert_int(1, (*lss1).i, "SS s1"); assert_int(2, (*lss1).j, "SS s1"); assert_int(3, (*lss2).i, "SS s1"); assert_int(4, (*lss2).j, "SS s1");
    assert_int(1, (*lsi1).i, "SS i1"); assert_int(2, (*lsi1).j, "SS i1"); assert_int(3, (*lsi2).i, "SS i1"); assert_int(4, (*lsi2).j, "SS i1");
    assert_int(1, (*lsl1).i, "SS l1"); assert_int(2, (*lsl1).j, "SS l1"); assert_int(3, (*lsl2).i, "SS l1"); assert_int(4, (*lsl2).j, "SS l1");

    lsc1->i = -1; lsc1->j= -2;
    lss1->i = -1; lss1->j= -2;
    lsi1->i = -1; lsi1->j= -2;
    lsl1->i = -1; lsl1->j= -2;
    assert_int(-1, lsc1->i, "SS -> lc->i"); assert_int(-2, lsc1->j, "SS -> lc->j");
    assert_int(-1, lss1->i, "SS -> ls->i"); assert_int(-2, lss1->j, "SS -> ls->j");
    assert_int(-1, lsi1->i, "SS -> li->i"); assert_int(-2, lsi1->j, "SS -> li->j");
    assert_int(-1, lsl1->i, "SS -> ll->i"); assert_int(-2, lsl1->j, "SS -> ll->j");

    gsc->i = 10; gsc->j = 20;
    gss->i = 10; gss->j = 20;
    gsi->i = 10; gsi->j = 20;
    gsl->i = 10; gsl->j = 20;
    assert_int(10, gsc->i, "SS -> gc->i"); assert_int(20, gsc->j, "SS -> gc->j");
    assert_int(10, gss->i, "SS -> gs->i"); assert_int(20, gss->j, "SS -> gs->j");
    assert_int(10, gsi->i, "SS -> gi->i"); assert_int(20, gsi->j, "SS -> gi->j");
    assert_int(10, gsl->i, "SS -> gl->i"); assert_int(20, gsl->j, "SS -> gl->j");

    assert_int( 3, sizeof(struct sc), "sizeof char struct");
    assert_int( 6, sizeof(struct ss), "sizeof short struct");
    assert_int(12, sizeof(struct si), "sizeof int struct");
    assert_int(24, sizeof(struct sl), "sizeof long struct");
}

void test_struct_member_alignment() {
    struct cc  *vc;
    struct cs  *vs;
    struct ci  *vi;
    struct cl  *vl;
    struct cf  *vf;
    struct cd  *vd;
    struct cld *vld;

    assert_int( 1, sizeof(struct sc1),  "struct member alignment c1");
    assert_int( 2, sizeof(struct sc2),  "struct member alignment c2");
    assert_int( 2, sizeof(struct ss1),  "struct member alignment s1");
    assert_int( 4, sizeof(struct ss2),  "struct member alignment s2");
    assert_int( 4, sizeof(struct si1),  "struct member alignment i1");
    assert_int( 8, sizeof(struct si2),  "struct member alignment i2");
    assert_int( 8, sizeof(struct sl1),  "struct member alignment l1");
    assert_int(16, sizeof(struct sl2),  "struct member alignment l2");
    assert_int( 4, sizeof(struct sf1),  "struct member alignment f1");
    assert_int( 8, sizeof(struct sf2),  "struct member alignment f2");
    assert_int( 8, sizeof(struct sd1),  "struct member alignment d1");
    assert_int(16, sizeof(struct sd2),  "struct member alignment d2");
    assert_int(16, sizeof(struct sld1), "struct member alignment ld1");
    assert_int(32, sizeof(struct sld2), "struct member alignment ld2");

    assert_int( 2, sizeof(struct cc),  "struct member alignment c1");
    assert_int( 4, sizeof(struct cs),  "struct member alignment s1");
    assert_int( 8, sizeof(struct ci),  "struct member alignment i1");
    assert_int(16, sizeof(struct cl),  "struct member alignment l1");
    assert_int( 8, sizeof(struct cf),  "struct member alignment f1");
    assert_int(16, sizeof(struct cd),  "struct member alignment d1");
    assert_int(32, sizeof(struct cld), "struct member alignment ld1");

    assert_int( 3, sizeof(struct ccc),  "struct member alignment c2");
    assert_int( 6, sizeof(struct csc),  "struct member alignment s2");
    assert_int(12, sizeof(struct cic),  "struct member alignment i2");
    assert_int(24, sizeof(struct clc),  "struct member alignment l2");
    assert_int(12, sizeof(struct cfc ), "struct member alignment f2");
    assert_int(24, sizeof(struct cdc ), "struct member alignment d2");
    assert_int(48, sizeof(struct cldc), "struct member alignment ld2");

    vc  = 0;
    vs  = 0;
    vi  = 0;
    vl  = 0;
    vf  = 0;
    vd  = 0;
    vld = 0;

    assert_int(1,  (long) &(vc ->c2)  - (long) &(vc ->c1), "struct member alignment c");
    assert_int(2,  (long) &(vs ->s1)  - (long) &(vs ->c1), "struct member alignment s");
    assert_int(4,  (long) &(vi ->i1)  - (long) &(vi ->c1), "struct member alignment i");
    assert_int(8,  (long) &(vl ->l1)  - (long) &(vl ->c1), "struct member alignment l");
    assert_int(4,  (long) &(vf ->f1)  - (long) &(vf ->c1), "struct member alignment f");
    assert_int(8,  (long) &(vd ->d1)  - (long) &(vd ->c1), "struct member alignment d");
    assert_int(16, (long) &(vld->ld1) - (long) &(vld->c1), "struct member alignment ld");
}

void test_struct_indirect_sizes() {
    struct ccc *ccc;

    ccc = malloc(sizeof(struct ccc));

    ccc->c1 = 10;
    ccc->c2 = 20;
    ccc->c3 = 30;

    assert_int(10, ccc->c1, "Struct indirect sizes 1");
    assert_int(20, ccc->c2, "Struct indirect sizes 2");
    assert_int(30, ccc->c3, "Struct indirect sizes 3");
}

void test_struct_alignment_bug() {
    struct s2 *eh;

    assert_int( 8, (long) &(eh->l1) - (long) eh, "struct alignment bug 1");
    assert_int(16, (long) &(eh->i2) - (long) eh, "struct alignment bug 2");
    assert_int(20, (long) &(eh->s1) - (long) eh, "struct alignment bug 3");
    assert_int(24, sizeof(struct s2),            "struct alignment bug 4");
}

void test_struct_member_size_lookup_bug() {
    struct s2 *s;

    s = malloc(sizeof(struct s2) * 2);
    memset(s, -1, sizeof(struct s2) * 2);
    s[1].s1 = -1;
    s[1].i2 = 1;
    assert_long(1, s[1].i2, "struct member size lookup bug");
}

void test_nested_struct() {
    struct ns1 *s1;
    struct ns2 *s2;

    s1 = malloc(sizeof(struct ns1));
    s2 = malloc(sizeof(struct ns2));
    s2->s = s1;
    s1->i = 1;
    s1->j = 2;
    assert_int(1, s2->s->i, "nested struct 1");
    assert_int(2, s2->s->j, "nested struct 2");
    assert_int(16, sizeof(struct ns1), "nested struct 3");
    assert_int(8,  sizeof(struct ns2), "nested struct 4");

    s2->s->i = 3;
    s2->s->j = 4;
    assert_int(3, s1->i,    "nested struct 5");
    assert_int(4, s1->j,    "nested struct 6");
    assert_int(3, s2->s->i, "nested struct 7");
    assert_int(4, s2->s->j, "nested struct 8");
}

void test_packed_struct() {
    struct pss1 *s1;
    struct pss2 *s2;
    struct pss3 *s3;

    assert_int(12, sizeof(struct pss1),                         "packed struct 1");
    assert_int( 4, (long) &(s1->c) - (long) s1,                 "packed struct 2");
    assert_int( 8, (long) &(s1->j) - (long) s1,                 "packed struct 3");
    assert_int( 9, sizeof(struct pss2),                         "packed struct 4");
    assert_int( 4, (long) &(s2->c) - (long) s2,                 "packed struct 5");
    assert_int( 5, (long) &(s2->j) - (long) s2,                 "packed struct 6");
    assert_int( 9, sizeof(struct pss3),                         "packed struct 7");
    assert_int( 4, (long) &(s3->c) - (long) s3,                 "packed struct 8");
    assert_int( 5, (long) &(s3->j) - (long) s3,                 "packed struct 9");
}

void test_incomplete_struct() {
    struct is1 *s1;
    struct is2 *s21;
    struct is2 *s22;

    s1 = malloc(sizeof(struct is1));
    s21 = malloc(sizeof(struct is2));
    s22 = malloc(sizeof(struct is2));
    s1->s1 = s21;
    s1->s2 = s22;
    s1->s1->i = 1;
    s1->s2->i = 2;
    assert_int(1, s1->s1->i, "incomplete struct 1");
    assert_int(1, s21->i,    "incomplete struct 2");
    assert_int(2, s1->s2->i, "incomplete struct 3");
    assert_int(2, s22->i,    "incomplete struct 4");
    s21->i = 3;
    s22->i = 4;
    assert_int(3, s1->s1->i, "incomplete struct 5");
    assert_int(3, s21->i,    "incomplete struct 6");
    assert_int(4, s1->s2->i, "incomplete struct 7");
    assert_int(4, s22->i,    "incomplete struct 8");
}

void test_struct_member_array_lookup() {
    struct smals *s;

    s = malloc(sizeof(struct smals));
    s->s = malloc(sizeof(char *) * 2);
    s->s[0] = "foo";
    s->s[1] = "bar";

    assert_string("foo", *s->s,   "struct member array lookup 1");
    assert_string("foo", s->s[0], "struct member array lookup 2");
    assert_string("bar", s->s[1], "struct member array lookup 3");
}

void test_double_deref_precision1() {
    struct sddp1 *s1;
    struct sddp2 *s2;
    struct sddp3 *s3;

    s1 = malloc(2 * sizeof(struct sddp1 *));
    s1[0].i = 1;
    s1[1].i = 2;

    s2 = malloc(sizeof(struct sddp2 *));
    s3 = malloc(sizeof(struct sddp3 *));
    s2->s3 = s3;
    s3->end = -1;

    assert_long(0, s2->s3->i, "Double deref precision 1-1");
}

void test_double_deref_precision2() {
    struct sddp5 *sddp5;

    sddp5 = malloc(sizeof(struct sddp5));
    sddp5->sddp4 = malloc(sizeof(struct sddp4));
    memset(sddp5->sddp4, -1, sizeof(struct sddp4));
    sddp5->sddp4[0].i = 1;
    assert_long(1, sddp5->sddp4[0].i, "Double deref precision 2-1");
}

void test_struct_double_indirect_assign_to_global() {
    struct s1 *s1;
    struct ns1 *ns1;
    struct ns2 *ns2;

    s1 = malloc(sizeof(struct s1));
    ns1 = malloc(sizeof(struct ns1));
    ns2 = malloc(sizeof(struct ns2));
    ns2->s = ns1;
    ns1->s1 = s1;

    s1->i = 1;
    gs1 = ns2->s->s1;
    assert_int(1, gs1->i, "nested double struct indirect 3");
}

struct frps *frps() {
    struct frps *s;

    s = malloc(sizeof(struct frps));
    s->i = 1;
    s->j = 2;
    return s;
}

void test_function_returning_a_pointer_to_a_struct() {
    struct frps *s;
    s = frps();
    assert_int(1, s->i, "function returning a pointer to a struct 1");
    assert_int(2, s->j, "function returning a pointer to a struct 2");
}

int fpsa(struct fpsas *s) {
    return s->i + s->j;
}

void test_function_with_a_pointer_to_a_struct_argument() {
    struct fpsas *s;

    s = malloc(sizeof(struct fpsas));
    s->i = 1;
    s->j = 2;

    assert_int(3, fpsa(s), "function with a pointer to a struct argument");
}

void test_struct_casting() {
    int i;
    struct scs *s;

    i = 1;
    s = (struct scs*) &i;
    assert_int(1, s->i, "struct casting");
}

void test_chocolate_factory_struct() {
    struct cfs *cfs;

    cfs = malloc(sizeof(struct cfs));
    memset(cfs, -1, sizeof(struct cfs));

    assert_long(-1,         cfs->c,  "cfgs c");
    assert_long(-1,         cfs->s,  "cfgs s");
    assert_long(-1,         cfs->i,  "cfgs i");
    assert_long(-1,         cfs->l,  "cfgs l");
    assert_long(0xff,       cfs->uc, "ucfgs c");
    assert_long(0xffff,     cfs->us, "ucfgs s");
    assert_long(0xffffffff, cfs->ui, "ucfgs i");
    assert_long(-1,         cfs->ul, "ucfgs l");
}

void test_struct_offset_pointer_indirects() {
    // The generated code should have things like movw 16(%rbx), %ax

    char c, *pc;
    short s, *ps;
    int i, *pi;
    long l, *pl;

    unsigned char uc, *puc;
    unsigned short us, *pus;
    unsigned int ui ,*pui;
    unsigned long ul, *pul;

    struct opi *opi;

    opi = malloc(sizeof(struct opi));
    memset(opi, -1, sizeof(struct opi));

    c = opi->c;
    s = opi->s;
    i = opi->i;
    l = opi->l;

    uc = opi->uc;
    us = opi->us;
    ui = opi->ui;
    ul = opi->ul;

    pc = opi->pc;
    ps = opi->ps;
    pi = opi->pi;
    pl = opi->pl;

    puc = opi->puc;
    pus = opi->pus;
    pui = opi->pui;
    pul = opi->pul;

    assert_long(-1,         c,  "opi c");  assert_long(-1, pc,  "opi pc");
    assert_long(-1,         s,  "opi s");  assert_long(-1, ps,  "opi ps");
    assert_long(-1,         i,  "opi i");  assert_long(-1, pi,  "opi pi");
    assert_long(-1,         l,  "opi l");  assert_long(-1, pl,  "opi pl");
    assert_long(0xff,       uc, "opi uc"); assert_long(-1, puc, "opi puc");
    assert_long(0xffff,     us, "opi us"); assert_long(-1, pus, "opi pus");
    assert_long(0xffffffff, ui, "opi ui"); assert_long(-1, pui, "opi pui");
    assert_long(-1,         ul, "opi ul"); assert_long(-1, pul, "opi pul");

}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_simple_struct();
    test_struct_member_alignment();
    test_struct_indirect_sizes();
    test_struct_alignment_bug();
    test_struct_member_size_lookup_bug();
    test_nested_struct();
    test_packed_struct();
    test_incomplete_struct();
    test_struct_member_array_lookup();
    test_double_deref_precision1();
    test_double_deref_precision2();
    test_struct_double_indirect_assign_to_global();
    test_function_returning_a_pointer_to_a_struct();
    test_function_with_a_pointer_to_a_struct_argument();
    test_struct_casting();
    test_chocolate_factory_struct();
    test_struct_offset_pointer_indirects();

    finalize();
}
