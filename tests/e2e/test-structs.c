#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

struct s1; // Test declaration without definition

struct s1 {
    int i;
};

struct s1 *pgs1;

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
// struct __attribute__ ((__packed__)) pss2 { int i; char c; int j; }; // TODO revive packed structs
// struct __attribute__ ((packed))     pss3 { int i; char c; int j; }; // TODO revive packed structs

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

struct cfs gcfs;

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

struct sld {
    short s;
    long double ld;
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
    // struct pss1 *s1;
    // struct pss2 *s2;
    // struct pss3 *s3;

    // assert_int(12, sizeof(struct pss1),                         "packed struct 1");
    // assert_int( 4, (long) &(s1->c) - (long) s1,                 "packed struct 2");
    // assert_int( 8, (long) &(s1->j) - (long) s1,                 "packed struct 3");
    // assert_int( 9, sizeof(struct pss2),                         "packed struct 4");
    // assert_int( 4, (long) &(s2->c) - (long) s2,                 "packed struct 5");
    // assert_int( 5, (long) &(s2->j) - (long) s2,                 "packed struct 6");
    // assert_int( 9, sizeof(struct pss3),                         "packed struct 7");
    // assert_int( 4, (long) &(s3->c) - (long) s3,                 "packed struct 8");
    // assert_int( 5, (long) &(s3->j) - (long) s3,                 "packed struct 9");
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
    pgs1 = ns2->s->s1;
    assert_int(1, pgs1->i, "nested double struct indirect 3");
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
    assert_int(        1,   ds->c,    "ds c");
    assert_int(        2,   ds->s,    "ds s");
    assert_int(        3,   ds->i,    "ds i");
    assert_long(       4,   ds->l,    "ds l");
    assert_float(      5.1, ds->f,    "ds f");
    assert_double(     6.1, ds->d,    "ds d");
    assert_long_double(7.1, ds->ld,   "ds ld");
    assert_int(        8,   ds->st.i, "ds st.i");
    assert_int(        9,   ds->st.j, "ds st.j");
    assert_int(        10,  ds->un.i, "ds un.i");
    assert_int(        10,  ds->un.j, "ds un.j");
}

int test_direct_structs() {
    gds.c = 1;
    gds.s = 2;
    gds.i = 3;
    gds.l = 4;
    gds.f = 5.1;
    gds.d = 6.1;
    gds.ld = 7.1;
    gds.st.i = 8;
    gds.st.j = 9;
    gds.un.i = 10;
    assert_ds(&gds);

    struct ds ds;
    ds.c = 1;
    ds.s = 2;
    ds.i = 3;
    ds.l = 4;
    ds.f = 5.1;
    ds.d = 6.1;
    ds.ld = 7.1;
    ds.st.i = 8;
    ds.st.j = 9;
    ds.un.i = 10;
    assert_ds(&ds);
}

int test_struct_long_double_temporary_bug() {
    struct sld sld;

    sld.s = 6;
    sld.ld = 1.1;

    assert_long_double(1.1, sld.ld, "Long double/temporary bug 1");
    assert_int(6, sld.s, "Long double/temporary bug 2");
}

struct gs { int i; };

int test_scoped_struct_tags() {
    struct gs s1;

    s1.i = 1;
    assert_int(1, s1.i, "Scoped struct tags gs");

    struct s { int i; int j; };

    struct s s2;

    s2.i = 2; s2.j = 3;
    assert_int(2, s2.i, "Scoped struct tags s2");
    assert_int(3, s2.j, "Scoped struct tags s2");

    {
        struct s { int i; int j; int k; } s2;
        s2.i = 4; s2.j = 5; s2.k = 6;
        assert_int(4, s2.i, "Scoped struct tags s3");
        assert_int(5, s2.j, "Scoped struct tags s3");
        assert_int(6, s2.k, "Scoped struct tags s3");
    }

    assert_int(2, s2.i, "Scoped struct tags s2");
    assert_int(3, s2.j, "Scoped struct tags s2");
}

int test_declaration_without_definition() {
    struct s1; // Test declaration without definition

    struct s1 {
        int i;
    };
}

