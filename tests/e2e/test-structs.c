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

struct cc  { char c1; char        c2; };
struct cs  { char c1; short       s1; };
struct ci  { char c1; int         i1; };
struct cl  { char c1; long        l1; };
struct cf  { char c1; float       f1; };
struct cd  { char c1; double      d1; };
struct cld { char c1; long double ld1; };
struct cla { char c1; long        l1[10]; };

struct ccc  { char c1; char         c2;     char c3; };
struct csc  { char c1; short        c2;     char c3; };
struct cic  { char c1; int          c2;     char c3; };
struct clc  { char c1; long         c2;     char c3; };
struct cfc  { char c1; float        c2;     char c3; };
struct cdc  { char c1; double       c2;     char c3; };
struct cldc { char c1; long double  c2;     char c3; };
struct clac { char c1; long         c2[10]; char c3; };

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

struct nss1 {
    int i, j;
};

struct nss2 {
    char c1;
    struct nss1 nss1;
    char c2;
};

struct nss3 {
    long l;
    struct nss1 nss1;
};

struct nss4 {
    short s;
    struct nss1 nss1;
    struct nss2 nss2;
};

struct st {
    char c1;
    struct { int i; } s1; // A struct without a tag
    char c2;
};

union u {int a; int b; char c0, c1, c2, c3;};

struct ds {
    char c;
    short s;
    int i;
    long l;
    float f;
    double d;
    long double ld;
    struct { int i, j; } st;
    union { int i, j; } un;
};

struct ds gds;

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

void test_sizeof() {
    assert_int(1,  sizeof(struct {char x;}            ), "sizeof struct {char x;}");
    assert_int(2,  sizeof(struct {short x;}           ), "sizeof struct {short x;}");
    assert_int(4,  sizeof(struct {int x;}             ), "sizeof struct {int x;}");
    assert_int(8,  sizeof(struct {long x;}            ), "sizeof struct {long x;}");
    assert_int(16, sizeof(struct {long x[2];}         ), "sizeof struct {long x[2];}");
    assert_int(24, sizeof(struct {long x[2]; char c;} ), "sizeof struct {long x[2]; char c;}");
    assert_int(48, sizeof(struct {long x[2][3];}      ), "sizeof struct {long x[2][3];}");
    assert_int(16, sizeof(struct {long x; char y;}    ), "sizeof struct {long x; char y;}");
}