int test_copy() {
    struct { char  i;         } s011, s012; s011.i = 1;                         s012 = s011; assert_int(0, memcmp(&s011, &s012, sizeof(s011)), "Struct copy 1");
    struct { short i;         } s021, s022; s021.i = 1;                         s022 = s021; assert_int(0, memcmp(&s021, &s022, sizeof(s021)), "Struct copy 2");
    struct { char  i, j, k;   } s031, s032; s031.i = 1; s031.j = 2; s031.k = 3; s032 = s031; assert_int(0, memcmp(&s031, &s032, sizeof(s031)), "Struct copy 3");
    struct { int   i;         } s041, s042; s041.i = 1;                         s042 = s041; assert_int(0, memcmp(&s041, &s042, sizeof(s041)), "Struct copy 4");
    struct { short i, j, k;   } s061, s062; s061.i = 1; s061.j = 2; s061.k = 3; s062 = s061; assert_int(0, memcmp(&s061, &s062, sizeof(s061)), "Struct copy 6");
    struct { long  i;         } s081, s082; s081.i = 1;                         s082 = s081; assert_int(0, memcmp(&s081, &s082, sizeof(s081)), "Struct copy 8");
    struct { int   i, j, k;   } s121, s122; s121.i = 1; s121.j = 2; s121.k = 3; s122 = s121; assert_int(0, memcmp(&s121, &s122, sizeof(s061)), "Struct copy 12");
    struct { long  i, j;      } s161, s162; s161.i = 1; s161.j = 2;             s162 = s161; assert_int(0, memcmp(&s161, &s162, sizeof(s161)), "Struct copy 16");
    struct { long  i, j, k;   } s241, s242; s241.i = 1; s241.j = 2; s241.k = 3; s242 = s241; assert_int(0, memcmp(&s241, &s242, sizeof(s241)), "Struct copy 24");

    // Size 15
    struct { char i, j, k, l, m, n, o, p, q, r, s, t, u, v, w; } s151, s152;
    s151.i = s151.k = s151.m = s151.o = s151.q = s151.s = s151.u = s151.w = 1;
    s151.j = s151.l = s151.n = s151.p = s151.r = s151.t = s151.v          = 2;
    s152 = s151;
    assert_int(0, memcmp(&s151, &s152, sizeof(s151)), "Struct copy 15");

    // Size 40, this tests the memcpy version
    struct { long l1, l2, l3, l4, l5; } s401, s402;
    s401.l1 = 1; s401.l2 = 2; s401.l3 = 3; s401.l4 = 4; s401.l5 = 5;
    s402 = s401;
    assert_int(0, memcmp(&s401, &s402, sizeof(s401)), "Struct copy 40");

    // Copying of nested struct members
    struct { int i; struct { int j, k; } s; } ns1, ns2;
    ns1.i = -1;
    ns1.s.j = 1;
    ns1.s.k = 2;

    ns2.i = -2;
    ns2.s = ns1.s;
    assert_int(0, memcmp(&ns1.s, &ns2.s, 8), "Struct member copy 1");
    assert_int(-1, ns1.i, "Struct member copy 2");
    assert_int(-2, ns2.i, "Struct member copy 3");

    // Copying of a struct member that is a struct
    struct ds ds1, ds2;
    ds1.st.i = 1; ds1.st.j = 2;
    ds2.st = ds1.st;
    assert_int(1, ds2.st.i, "Struct member copy that is a struct 1");
    assert_int(2, ds2.st.j, "Struct member copy that is a struct 2");

    // Dereferened member copy that is a pointer to struct
    struct st {int i; int j;};
    struct s { int i; struct st *st; } s;
    s.st = malloc(sizeof(struct st));
    s.st->i = 1; s.st->j = 2;
    struct st st = *s.st;
    assert_int(1, st.i, "Dereferened member copy that is a pointer to struct 1");
    assert_int(2, st.j, "Dereferened member copy that is a pointer to struct 2");

    // Test bug where the same vreg was allocated for a temporary in the struct
    // copy generated code.
    struct { int i, j, k; } *ps1, *ps2, s1, s2;
    s1.i = 1; s1.j = 2;
    ps2 = &s2;
    s2 = s1;
    assert_int(1, ps2->i, "Struct copy bad vreg bug 1");
    assert_int(2, ps2->j, "Struct copy bad vreg bug 2");
}

int test_pointers() {
    // Pointer to global struct
    struct cfs *pcfs1, *pcfs2, **ppcfs;

    gcfs.c = 1;
    gcfs.s = 2;

    pcfs1 = &gcfs;
    assert_int(1, pcfs1->c, "Pointer to global struct 1");
    assert_int(2, pcfs1->s, "Pointer to global struct 2");

    // Pointer to local struct
    struct cfs cfs;
    cfs.c = 3;
    cfs.s = 4;

    pcfs1 = &cfs;
    assert_int(3, pcfs1->c, "Pointer to local struct 1");
    assert_int(4, pcfs1->s, "Pointer to local struct 2");

    pcfs1->c = 5;
    pcfs1->s = 6;
    assert_int(5, cfs.c, "Pointer to local struct 3");
    assert_int(6, cfs.s, "Pointer to local struct 4");

    pcfs1 = malloc(sizeof(struct cfs));
    pcfs1->c = 7;
    pcfs1->s = 8;

    gcfs = *pcfs1;
    assert_int(7, gcfs.c, "Struct copy g = *p 1");
    assert_int(8, gcfs.s, "Struct copy g = *p 2");

    pcfs1->c = 9;
    pcfs1->s = 10;
    cfs = *pcfs1;
    assert_int(9,  cfs.c, "Struct copy l = *p 1");
    assert_int(10, cfs.s, "Struct copy l = *p 2");

    gcfs.c = 11;
    gcfs.s = 12;
    *pcfs1 = gcfs;
    assert_int(11, pcfs1->c, "Struct copy *p = g 1");
    assert_int(12, pcfs1->s, "Struct copy *p = g 2");

    cfs.c = 13;
    cfs.s = 14;
    *pcfs1 = cfs;
    assert_int(13, pcfs1->c, "Struct copy *p = g 1");
    assert_int(14, pcfs1->s, "Struct copy *p = g 2");

    pcfs2 = malloc(sizeof(struct cfs));
    *pcfs2 = *pcfs1;
    assert_int(13, pcfs2->c, "Struct copy *p = *p 1");
    assert_int(14, pcfs2->s, "Struct copy *p = *p 2");

    // Double dereference
    ppcfs = &pcfs1;

    (*ppcfs)->c = 15; assert_int(15, (*ppcfs)->c, "Struct ** dereference 1");
    (**ppcfs).c = 16; assert_int(16, (**ppcfs).c, "Struct ** dereference 2");
}

int test_arithmetic_with_local_struct_members() {
    // Some miscellaneous sanity tests

    int i = 1;
    struct s {int i; int j;} st1, *pst1;
    st1.i = 1; st1.j = 2;
    assert_int(4, i + st1.i + st1.j, "Local struct member arithmetic 1");

    pst1 = malloc(sizeof(struct s));
    pst1->i = 3; pst1->j = 4;
    assert_int(8, i + pst1->i + pst1->j, "Local struct member arithmetic 2");
    assert_int(11, i + st1.i + st1.j + pst1->i + pst1->j, "Local struct member arithmetic 3");

    st1.i++;
    assert_int(2, st1.i, "Local struct member compound assignment");
}