void test_struct_member_alignment() {
    struct cc  *vc;
    struct cs  *vs;
    struct ci  *vi;
    struct cl  *vl;
    struct cf  *vf;
    struct cd  *vd;
    struct cld *vld;
    struct cla *vla;

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
    assert_int(88, sizeof(struct cla), "struct member alignment l[10]1");

    assert_int( 3, sizeof(struct ccc),  "struct member alignment c2");
    assert_int( 6, sizeof(struct csc),  "struct member alignment s2");
    assert_int(12, sizeof(struct cic),  "struct member alignment i2");
    assert_int(24, sizeof(struct clc),  "struct member alignment l2");
    assert_int(12, sizeof(struct cfc ), "struct member alignment f2");
    assert_int(24, sizeof(struct cdc ), "struct member alignment d2");
    assert_int(48, sizeof(struct cldc), "struct member alignment ld2");
    assert_int(96, sizeof(struct clac), "struct member alignment l[10]2");

    vc  = 0;
    vs  = 0;
    vi  = 0;
    vl  = 0;
    vf  = 0;
    vd  = 0;
    vld = 0;
    vla = 0;

    assert_int(1,  (long) &(vc ->c2)  - (long) &(vc ->c1), "struct member alignment c");
    assert_int(2,  (long) &(vs ->s1)  - (long) &(vs ->c1), "struct member alignment s");
    assert_int(4,  (long) &(vi ->i1)  - (long) &(vi ->c1), "struct member alignment i");
    assert_int(8,  (long) &(vl ->l1)  - (long) &(vl ->c1), "struct member alignment l");
    assert_int(4,  (long) &(vf ->f1)  - (long) &(vf ->c1), "struct member alignment f");
    assert_int(8,  (long) &(vd ->d1)  - (long) &(vd ->c1), "struct member alignment d");
    assert_int(16, (long) &(vld->ld1) - (long) &(vld->c1), "struct member alignment ld");
    assert_int(8,  (long) &(vla->l1)  - (long) &(vla->c1), "struct member alignment la");
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

int test_sub_struct() {
    struct nss1 * nss1;
    struct nss2 * nss2;
    struct nss3 * nss3;
    struct nss4 * nss4;

    assert_int(16, sizeof(*nss2), "sizeof *nss2");
    assert_int(16, sizeof(*nss3), "sizeof *nss3");
    assert_int(28, sizeof(*nss4), "sizeof *nss4");

    assert_int(4,  (long) &nss1->j      - (long) &nss1->i,  "nested sub struct nss1 j");
    assert_int(4,  (long) &nss2->nss1   - (long) &nss2->c1, "nested sub struct nss1");
    assert_int(4,  (long) &nss2->nss1.i - (long) &nss2->c1, "nested sub struct nss2 i");
    assert_int(12, (long) &nss2->c2     - (long) &nss2->c1, "nested sub struct nss2 c2");
    assert_int(8,  (long) &nss2->nss1.j - (long) &nss2->c1, "nested sub struct nss2 j");
    assert_int(8,  (long) &nss3->nss1.i - (long) &nss3->l,  "nested sub struct nss3 i");
    assert_int(12, (long) &nss3->nss1.j - (long) &nss3->l,  "nested sub struct nss3 j");

    assert_int(4,  (long) &nss4->nss1.i      - (long) &nss4->s, "nested sub struct nss4 nss1.i");
    assert_int(8,  (long) &nss4->nss1.j      - (long) &nss4->s, "nested sub struct nss4 nss1.j");
    assert_int(12, (long) &nss4->nss2.c1     - (long) &nss4->s, "nested sub struct nss4 nss2.c1");
    assert_int(16, (long) &nss4->nss2.nss1   - (long) &nss4->s, "nested sub struct nss4 nss2.nss1");
    assert_int(16, (long) &nss4->nss2.nss1.i - (long) &nss4->s, "nested sub struct nss4 nss2.nss1.1");
    assert_int(20, (long) &nss4->nss2.nss1.j - (long) &nss4->s, "nested sub struct nss4 nss2.nss1.j");
    assert_int(24, (long) &nss4->nss2.c2     - (long) &nss4->s, "nested sub struct nss4 nss2.c2");

    struct st *st;
    assert_int(12, sizeof(struct st), "sizeof st");

    assert_int(4,  (long) &st->s1.i - (long) &st->c1, "nested sub struct st s1.i");
    assert_int(8,  (long) &st->c2   - (long) &st->c1, "nested sub struct st c2");
}

int test_unions() {
    assert_int(1,   sizeof(union {char c1; char c2;}), "sizeof union with char");
    assert_int(2,   sizeof(union {char c; short s;}), "sizeof union with short");
    assert_int(4,   sizeof(union {char c; int i;}), "sizeof union with int");
    assert_int(8,   sizeof(union {char c; long l;}), "sizeof union with long");
    assert_int(16,  sizeof(union {char c; long double ld;}), "sizeof union with long double");
    assert_int(32,  sizeof(union {char c; struct s {long double ld1; long double ld2;} s;}), "sizeof union with struct with 2x long double");

    union u *v = malloc(sizeof(union u));

    assert_int(4, sizeof(union u), "Simple union 1");
    assert_int(1, (long) &v->a == (long) &v->b && (long) &v->a == (long) &v->c0, "Simple union 2");

    v->a = -1;
    assert_int(-1, v->a,  "Simple union 3");
    assert_int(-1, v->b,  "Simple union 4");
    assert_int(-1, v->c0, "Simple union 5");
    assert_int(-1, v->c1, "Simple union 6");
    assert_int(-1, v->c2, "Simple union 7");
    assert_int(-1, v->c3, "Simple union 8");

    int va = v->a;
    assert_int(-1, va, "Simple union 9");
}

// A struct of structs
struct nss {
    struct {int a; int b; } s1;
    struct {int c; int d; } s2;
} v1;

// A union of structs
union nus {
    struct {int a; int b; } s1;
    struct {int c; int d; } s2;
} v2;

// A struct of unions
struct nsu {
    union {int a; int b; } u1;
    union {int c; int d; } u2;
} v3;

// A union of unions
union nuu {
    union {int a; int b; } u1;
    union {int c; int d; } u2;
} v4;

int test_nested_structs_and_unions() {
    assert_int(16, sizeof(struct nss),           "Anonymous s/s 1");
    assert_int(16, sizeof(v1),                   "Anonymous s/s 2");
    assert_int(0,  (int) &v1.s1   -  (int) &v1,  "Anonymous s/s 3");
    assert_int(0,  (int) &v1.s1.a -  (int) &v1,  "Anonymous s/s 4");
    assert_int(4,  (int) &v1.s1.b -  (int) &v1,  "Anonymous s/s 5");
    assert_int(8,  (int) &v1.s2   -  (int) &v1,  "Anonymous s/s 6");
    assert_int(8,  (int) &v1.s2.c -  (int) &v1,  "Anonymous s/s 7");
    assert_int(12, (int) &v1.s2.d -  (int) &v1,  "Anonymous s/s 8");

    assert_int(8, sizeof(union nus),             "Anonymous u/s 1");
    assert_int(8, sizeof(v2),                    "Anonymous u/s 2");
    assert_int(0, (int) &v2.s1   -   (int) &v2,  "Anonymous u/s 3");
    assert_int(0, (int) &v2.s1.a -   (int) &v2,  "Anonymous u/s 4");
    assert_int(4, (int) &v2.s1.b -   (int) &v2,  "Anonymous u/s 5");
    assert_int(0, (int) &v2.s2   -   (int) &v2,  "Anonymous u/s 6");
    assert_int(0, (int) &v2.s2.c -   (int) &v2,  "Anonymous u/s 7");
    assert_int(4, (int) &v2.s2.d -   (int) &v2,  "Anonymous u/s 8");

    assert_int(8, sizeof(struct nsu),            "Anonymous s/u 1");
    assert_int(8, sizeof(v3),                    "Anonymous s/u 2");
    assert_int(0, (int) &v3.u1   -   (int) &v3,  "Anonymous s/u 3");
    assert_int(0, (int) &v3.u1.a -   (int) &v3,  "Anonymous s/u 4");
    assert_int(0, (int) &v3.u1.b -   (int) &v3,  "Anonymous s/u 5");
    assert_int(4, (int) &v3.u2   -   (int) &v3,  "Anonymous s/u 6");
    assert_int(4, (int) &v3.u2.c -   (int) &v3,  "Anonymous s/u 7");
    assert_int(4, (int) &v3.u2.d -   (int) &v3,  "Anonymous s/u 8");

    assert_int(4, sizeof(union nuu),             "Anonymous u/u 1");
    assert_int(4, sizeof(v4),                    "Anonymous u/u 2");
    assert_int(0, (int) &v4.u1   -   (int) &v4,  "Anonymous u/u 3");
    assert_int(0, (int) &v4.u1.a -   (int) &v4,  "Anonymous u/u 4");
    assert_int(0, (int) &v4.u1.b -   (int) &v4,  "Anonymous u/u 5");
    assert_int(0, (int) &v4.u2   -   (int) &v4,  "Anonymous u/u 6");
    assert_int(0, (int) &v4.u2.c -   (int) &v4,  "Anonymous u/u 7");
    assert_int(0, (int) &v4.u2.d -   (int) &v4,  "Anonymous u/u 8");
}

void assert_ds(struct ds *ds) {
    assert_int(   1,   ds->c,    "ds c");
    assert_int(   2,   ds->s,    "ds s");
    assert_int(   3,   ds->i,    "ds i");
    assert_long(  4,   ds->l,    "ds l");
    assert_float( 5.1, ds->f,    "ds f");
    assert_double(6.1, ds->d,    "ds d");
    // assert_long_double(7.1, ds->ld,    "ds ld"); // swip
    assert_int(   8,   ds->st.i, "ds st.i");
    assert_int(   9,   ds->st.j, "ds st.j");
    assert_int(   10,  ds->un.i, "ds un.i");
    assert_int(   10,  ds->un.j, "ds un.j");
}

int test_direct_structs() {
    gds.c = 1;
    gds.s = 2;
    gds.i = 3;
    gds.l = 4;
    gds.f = 5.1;
    gds.d = 6.1;
    // gds.ld = 7.1; // swip
    gds.st.i = 8;
    gds.st.j = 9;
    gds.un.i = 10;
    assert_ds(&gds);
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_simple_struct();
    test_sizeof();
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
    test_sub_struct();
    test_unions();
    test_nested_structs_and_unions();
    test_direct_structs();

    finalize();
}