void test_bit_field_sizes() {
    // A single bit field
    assert_int(4,  sizeof(struct {int a:1;}),                                   "Bit fields size 1");
    assert_int(4,  sizeof(struct {int a:31;}),                                  "Bit fields size 2");
    assert_int(4,  sizeof(struct {int a:32;}),                                  "Bit fields size 3");
    assert_int(8,  sizeof(struct {int a:32, b:1;}),                             "Bit fields size 4");
    assert_int(8,  sizeof(struct {int a:32, b:32;}),                            "Bit fields size 5");
    assert_int(12, sizeof(struct {int a:32, b:32, c:1;}),                       "Bit fields size 6");
    assert_int(12, sizeof(struct {int a:32, b:32, c:32;}),                      "Bit fields size 7");
    assert_int(4,  sizeof(struct {int i:16, j:16;}),                            "Bit fields size 8");
    assert_int(12, sizeof(struct {int i:1, j, k:1;}),                           "Bit fields size 9");
    assert_int(12, sizeof(struct {int a:31, b:32, c:32;}),                      "Bit fields size 10");
    assert_int(12, sizeof(struct {int a:32, b:32, c:32;}),                      "Bit fields size 11");
    assert_int(16, sizeof(struct {int a:32, b:32, c:32, d:1;}),                 "Bit fields size 12");
    assert_int(16, sizeof(struct {int a:32, b:32, c:32, d:32;}),                "Bit fields size 13");
    assert_int(20, sizeof(struct {int a:32, b:32, c:32, d:32, e:1;}),           "Bit fields size 14");
    assert_int(8,  sizeof(struct {int a:31, b:32;}),                            "Bit fields size 15");
    assert_int(8,  sizeof(struct {int a:32, b:32;}),                            "Bit fields size 16");
    assert_int(8,  sizeof(struct {int a:31, b:31, c:1;}),                       "Bit fields size 17");
    assert_int(12, sizeof(struct {int a:31, b:31, c:2;}),                       "Bit fields size 18");
    assert_int(16, sizeof(struct {int a:31, b:2, c:31, d:2;}),                  "Bit fields size 19");
    assert_int(16, sizeof(struct {int a:31, b:2, c:31, d:2,e:30;}),             "Bit fields size 20");
    assert_int(20, sizeof(struct {int a:31, b:2, c:31, d:2,e:31;}),             "Bit fields size 21");
    assert_int(20, sizeof(struct {int a:31,  :2, c:31, d:2,e:31;}),             "Bit fields size 22"); // unnamed bit-field
    assert_int(4,  sizeof(struct {int a:3, b:5, c:16, d:4, e:4;}),              "Bit fields size 23"); // Some misc combinations
    assert_int(8,  sizeof(struct {int a:3, b:5, c:16, d:4, e:4, f:1;}),         "Bit fields size 24");
    assert_int(4,  sizeof(struct {int a:3, b:5, c:16, d:4, e:4;}),              "Bit fields size 25");
    assert_int(8,  sizeof(struct {int a:3, b:5, c:16, d:4, e:4, f:1;}),         "Bit fields size 26");
    assert_int(12, sizeof(struct {int a:3, b, c:1;}),                           "Bit fields size 27");
    assert_int(12, sizeof(struct {int a, b:1, c;}),                             "Bit fields size 28");
    assert_int(4,  sizeof(struct {char a; int b:1;}),                           "Bit fields size 29");
    assert_int(4,  sizeof(struct {char a; int b:24;}),                          "Bit fields size 30");
    assert_int(8,  sizeof(struct {char a; int b:25;}),                          "Bit fields size 31");
    assert_int(8,  sizeof(struct {int a:1, :0, b:1;}),                          "Bit fields size 32"); // Usage of :0
    assert_int(8,  sizeof(struct {int a,   :0, b:1;}),                          "Bit fields size 33");
    assert_int(8,  sizeof(struct {int a,   :0, b;}),                            "Bit fields size 34");
    assert_int(8,  sizeof(struct {int a:1, :0, b;}),                            "Bit fields size 35");
    assert_int(12, sizeof(struct {int a:1, :0, b:1, :0, c:32;}),                "Bit fields size 36");
    assert_int(12, sizeof(struct {int a:1; int :0, b:1, :0, c:32;}),            "Bit fields size 37");
    assert_int(12, sizeof(struct {int a:1; int :0; int b:1, :0, c:32;}),        "Bit fields size 38");
    assert_int(12, sizeof(struct {int a:1; int :0; int b:1, :0, :0, c:32;}),    "Bit fields size 39");
}

static void test_bit_field_loading() {
    struct { unsigned int i:5, j:5, k:5, l:5, m:5, :0, n:3, o:11; } s1;

    // 0101... pattern test with unsigned integers
    unsigned long *pl = (unsigned long *) &s1;
    *pl = 0x5a5a5a5aa5a5a5a5UL;
    assert_int(5,   s1.i, "5a5a5a5aa5a5a5a5 s.i");
    assert_int(13,  s1.j, "5a5a5a5aa5a5a5a5 s.j");
    assert_int(9,   s1.k, "5a5a5a5aa5a5a5a5 s.k");
    assert_int(11,  s1.l, "5a5a5a5aa5a5a5a5 s.l");
    assert_int(26,  s1.m, "5a5a5a5aa5a5a5a5 s.m");
    assert_int(2,   s1.n, "5a5a5a5aa5a5a5a5 s.n");
    assert_int(843, s1.o, "5a5a5a5aa5a5a5a5 s.o");

    // Unsigned integers
    struct { unsigned int i:3, j:4, k:5, l:6, m:7, :0, n:3, o:11, p:18; } s2;
    pl = (long *) &s2;

    // -1 1, -1, 1, -1, 1 pattern
    *pl = 0x7ff941fc1f8fUL;
    assert_int(7,    s2.i, "7ff941fc1f8f unsigned s.i");
    assert_int(1,    s2.j, "7ff941fc1f8f unsigned s.j");
    assert_int(31,   s2.k, "7ff941fc1f8f unsigned s.k");
    assert_int(1,    s2.l, "7ff941fc1f8f unsigned s.l");
    assert_int(127,  s2.m, "7ff941fc1f8f unsigned s.m");
    assert_int(1,    s2.n, "7ff941fc1f8f unsigned s.n");
    assert_int(2047, s2.o, "7ff941fc1f8f unsigned s.o");
    assert_int(1,    s2.p, "7ff941fc1f8f unsigned s.p");

    // 1, -1, 1, -1, 1, -1 pattern
    *pl = 0xffffc00f4007f0f9UL;
    assert_int(1,      s2.i, "ffffc00f4007f0f9 unsigned s.i");
    assert_int(15,     s2.j, "ffffc00f4007f0f9 unsigned s.j");
    assert_int(1,      s2.k, "ffffc00f4007f0f9 unsigned s.k");
    assert_int(63,     s2.l, "ffffc00f4007f0f9 unsigned s.l");
    assert_int(1,      s2.m, "ffffc00f4007f0f9 unsigned s.m");
    assert_int(7,      s2.n, "ffffc00f4007f0f9 unsigned s.n");
    assert_int(1,      s2.o, "ffffc00f4007f0f9 unsigned s.o");
    assert_int(262143, s2.p, "ffffc00f4007f0f9 unsigned s.p");

    // Signed integers
    struct { signed int i:3, j:4, k:5, l:6, m:7, :0, n:3, o:11, p:18; } s3;
    pl = (long *) &s3;

    // -1 1, -1, 1, -1, 1 pattern
    *pl = 0x7ff941fc1f8fUL;
    assert_int(-1,    s3.i, "7ff941fc1f8f signed s.i");
    assert_int(1,     s3.j, "7ff941fc1f8f signed s.j");
    assert_int(-1,    s3.k, "7ff941fc1f8f signed s.k");
    assert_int(1,     s3.l, "7ff941fc1f8f signed s.l");
    assert_int(-1,    s3.m, "7ff941fc1f8f signed s.m");
    assert_int(1,     s3.n, "7ff941fc1f8f signed s.n");
    assert_int(-1,    s3.o, "7ff941fc1f8f signed s.o");
    assert_int(1,     s3.p, "7ff941fc1f8f signed s.p");

    // 1, -1, 1, -1, 1, -1 pattern
    *pl = 0xffffc00f4007f0f9UL;
    assert_int(1,     s3.i, "ffffc00f4007f0f9 signed s.i");
    assert_int(-1,    s3.j, "ffffc00f4007f0f9 signed s.j");
    assert_int(1,     s3.k, "ffffc00f4007f0f9 signed s.k");
    assert_int(-1,    s3.l, "ffffc00f4007f0f9 signed s.l");
    assert_int(1,     s3.m, "ffffc00f4007f0f9 signed s.m");
    assert_int(-1,    s3.n, "ffffc00f4007f0f9 signed s.n");
    assert_int(1,     s3.o, "ffffc00f4007f0f9 signed s.o");
    assert_int(-1,    s3.p, "ffffc00f4007f0f9 signed s.p");

    // In a nested struct
    struct { int i; struct { int i:5, j:18; } s; } s4;
    pl = (long *) &s4;
    *pl = 0x7fffa200000001UL;
    assert_int(1,     s4.i,   "7fffa200000001 nested s.i");
    assert_int(2,     s4.s.i, "7fffa200000001 nested s.j");
    assert_int(-3,    s4.s.j, "7fffa200000001 nested s.k");
}

static void test_bit_field_saving() {
    // Signed integer
    struct { int i:3, j:4, k:5, l:6, m:7, :0, n:3, o:11, p:18; } s1;
    unsigned long *pl = (unsigned long *) &s1;

    *pl = 0;

    s1.i = 1;      assert_long(0x0000000000000001UL, *pl, "bit field saving si1.i");
    s1.j = 3;      assert_long(0x0000000000000019UL, *pl, "bit field saving si1.j");
    s1.k = 7;      assert_long(0x0000000000000399UL, *pl, "bit field saving si1.k");
    s1.l = 15;     assert_long(0x000000000000f399UL, *pl, "bit field saving si1.l");
    s1.m = 31;     assert_long(0x00000000007cf399UL, *pl, "bit field saving si1.m");
    s1.n = 1;      assert_long(0x00000001007cf399UL, *pl, "bit field saving si1.n");
    s1.o = 3;      assert_long(0x00000019007cf399UL, *pl, "bit field saving si1.o");
    s1.p = 7;      assert_long(0x0001c019007cf399UL, *pl, "bit field saving si1.p");
    s1.i = 0;      assert_long(0x0001c019007cf398UL, *pl, "bit field saving si1.i 2");
    s1.p = 123456; assert_long(0x78900019007cf398UL, *pl, "bit field saving si1.p 2");
    s1.p = -1;     assert_long(0xffffc019007cf398UL, *pl, "bit field saving si1.p 3");

    // Unsigned integer.
    // This test is a bit paranoid, since there the sign of the dst isn't relevant
    struct { unsigned int i:3, j:4, k:5, l:6, m:7, :0, n:3, o:11, p:18; } s2;
    pl = (unsigned long *) &s2;

    *pl = 0;
    s2.i = 1;      assert_long(0x0000000000000001UL, *pl, "bit field saving si1.i");
    s2.j = 3;      assert_long(0x0000000000000019UL, *pl, "bit field saving si1.j");
    s2.k = 7;      assert_long(0x0000000000000399UL, *pl, "bit field saving si1.k");
    s2.l = 15;     assert_long(0x000000000000f399UL, *pl, "bit field saving si1.l");
    s2.m = 31;     assert_long(0x00000000007cf399UL, *pl, "bit field saving si1.m");
    s2.n = 1;      assert_long(0x00000001007cf399UL, *pl, "bit field saving si1.n");
    s2.o = 3;      assert_long(0x00000019007cf399UL, *pl, "bit field saving si1.o");
    s2.p = 7;      assert_long(0x0001c019007cf399UL, *pl, "bit field saving si1.p");
    s2.i = 0;      assert_long(0x0001c019007cf398UL, *pl, "bit field saving si1.i 2");
    s2.p = 123456; assert_long(0x78900019007cf398UL, *pl, "bit field saving si1.p 2");
    s2.p = -1;     assert_long(0xffffc019007cf398UL, *pl, "bit field saving si1.p 3");

    // From non-ints
    *pl = 0;
    s2.i = 1.1; assert_long(0x0000000000000001UL, *pl, "bit field saving si1.i from double");
    s2.i = 1.1L; assert_long(0x0000000000000001UL, *pl, "bit field saving si1.i from long double");

    // From another bit field
    s2.k = s1.j; assert_long(0x0000000000000181UL, *pl, "bit field saving si1.i from long double");

    struct { unsigned int i:1, j:31, k:31, l:1; } s3;
    pl = (unsigned long *) &s3;
    *pl = 0;

    // Size 31
    s3.i = 1;   assert_long(0x0000000000000001UL, *pl, "31 sized bit field write 1");
    s3.j = -1;  assert_long(0x00000000ffffffffUL, *pl, "31 sized bit field write 2");
    s3.k =  1;  assert_long(0x00000001ffffffffUL, *pl, "31 sized bit field write 3");
    s3.l =  -1; assert_long(0x80000001ffffffffUL, *pl, "31 sized bit field write 4");

    // Size 32 + 16 + 16
    struct { unsigned int i:32, j:16, k:16; } s4;
    pl = (unsigned long *) &s4;
    *pl = 0;
    s4.i = -1;   assert_long(0x00000000ffffffffUL, *pl, "32 sized bit field write 1");
    s4.j = -1;   assert_long(0x0000ffffffffffffUL, *pl, "32 sized bit field write 2");
    s4.k = -1;   assert_long(0xffffffffffffffffUL, *pl, "32 sized bit field write 3");
    s4.i = 0;    assert_long(0xffffffff00000000UL, *pl, "32 sized bit field write 4");
    s4.k = 8192; assert_long(0x2000ffff00000000UL, *pl, "32 sized bit field write 5");
}

void test_array_member_array_lookup() {
    // This tests a bug where a pointer in a register with an offset would get lost

    struct {short s; char ca[1];} sa[2] = {1, 2, 3, 4};

    assert_int(0, (void *) &(sa[0]      ) - (void *) &sa, "Struct array member array lookup offset of sa[0]      ");
    assert_int(4, (void *) &(sa[1]      ) - (void *) &sa, "Struct array member array lookup offset of sa[1]      ");
    assert_int(0, (void *) &(sa[0].s    ) - (void *) &sa, "Struct array member array lookup offset of sa[0].s    ");
    assert_int(2, (void *) &(sa[0].ca   ) - (void *) &sa, "Struct array member array lookup offset of sa[0].ca   ");
    assert_int(2, (void *) &(sa[0].ca[0]) - (void *) &sa, "Struct array member array lookup offset of sa[0].ca[0]");
    assert_int(4, (void *) &(sa[1].s    ) - (void *) &sa, "Struct array member array lookup offset of sa[1].s    ");
    assert_int(6, (void *) &(sa[1].ca[0]) - (void *) &sa, "Struct array member array lookup offset of sa[1].ca[0]");

    assert_int(1, sa[0].s,     "Struct array member array lookup 1");
    assert_int(2, sa[0].ca[0], "Struct array member array lookup 2");
    assert_int(3, sa[1].s,     "Struct array member array lookup 3");
    assert_int(4, sa[1].ca[0], "Struct array member array lookup 4");
}

void test_anonymous_struct_flattening() {
    struct { struct { int i, j; }; int k; } s2 = {1, 2, 3};

    assert_int(1, s2.i, "Anonymous struct flattening s2 1");
    assert_int(2, s2.j, "Anonymous struct flattening s2 2");
    assert_int(3, s2.k, "Anonymous struct flattening s2 3");


    struct {
        int a;
        union {
            short b;
            unsigned char c[2];
            };
        int d;
    } foo;

    foo.b = 0xff00;
    assert_int(0x00, foo.c[0], "Anonymous struct flattening 1");
    assert_int(0xff, foo.c[1], "Anonymous struct flattening 2");

    foo.b = 0x00ff;
    assert_int(0xff, foo.c[0], "Anonymous struct flattening 3");
    assert_int(0x00, foo.c[1], "Anonymous struct flattening 4");
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
    // test_packed_struct(); // TODO revive packed structs
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
    test_struct_long_double_temporary_bug();
    test_scoped_struct_tags();
    test_declaration_without_definition();
    test_copy();
    test_pointers();
    test_arithmetic_with_local_struct_members();
    test_bit_field_sizes();
    test_bit_field_loading();
    test_bit_field_saving();
    test_array_member_array_lookup();
    test_anonymous_struct_flattening();

    finalize();
}
