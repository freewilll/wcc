#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

static char gc = 1;
static char *gpc1 = &gc;
static char *gpc2 = &gc + 1;
static char *gpc3 = &gc + 2;

static short gs = 1;
static short *gps1 = &gs;
static short *gps2 = &gs + 1;
static short *gps3 = &gs + 2;

static int gi = 1;
static int *gpi1 = &gi;
static int *gpi2 = &gi + 1;
static int *gpi3 = &gi + 2;
static int *gpi4 = 1 + &gi;
static int *gpi5 = &gi - 1;

float gf = 1.1;
double gd = 1.2;
long double gld = 1.3;

static int gia[2];
static int *gpia1 = &gia;
static int *gpia2 = &gia + 1;
static int *gpia3 = &gia + 2;

static long gll = 0xffffffffffffffff;
static long glla[2];

int gia2[3] = {1, 2};
int gia3[] = {1, 2, 3};

static long *gplla1 = &glla;
static long *gplla2 = &glla + 1;
static long *gplla3 = &glla + 2;

static void *gpv1 = (void *) 0;
static void *gpv2 = 0;

// Strings
static char *string1 = "string1";
static char *string2 = "string2" + 1;
static char *string3 = "string3" + 2;

static char ca1[] = "chararr1";
static char ca2[] = {"chararr2"};
static char ca3[2] = "foo";
static char ca4[3] = "foo";
static char ca5[4] = "foo";
static char ca5post = 'a';
static char ca6[] = "foo";
static char ca7[2] = "foo";

static wchar_t wc1[] = L"foo";
static wchar_t wc2[2] = L"foo";


char *apc[3] = {"foo", "bar", "bazzer"};
char *aapc[3][3] = {
    {"a",       "ab",       "abc"},
    {"abcd",    "abcde",    "abcdef"},
    {"abcdefg", "abcdefgh", "abcdefghi"},
};

// Test casting + pointer addition
static void *gpvc1  = ((char *)      0) + 1;
static void *gpvs1  = ((short *)     0) + 1;
static void *gpvi1  = ((int *)       0) + 1;
static void *gpvll1 = ((long *) 0) + 1;
static void *gpvc2  = ((char *)      0) - 1;
static void *gpvs2  = ((short *)     0) - 1;
static void *gpvi2  = ((int *)       0) - 1;
static void *gpvll2 = ((long *) 0) - 1;

char ca[10];
short sa[10];
int ia[10];
long la[10];
static void *gpvca  = &ca  + 1;
static void *gpvsa  = &sa  + 1;
static void *gpvia  = &ia  + 1;
static void *gpvlla = &la + 1;

static void *gpvca1 = &ca[1];
static void *gpvsa1 = &sa[1];
static void *gpvia1 = &ia[1];
static void *gpvla1 = &la[1];
static void *gpvca2 = &ca[1] + 1;
static void *gpvsa2 = &sa[1] + 1;
static void *gpvia2 = &ia[1] + 1;
static void *gpvla2 = &la[1] + 1;

static int csize  = sizeof(char);
static int ssize  = sizeof(short);
static int isize  = sizeof(int);
static int llsize = sizeof(long);

static char      ccast  = (char)      1;
static short     scast  = (short)     1;
static int       icast  = (int)       1;
static long llcast = (long) 1;

int ia25[2][5];
static void *gpvia25_00  = &ia25[0][0];
static void *gpvia25_01  = &ia25[0][1];
static void *gpvia25_10  = &ia25[1][0];
static void *gpvia25_11  = &ia25[1][1];
static void *gpvia25_11x = &ia25[1][1] + 1;

// enums
enum e {EA=100, EB};
static int enum_const1 = EA;
static int enum_const2 = EB;
static enum e enum1 = EA;
static enum e enum2 = EB;

// structs
struct s {int a; int b;} sv1;
int *spi1 = &sv1.a;
int *spi2 = &sv1.a + 1;
int *spi3 = (&(sv1) + 1);
int *spi4 = &sv1.b;
int *spi5 = &sv1.b + 1;

struct s sv2 = {1, 2};

static struct {char c1; short s1; int i1; char c2;} s2 = {1, 2, 3, 4};

struct s1 {short s; char ca[3];} v6 = {1, 2, 3, 4};
struct {short s; struct s1 v; int i;} s3[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

static struct {char c1; long ll; char c2;} s4;
struct {char c1; float f; char c2; double d; char c3; long double ld; char c4;} s5;

struct s2 {int i, j, k, l;} st2 = {1, 2, 3};

// Structs, pointers, sizeof and offsetof
struct s6 {
    long l1;
    struct s s;
    long l2;
};

struct s6 s6;
struct s6 *ps6 = 0;

// Offset of a struct
char ac1v[(void  *) &s6.s - (void  *) &s6];
char ac1c[(char  *) &s6.s - (char  *) &s6];
char ac1s[(short *) &s6.s - (short *) &s6];
char ac1i[(int   *) &s6.s - (int   *) &s6];
char ac1l[(long  *) &s6.s - (long  *) &s6];

// Offset of a pointer to a struct
char ac2[(void *) &ps6->s - (void *) ps6];

// Offset of with a constant pointer
char ac3[(((size_t) &((struct s6 *) 0)->s))];
char ac4[(((size_t) &((*(struct s6 *) 0)).s))];  // Using (*p). instead of p->

struct bfs { int i:3, j:4, k:5, :0, l:5, m:5; } bfs = {-1, -2, -3, -4, -5};

int plus(int i) {}
int (*global_func_ptr)(int) = plus;
int (*global_func_ptr2)(int) = &plus;

char ac[] = "Hello";
char *pc = ac;

void *pgi = (void *) &gi;

char *space = (char *) " ";

static void test_scalar_initializers() {
    int a = 1;          assert_int(1, a, "Scalar initializer 1 a");
    int b = {1};        assert_int(1, b, "Scalar initializer 1 b");
    int c = {{1}};      assert_int(1, c, "Scalar initializer 1 c");
    int d = {{{1}}};    assert_int(1, d, "Scalar initializer 1 d");
    int e = 1;          assert_int(1, e, "Scalar initializer 1 e");
    int f = {1,};       assert_int(1, f, "Scalar initializer 1 f");
    int g = {{1,}};     assert_int(1, g, "Scalar initializer 1 g");
    int h = {{{1,}}};   assert_int(1, h, "Scalar initializer 1 h");
    int i = {{{1,},},}; assert_int(1, h, "Scalar initializer 1 i");
}

static void test_array_init1() {
    char c1[2][2] = {1, 2, 3, 4};
    assert_int(1, c1[0][0], "Nested arrays c1[0][0]");
    assert_int(2, c1[0][1], "Nested arrays c1[0][1]");
    assert_int(3, c1[1][0], "Nested arrays c1[1][0]");
    assert_int(4, c1[1][1], "Nested arrays c1[1][1]");

    char c2[2][2] = {{1, 2}, 3, 4};
    assert_int(1, c2[0][0], "Nested arrays c2[0][0]");
    assert_int(2, c2[0][1], "Nested arrays c2[0][1]");
    assert_int(3, c2[1][0], "Nested arrays c2[1][0]");
    assert_int(4, c2[1][1], "Nested arrays c2[1][1]");

    char c3[2][2] = {{1, 2}, {3, 4}};
    assert_int(1, c3[0][0], "Nested arrays c3[0][0]");
    assert_int(2, c3[0][1], "Nested arrays c3[0][1]");
    assert_int(3, c3[1][0], "Nested arrays c3[1][0]");
    assert_int(4, c3[1][1], "Nested arrays c3[1][1]");

    char c4[2][2] = {1, {2}, 3, 4};
    assert_int(1, c4[0][0], "Nested arrays c4[0][0]");
    assert_int(2, c4[0][1], "Nested arrays c4[0][1]");
    assert_int(3, c4[1][0], "Nested arrays c4[1][0]");
    assert_int(4, c4[1][1], "Nested arrays c4[1][1]");

    char c5[2][2] = {1, {2, 3}, {4}};
    assert_int(1, c5[0][0], "Nested arrays c5[0][0]");
    assert_int(2, c5[0][1], "Nested arrays c5[0][1]");
    assert_int(4, c5[1][0], "Nested arrays c5[1][0]");
    assert_int(0, c5[1][1], "Nested arrays c5[1][1]");

    char c6[2][2] = {1, {2, 3}, 4};
    assert_int(1, c6[0][0], "Nested arrays c6[0][0]");
    assert_int(2, c6[0][1], "Nested arrays c6[0][1]");
    assert_int(4, c6[1][0], "Nested arrays c6[1][0]");
    assert_int(0, c6[1][1], "Nested arrays c6[1][1]");

    char c7[2][2] = {{1}, 2, 3, 4};
    assert_int(1, c7[0][0], "Nested arrays c7[0][0]");
    assert_int(0, c7[0][1], "Nested arrays c7[0][1]");
    assert_int(2, c7[1][0], "Nested arrays c7[1][0]");
    assert_int(3, c7[1][1], "Nested arrays c7[1][1]");

    char c8[2][2] = {{1, 2, 3}, 4};
    assert_int(1, c8[0][0], "Nested arrays c8[0][0]");
    assert_int(2, c8[0][1], "Nested arrays c8[0][1]");
    assert_int(4, c8[1][0], "Nested arrays c8[1][0]");
    assert_int(0, c8[1][1], "Nested arrays c8[1][1]");

    char c9[4] = {1};
    assert_int(1, c9[0], "Nested arrays c9[0]");
    assert_int(0, c9[1], "Nested arrays c9[1]");
    assert_int(0, c9[2], "Nested arrays c9[2]");
    assert_int(0, c9[3], "Nested arrays c9[3]");

    char c10[4] = {{1}};
    assert_int(1, c10[0], "Nested arrays c10[0]");
    assert_int(0, c10[1], "Nested arrays c10[1]");
    assert_int(0, c10[2], "Nested arrays c10[2]");
    assert_int(0, c10[3], "Nested arrays c10[3]");

    char c11[] = {{1, 2, 3, 4}};
    assert_int(1, sizeof(c11), "sizeof(c11)");
    assert_int(1, c11[0], "Nested arrays c11[0]");

    char c12[3] = {1, 2, 3};
    assert_int(1, c12[0], "Nested arrays c12");
    assert_int(2, c12[1], "Nested arrays c12");
    assert_int(3, c12[2], "Nested arrays c12");

    char c13[2][3] = {1, 2, 3, 4, 5, 6};
    assert_int(1, c13[0][0], "Nested arrays c13");
    assert_int(2, c13[0][1], "Nested arrays c13");
    assert_int(3, c13[0][2], "Nested arrays c13");
    assert_int(4, c13[1][0], "Nested arrays c13");
    assert_int(5, c13[1][1], "Nested arrays c13");
    assert_int(6, c13[1][2], "Nested arrays c13");

    char c14[2][3] = {{1, 2, 3}, {4, 5, 6}};
    assert_int(1, c14[0][0], "Nested arrays c14");
    assert_int(2, c14[0][1], "Nested arrays c14");
    assert_int(3, c14[0][2], "Nested arrays c14");
    assert_int(4, c14[1][0], "Nested arrays c14");
    assert_int(5, c14[1][1], "Nested arrays c14");
    assert_int(6, c14[1][2], "Nested arrays c14");

    char c15[3][3] = {{1, 2, 3}, {4, 5}, {6}};
    assert_int(1, c15[0][0], "Nested arrays c15[0][0]");
    assert_int(2, c15[0][1], "Nested arrays c15[0][1]");
    assert_int(3, c15[0][2], "Nested arrays c15[0][2]");
    assert_int(4, c15[1][0], "Nested arrays c15[1][0]");
    assert_int(5, c15[1][1], "Nested arrays c15[1][1]");
    assert_int(0, c15[1][2], "Nested arrays c15[1][2]");
    assert_int(6, c15[2][0], "Nested arrays c15[2][0]");
    assert_int(0, c15[2][1], "Nested arrays c15[2][1]");
    assert_int(0, c15[2][2], "Nested arrays c15[2][2]");

    int c16[2][2] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    assert_int(1, c16[0][0], "Nested arrays c16[0][0]");
    assert_int(2, c16[0][1], "Nested arrays c16[0][1]");
    assert_int(4, c16[1][0], "Nested arrays c16[1][0]");
    assert_int(5, c16[1][1], "Nested arrays c16[1][1]");

    int c17[2][2] = {{1, 2, 3}, {4, 5, 6}, 7};
    assert_int(1, c17[0][0], "Nested arrays c17[0][0]");
    assert_int(2, c17[0][1], "Nested arrays c17[0][1]");
    assert_int(4, c17[1][0], "Nested arrays c17[1][0]");
    assert_int(5, c17[1][1], "Nested arrays c17[1][1]");

    struct {int a[2], b;} c18[] = {{1}, 2};
    assert_int(24, sizeof(c18), "Nested arrays c18 size");
    assert_int(1, c18[0].a[0],  "Nested arrays c18[0][0]");
    assert_int(0, c18[0].a[1],  "Nested arrays c18[0][1]");
    assert_int(2, c18[1].a[0],  "Nested arrays c18[1][0]");
    assert_int(0, c18[1].a[1],  "Nested arrays c18[1][1]");

    int c19[] = {1, 2, 3};  assert_int(12, sizeof(c19), "Nested arrays c19 size");
}

static void test_array_init2() {
    int a[3] = {0, 1, 2, 3};
    assert_int(0, a[0], "init array 2 [0]");
    assert_int(1, a[1], "init array 2 [1]");
    assert_int(2, a[2], "init array 2 [2]");
}

static void test_array_init3() {
    int a[3][2] = {{0, 1}, {2, 3}, {4, 5}};
    assert_int(0, a[0][0], "init array 3 [0][0]");
    assert_int(1, a[0][1], "init array 3 [0][1]");
    assert_int(2, a[1][0], "init array 3 [1][0]");
    assert_int(3, a[1][1], "init array 3 [1][1]");
    assert_int(4, a[2][0], "init array 3 [2][0]");
    assert_int(5, a[2][1], "init array 3 [2][1]");
}

static void test_array_init4() {
    int i1 = {11};
    int i2 = {12, 13};
    int i3 = {{13, 14}, 15};
    assert_int(11, i1, "init array 4 1");
    assert_int(12, i2, "init array 4 2");
    assert_int(13, i3, "init array 4 3");
}

static void test_array_init5() {
    int a[3] = {1, 2, 3};
    int b[3] = {1, 2, 3, };
    int c[3] = {1, 2, {3}};
    int d[3] = {1, 2, {3, 4}};
    int e[3] = {1, 2, {3, 4}, 5, 6, 7, 8, 9};

    assert_int(11111, 10000 * a[0] + 1000 * b[0] + 100 * c[0] + 10 * d[0] + e[0], "init array 5 [0]");
    assert_int(22222, 10000 * a[1] + 1000 * b[1] + 100 * c[1] + 10 * d[1] + e[1], "init array 5 [0]");
    assert_int(33333, 10000 * a[2] + 1000 * b[2] + 100 * c[2] + 10 * d[2] + e[2], "init array 5 [2]");
}

static void test_array_init6() {
    int a[3] = {100};
    assert_int(100, a[0], "init array 6");
}

static void test_array_init7() {
    int a[2][2] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    assert_int(1, a[0][0], "init array 7 1 [0][0]");
    assert_int(2, a[0][1], "init array 7 1 [0][1]");
    assert_int(4, a[1][0], "init array 7 1 [1][0]");
    assert_int(5, a[1][1], "init array 7 1 [1][1]");

    int b[2][2] = {{1, 2, 3}, {4, 5, 6}, 7};
    assert_int(1, b[0][0], "init array 7 2 [0][0]");
    assert_int(2, b[0][1], "init array 7 2 [0][1]");
    assert_int(4, b[1][0], "init array 7 2 [1][0]");
    assert_int(5, b[1][1], "init array 7 2 [1][1]");
}

static void test_array_init8() {
    int a[3] = {1, 2, 3};
    assert_int(1, a[0], "init array 8 1 a[0]");
    assert_int(2, a[1], "init array 8 1 a[1]");
    assert_int(3, a[2], "init array 8 1 a[2]");

    int b[3][2] = {{1, 2}, {3, 4}, {5, 6}};
    assert_int(1, b[0][0], "init array 8 1 b[0][0]");
    assert_int(2, b[0][1], "init array 8 1 b[1][0]");
    assert_int(3, b[1][0], "init array 8 1 b[2][0]");
    assert_int(4, b[1][1], "init array 8 1 b[0][1]");
    assert_int(5, b[2][0], "init array 8 1 b[1][1]");
    assert_int(6, b[2][1], "init array 8 1 b[2][1]");

    int c[2][3] = {{1, 2, 3}, {4, 5, 6}};
    assert_int(1, c[0][0], "init array 8 3 c[0][0]");
    assert_int(2, c[0][1], "init array 8 3 c[1][0]");
    assert_int(3, c[0][2], "init array 8 3 c[2][0]");
    assert_int(4, c[1][0], "init array 8 3 c[0][1]");
    assert_int(5, c[1][1], "init array 8 3 c[1][1]");
    assert_int(6, c[1][2], "init array 8 3 c[2][1]");
}

static void test_array_init9() {
    // 6.7.8.26
    int a[4][3] = {
        {1, 3, 5},
        {2, 4, 6},
        {3, 5, 7},
    };
    int b[4][3] = {1, 3, 5, 2, 4, 6, 3, 5, 7};

    assert_int(a[0][0], b[0][0], "Array init 9 00");
    assert_int(a[0][1], b[0][1], "Array init 9 01");
    assert_int(a[0][2], b[0][2], "Array init 9 02");
    assert_int(a[1][0], b[1][0], "Array init 9 10");
    assert_int(a[1][1], b[1][1], "Array init 9 11");
    assert_int(a[1][2], b[1][2], "Array init 9 12");
    assert_int(a[2][0], b[2][0], "Array init 9 20");
    assert_int(a[2][1], b[2][1], "Array init 9 21");
    assert_int(a[2][2], b[2][2], "Array init 9 22");
}

static void test_array_init10() {
    int a[4] = {1,2};
    assert_int(1, a[0], "Array init 10 1 0");
    assert_int(2, a[1], "Array init 10 1 1");
    assert_int(0, a[2], "Array init 10 1 2");
    assert_int(0, a[3], "Array init 10 1 3");

    // 6.7.8.27
    int b[2][3] = {{1}, {2}};
    assert_int(1, b[0][0], "Array init 10 2 00");
    assert_int(0, b[0][1], "Array init 10 2 01");
    assert_int(0, b[0][2], "Array init 10 2 02");
    assert_int(2, b[1][0], "Array init 10 2 10");
    assert_int(0, b[1][1], "Array init 10 2 11");
    assert_int(0, b[1][2], "Array init 10 2 12");
}

static void test_array_init11() {
    // 6.7.8.28
    struct {int a[2], b;} v[] = {{1}, 2};
    assert_int(24, sizeof(v), "Array/struct init 11 size ");
    assert_int(1, v[0].a[0],  "Array/struct init 11 00");
    assert_int(0, v[0].a[1],  "Array/struct init 11 01");
    assert_int(2, v[1].a[0],  "Array/struct init 11 10");
    assert_int(0, v[1].a[1],  "Array/struct init 11 11");
}

static void check_array_init12(int (*a)[3][2], char *message) {
    // Very low tech assertion. It's deliberately not written in a clever
    // way to ensure the compiler is testing itself and checking its own
    // bugs.
    char *buf;
    wasprintf(&buf, "%s 000", message); assert_int(1, a[0][0][0], buf);
    wasprintf(&buf, "%s 001", message); assert_int(0, a[0][0][1], buf);
    wasprintf(&buf, "%s 010", message); assert_int(0, a[0][1][0], buf);
    wasprintf(&buf, "%s 011", message); assert_int(0, a[0][1][1], buf);
    wasprintf(&buf, "%s 020", message); assert_int(0, a[0][2][0], buf);
    wasprintf(&buf, "%s 021", message); assert_int(0, a[0][2][1], buf);
    wasprintf(&buf, "%s 100", message); assert_int(2, a[1][0][0], buf);
    wasprintf(&buf, "%s 101", message); assert_int(3, a[1][0][1], buf);
    wasprintf(&buf, "%s 110", message); assert_int(0, a[1][1][0], buf);
    wasprintf(&buf, "%s 111", message); assert_int(0, a[1][1][1], buf);
    wasprintf(&buf, "%s 120", message); assert_int(0, a[1][2][0], buf);
    wasprintf(&buf, "%s 121", message); assert_int(0, a[1][2][1], buf);
    wasprintf(&buf, "%s 200", message); assert_int(4, a[2][0][0], buf);
    wasprintf(&buf, "%s 201", message); assert_int(5, a[2][0][1], buf);
    wasprintf(&buf, "%s 210", message); assert_int(6, a[2][1][0], buf);
    wasprintf(&buf, "%s 211", message); assert_int(0, a[2][1][1], buf);
    wasprintf(&buf, "%s 220", message); assert_int(0, a[2][2][0], buf);
    wasprintf(&buf, "%s 221", message); assert_int(0, a[2][2][1], buf);
    wasprintf(&buf, "%s 300", message); assert_int(0, a[3][0][0], buf);
    wasprintf(&buf, "%s 301", message); assert_int(0, a[3][0][1], buf);
    wasprintf(&buf, "%s 310", message); assert_int(0, a[3][1][0], buf);
    wasprintf(&buf, "%s 311", message); assert_int(0, a[3][1][1], buf);
    wasprintf(&buf, "%s 320", message); assert_int(0, a[3][2][0], buf);
    wasprintf(&buf, "%s 321", message); assert_int(0, a[3][2][1], buf);
}

static void test_array_init12() {
    // 6.7.8.29
    // a, b and c are equivalent

    int a[4][3][2] = {
        {1},
        {2, 3},
        {4, 5, 6}
    };
    check_array_init12(&a, "Array init 12 a");

    int b[4][3][2] = {
        1, 0, 0, 0, 0, 0,
        2, 3, 0, 0, 0, 0,
        4, 5, 6
    };
    check_array_init12(&b, "Array init 12 b");

    int c[4][3][2] = {
        {{1}},
        {{2, 3}},
        {{4, 5}, {6}}
    };
    check_array_init12(&c, "Array init 12 c");
}

static void test_array_init13() {
    // 6.7.8.31
    typedef int A[];
    A a = { 1, 2 };
    A b = { 3, 4, 5  };
    int c[] = { 1, 2 }, d[] = { 3, 4, 5 };

    assert_int(8,  sizeof(a), "Array init 13 1 1");
    assert_int(12, sizeof(b), "Array init 13 1 2");
    assert_int(8,  sizeof(c), "Array init 13 1 3");
    assert_int(12, sizeof(d), "Array init 13 1 4");

    assert_int(1, a[0], "Array init 13 2a"); assert_int(1, c[0], "Array init 13 2b");
    assert_int(2, a[1], "Array init 13 3a"); assert_int(2, c[1], "Array init 13 3b");

    assert_int(3, b[0], "Array init 13 4a"); assert_int(3, d[0], "Array init 13 4b");
    assert_int(4, b[1], "Array init 13 5a"); assert_int(4, d[1], "Array init 13 5b");
    assert_int(5, b[2], "Array init 13 6a"); assert_int(5, d[2], "Array init 13 6b");
}

void test_array_init14() {
    int ia1[3][2] = {1,2,3,4,5,6};
    assert_int(1, ia1[0][0], "Array init 14 1 00");
    assert_int(2, ia1[0][1], "Array init 14 1 01");
    assert_int(3, ia1[1][0], "Array init 14 1 10");
    assert_int(4, ia1[1][1], "Array init 14 1 11");
    assert_int(5, ia1[2][0], "Array init 14 1 20");
    assert_int(6, ia1[2][1], "Array init 14 1 21");

    int ia2[3][2] = {{1,2},3,4,5,6};
    assert_int(1, ia2[0][0], "Array init 14 2 00");
    assert_int(2, ia2[0][1], "Array init 14 2 01");
    assert_int(3, ia2[1][0], "Array init 14 2 10");
    assert_int(4, ia2[1][1], "Array init 14 2 11");
    assert_int(5, ia2[2][0], "Array init 14 2 20");
    assert_int(6, ia2[2][1], "Array init 14 2 21");

    int ia3[3][3] = {1,{2,10,11},3,4,5,6,7,8,9};
    assert_int(1, ia3[0][0], "Array init 14 3 00");
    assert_int(2, ia3[0][1], "Array init 14 3 01");
    assert_int(3, ia3[0][2], "Array init 14 3 02");
    assert_int(4, ia3[1][0], "Array init 14 3 10");
    assert_int(5, ia3[1][1], "Array init 14 3 11");
    assert_int(6, ia3[1][2], "Array init 14 3 12");
    assert_int(7, ia3[2][0], "Array init 14 3 20");
    assert_int(8, ia3[2][1], "Array init 14 3 21");
    assert_int(9, ia3[2][2], "Array init 14 3 22");

    int ia4[3][3] = {1,2,3,{4,5,6},7,8,9,10};
    assert_int(1, ia4[0][0], "Array init 14 4 00");
    assert_int(2, ia4[0][1], "Array init 14 4 01");
    assert_int(3, ia4[0][2], "Array init 14 4 02");
    assert_int(4, ia4[1][0], "Array init 14 4 10");
    assert_int(5, ia4[1][1], "Array init 14 4 11");
    assert_int(6, ia4[1][2], "Array init 14 4 12");
    assert_int(7, ia4[2][0], "Array init 14 4 20");
    assert_int(8, ia4[2][1], "Array init 14 4 21");
    assert_int(9, ia4[2][2], "Array init 14 4 22");
}

void test_array_init15() {
    char prestring = 0;
    int a[2] = {1, 2, 3};
    char poststring = 0;
    assert_int(1, a[0],        "Array init 15 1");
    assert_int(2, a[1],        "Array init 15 2");
    assert_int(0, prestring,   "Array init 15 3");
    assert_int(0, poststring,  "Array init 15 4");
}

static void test_array_init16() {
    char ca2[] = {'a', 'b', 'c', 0};
    assert_int(4, sizeof(ca2), "Array init 16 1");
    assert_int('a', ca2[0],    "Array init 16 2");
    assert_int('b', ca2[1],    "Array init 16 3");
    assert_int('c', ca2[2],    "Array init 16 4");
    assert_int(0,   ca2[3],    "Array init 16 4");
}

static void test_array_init17() {
    int b;
    int a[] = {b = 1};
    assert_int(1, b,         "Array init 17 1");
    assert_int(4, sizeof(a), "Array init 17 1");
    assert_int(1, a[0],      "Array init 17 3");
}

void test_struct_init1() {
    struct s { int i; short j; struct { int k, l; } s; } s1 = {1, 2, 3, 4};
    assert_int(1, s1.i,   "Struct init 1 1");
    assert_int(2, s1.j,   "Struct init 1 2");
    assert_int(3, s1.s.k, "Struct init 1 3");
    assert_int(4, s1.s.l, "Struct init 1 4");

    struct st {int i; };
    struct s2 { struct st *st; };
    struct s2 s2 = {malloc(sizeof(struct st))};
    struct st st = *s2.st;
}

void test_struct_init2() {
    struct s {
        int i;
        struct {
            int j, k;
            struct {
                int l, m;
            } s;
        } s;
    };

    struct s v1 = {1, 2, 3, 4, 5, 6};
    assert_int(1, v1.i,     "Struct init 1i");
    assert_int(2, v1.s.j,   "Struct init 1j");
    assert_int(3, v1.s.k,   "Struct init 1k");
    assert_int(4, v1.s.s.l, "Struct init 1l");
    assert_int(5, v1.s.s.m, "Struct init 1m");
}

void test_struct_init3() {
    struct s {int i;} v1 = {1};
    struct s2 {
        struct s s;
        int i;
    };

    struct s2 v2 = {v1};
    assert_int(1, v2.s.i, "Struct init 3 1");

    int i = 1;
    struct {int i;} v3 = {i};
    assert_int(1, v3.i, "Struct init 3 2");
}

void test_struct_init4() {
    char prestring = 0;
    struct s {int i, j;} v = {1, 2, 3};
    char poststring = 0;
    assert_int(1, v.i,          "Struct init 4 1");
    assert_int(2, v.j,          "Struct init 4 2");
    assert_int(0, prestring,    "Struct init 4 3");
    assert_int(0, poststring,   "Struct init 4 4");
}

void test_struct_init5() {
    char prestring1 = 0;
    union {int i, j;} v1 = {1, 2, 3};
    char poststring1 = 0;
    assert_int(1, v1.i,         "Struct init 5 union 1 1");
    assert_int(1, v1.j,         "Struct init 5 union 1 2");
    assert_int(0, prestring1,   "Struct init 5 union 1 3");
    assert_int(0, poststring1,  "Struct init 5 union 1 4");

    char prestring2 = 0;
    union {
        int i, j;
        struct {int k, l;} s;
    } v2 = {1, 2, 3};
    char poststring2 = 0;
    assert_int(1, v2.i,         "Union init 5 union 2 5");
    assert_int(1, v2.j,         "Union init 5 union 2 6");
    assert_int(1, v2.s.k,       "Union init 5 union 2 7");
    assert_int(0, v2.s.l,       "Union init 5 union 2 8");
    assert_int(0, prestring2,   "Union init 5 union 2 9");
    assert_int(0, poststring2,  "Union init 5 union 2 10");
}

void test_struct_init6() {
    struct {char pc; char c;} v1 = {1, 2};
    assert_int(2, sizeof(v1),              "Struct init 6 char 1");
    assert_int(1, (int) &v1.c - (int) &v1, "Struct init 6 char 2");
    assert_int(1, v1.pc,                   "Struct init 6 char 3");
    assert_int(2, v1.c,                    "Struct init 6 char 4");

    struct {char pc; short s;} v2 = {1, 2};
    assert_int(4, sizeof(v2),              "Struct init 6 short 1");
    assert_int(2, (int) &v2.s - (int) &v2, "Struct init 6 short 2");
    assert_int(1, v2.pc,                   "Struct init 6 short 3");
    assert_int(2, v2.s,                    "Struct init 6 short 4");

    struct {char pc; int i;} v3 = {1, 2};
    assert_int(8, sizeof(v3),              "Struct init 6 int 1");
    assert_int(4, (int) &v3.i - (int) &v3, "Struct init 6 int 2");
    assert_int(1, v3.pc,                   "Struct init 6 int 3");
    assert_int(2, v3.i,                    "Struct init 6 int 4");

    struct {char pc; long ll;} v4a = {1, 2};
    assert_int(16, sizeof(v4a),               "Struct init 6 long 1");
    assert_int(8, (int) &v4a.ll - (int) &v4a, "Struct init 6 long 2");
    assert_int(1, v4a.pc,                     "Struct init 6 long 3");
    assert_int(2, v4a.ll,                     "Struct init 6 long 4");

    struct {char c1; short s1; int i1; long ll1; char c2;} v4 = {1, 2, 3, 4, 5};
    assert_int(24, sizeof(v4),                "Struct init 6 csillc size");
    assert_int(0,  (int) &v4.c1 -  (int) &v4, "Struct init 6 csillc c1 offset");
    assert_int(2,  (int) &v4.s1 -  (int) &v4, "Struct init 6 csillc s1 offset");
    assert_int(4,  (int) &v4.i1 -  (int) &v4, "Struct init 6 csillc i1 offset");
    assert_int(8,  (int) &v4.ll1 - (int) &v4, "Struct init 6 csillc ll1 offset");
    assert_int(16, (int) &v4.c2 -  (int) &v4, "Struct init 6 csillc c2 offset");
    assert_int(1, v4.c1,                      "Struct init 6 csillc c1 value");
    assert_int(2, v4.s1,                      "Struct init 6 csillc s1 value");
    assert_int(3, v4.i1,                      "Struct init 6 csillc i1 value");
    assert_int(4, v4.ll1,                     "Struct init 6 csillc ll1 value");
    assert_int(5, v4.c2,                      "Struct init 6 csillc c2 value");

    struct {int i; char ca[2];} v5 = {1, 2, 3};
    assert_int(8, sizeof(v5),               "Struct init 6 ica2 size");
    assert_int(0, (int) &v5.i  - (int) &v5, "Struct init 6 ica2 i offset");
    assert_int(4, (int) &v5.ca - (int) &v5, "Struct init 6 ica2 ca offset");
    assert_int(1, v5.i,                     "Struct init 6 ica2 i value");
    assert_int(2, v5.ca[0],                 "Struct init 6 ica2 ca[0] value");
    assert_int(3, v5.ca[1],                 "Struct init 6 ica2 ca[1] value");

    struct s1 {short s; char ca[3];} v6 = {1, 2, 3, 4};
    assert_int(6, sizeof(v6),               "Struct init 6 ica3 size");
    assert_int(0, (int) &v6.s  - (int) &v6, "Struct init 6 ica3 s offset");
    assert_int(2, (int) &v6.ca - (int) &v6, "Struct init 6 ica3 ca offset");
    assert_int(1, v6.s,                     "Struct init 6 ica3 s value");
    assert_int(2, v6.ca[0],                 "Struct init 6 ica3 ca[0] value");
    assert_int(3, v6.ca[1],                 "Struct init 6 ica3 ca[1] value");
    assert_int(4, v6.ca[2],                 "Struct init 6 ica3 ca[2] value");

    struct {short s; struct s1 v; int i;} v7[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    assert_int(24, sizeof(v7),                       "Struct alignment svi1[] size");
    assert_int(2,  (int) &v7[0].v  -   (int) &v7[0], "Struct alignment svi1 [0].v offset");
    assert_int(2,  (int) &v7[0].v.s  - (int) &v7[0], "Struct alignment svi1 [0].v.s offset");
    assert_int(4,  (int) &v7[0].v.ca - (int) &v7[0], "Struct alignment svi1 [0].v.ca offset");
    assert_int(8,  (int) &v7[0].i -    (int) &v7[0], "Struct alignment svi1 [0].v.i offset");
    assert_int(12, (int) &v7[1] -      (int) &v7[0], "Struct alignment svi1 [1] offset");
    assert_int(14, (int) &v7[1].v  -   (int) &v7[0], "Struct alignment svi1 [1].v offset");
    assert_int(14, (int) &v7[1].v.s  - (int) &v7[0], "Struct alignment svi1 [1].v.s offset");
    assert_int(16, (int) &v7[1].v.ca - (int) &v7[0], "Struct alignment svi1 [1].v.ca offset");
    assert_int(20, (int) &v7[1].i -    (int) &v7[0], "Struct alignment svi1 [1].v.i offset");
    assert_int(1,  v7[0].s,                          "Struct alignment svi1 [0].s value");
    assert_int(2,  v7[0].v.s,                        "Struct alignment svi1 [0].v.s value");
    assert_int(3,  v7[0].v.ca[0],                    "Struct alignment svi1 [0].ca[0] value");
    assert_int(4,  v7[0].v.ca[1],                    "Struct alignment svi1 [0].ca[1] value");
    assert_int(5,  v7[0].v.ca[2],                    "Struct alignment svi1 [0].ca[2] value");
    assert_int(6,  v7[0].i,                          "Struct alignment svi1 [0].i value");
    assert_int(7,  v7[1].s,                          "Struct alignment svi1 [1].s value");
    assert_int(8,  v7[1].v.s,                        "Struct alignment svi1 [1].v.s value");
    assert_int(9,  v7[1].v.ca[0],                    "Struct alignment svi1 [1].ca[0] value");
    assert_int(10, v7[1].v.ca[1],                    "Struct alignment svi1 [1].ca[1] value");
    assert_int(11, v7[1].v.ca[2],                    "Struct alignment svi1 [1].ca[2] value");
    assert_int(12, v7[1].i,                          "Struct alignment svi1 [1].i value");

    struct {char c1; long ll1; char c2;} v8 = {1, 0x100000001, 3};
    assert_int(24, sizeof(v8),                "Struct init 6 cllc size");
    assert_int(0,  (int) &v8.c1  - (int) &v8, "Struct init 6 cllc c1 offset");
    assert_int(8,  (int) &v8.ll1 - (int) &v8, "Struct init 6 cllc ll offset");
    assert_int(16, (int) &v8.c2  - (int) &v8, "Struct init 6 cllc c2 offset");
    assert_int(1,           v8.c1,            "Struct init 6 cllc c1 value");
    assert_int(0x100000001, v8.ll1,           "Struct init 6 cllc ll1 value");
    assert_int(3,           v8.c2,            "Struct init 6 cllc c2 value");

    // floating type assignment isn't implemented. Only the alignment can be checked.
    struct {char c1; float f; char c2; double d; char c3; long double ld; char c4;} v9;
    assert_int(64, sizeof(v9),                "Struct init 6 floating point types size");
    assert_int(0,  (int) &v9.c1  - (int) &v9, "Struct init 6 floating point types c1 offset");
    assert_int(4,  (int) &v9.f   - (int) &v9, "Struct init 6 floating point types f offset");
    assert_int(8,  (int) &v9.c2  - (int) &v9, "Struct init 6 floating point types c2 offset");
    assert_int(16, (int) &v9.d   - (int) &v9, "Struct init 6 floating point types d offset");
    assert_int(24, (int) &v9.c3  - (int) &v9, "Struct init 6 floating point types c3 offset");
    assert_int(32, (int) &v9.ld  - (int) &v9, "Struct init 6 floating point types ld offset");
    assert_int(48, (int) &v9.c4  - (int) &v9, "Struct init 6 floating point types c4 offset");
}

void test_struct_init7() {
    // Tests combinations of
    // a) array/struct init
    // b) pointer arithmetic on an array of structs
    // c) array lookups an array of structs
    // d) struct member lookup
    // The array has been declared using "struct s", which stress test
    // the incomplete struct lookup code

    struct s {int i, j;};
    struct s va[3] = {{1, 2}, {3, 4}, {5, 6}};
    assert_int(8,  sizeof(struct s), "Struct init 7 sanity 1a");
    assert_int(8,  sizeof(va[0]),    "Struct init 7 sanity 1b");
    assert_int(8,  sizeof(va[1]),    "Struct init 7 sanity 1c");
    assert_int(8,  sizeof(va[2]),    "Struct init 7 sanity 1d");
    assert_int(24, sizeof(va),       "Struct init 7 sanity 1e");
    struct s *ps;
    ps = &va;

    assert_int(1, va[0].i, "Struct init 7 2a");
    assert_int(2, va[0].j, "Struct init 7 2b");
    assert_int(3, va[1].i, "Struct init 7 2c");
    assert_int(4, va[1].j, "Struct init 7 2d");
    assert_int(5, va[2].i, "Struct init 7 2e");
    assert_int(6, va[2].j, "Struct init 7 2f");

    assert_int(1, ps[0].i, "Struct init 7 3a");
    assert_int(2, ps[0].j, "Struct init 7 3b");
    assert_int(3, ps[1].i, "Struct init 7 3c");
    assert_int(4, ps[1].j, "Struct init 7 3d");
    assert_int(5, ps[2].i, "Struct init 7 3e");
    assert_int(6, ps[2].j, "Struct init 7 3f");

    assert_int(1, ((ps + 0)[0]).i, "Struct init 7 4a");
    assert_int(3, ((ps + 0)[1]).i, "Struct init 7 4b");
    assert_int(3, ((ps + 1)[0]).i, "Struct init 7 4c");
    assert_int(5, ((ps + 1)[1]).i, "Struct init 7 4d");
    assert_int(6, ((ps + 1)[1]).j, "Struct init 7 4e");

    assert_int(1, ((0 + ps)[0]).i, "Struct init 7 5a");
    assert_int(3, ((0 + ps)[1]).i, "Struct init 7 5b");
    assert_int(3, ((1 + ps)[0]).i, "Struct init 7 5c");
    assert_int(5, ((1 + ps)[1]).i, "Struct init 7 5d");
    assert_int(6, ((1 + ps)[1]).j, "Struct init 7 5e");
}

static void test_struct_init8() {
    struct s1 { int i:3, j:3, k:3; } s1 = {1, 2, 3};
    assert_int(1, s1.i, "Struct init 8 1");
    assert_int(2, s1.j, "Struct init 8 2");
    assert_int(3, s1.k, "Struct init 8 3");

    struct s2 { int i:3, j:3, :0, k:3; } s2 = {1, 2, 3};
    assert_int(1, s2.i, "Struct init 8 4");
    assert_int(2, s2.j, "Struct init 8 5");
    assert_int(3, s2.k, "Struct init 8 6");
}

static void test_char_array_string_literal_inits() {
    char c1[] = "foo";
    assert_int(4, sizeof(c1), "sizeof(c1)");
    assert_int('f', c1[0], "c1[0]");
    assert_int('o', c1[1], "c1[1]");
    assert_int('o', c1[2], "c1[2]");
    assert_int(0,   c1[3], "c1[3]");

    char c1b[] = {"foo"};
    assert_int(4, sizeof(c1b), "sizeof(c1b)");
    assert_int('f', c1b[0], "c1b[0]");
    assert_int('o', c1b[1], "c1b[1]");
    assert_int('o', c1b[2], "c1b[2]");
    assert_int(0,   c1b[3], "c1b[3]");

    char c2[3] = "foo";
    assert_int(3, sizeof(c2), "sizeof(c2)");
    assert_int('f', c2[0], "c2[0]");
    assert_int('o', c2[1], "c2[1]");
    assert_int('o', c2[2], "c2[2]");

    struct s { char x[10]; } s = { "ABCDEFGHI" };
    assert_string("ABCDEFGHI", s.x, "String literal in sruct");
}

static void test_wide_char_array_string_literal_inits() {
    wchar_t wc1[] = L"foo";
    assert_int(16, sizeof(wc1), "wide char initialization size");
    assert_int('f', wc1[0], "wide char initialization f");
    assert_int('o', wc1[1], "wide char initialization o");
    assert_int('o', wc1[2], "wide char initialization o");
    assert_int(0,   wc1[3], "wide char initialization 0");

    wchar_t wc2[] = {L"foo"};
    assert_int(16, sizeof(wc2), "wide char initialization size");
    assert_int('f', wc2[0], "wide char initialization f");
    assert_int('o', wc2[1], "wide char initialization o");
    assert_int('o', wc2[2], "wide char initialization o");
    assert_int(0,   wc2[3], "wide char initialization 0");

    wchar_t wc3[3] = L"foo";
    assert_int(12, sizeof(wc3), "wide char initialization size");
    assert_int('f', wc3[0], "wide char initialization f");
    assert_int('o', wc3[1], "wide char initialization o");
    assert_int('o', wc3[2], "wide char initialization o");
}

static void test_char_array_bound_checks() {
    // Ensure string length determination is correct by checking variables
    // before & after in the stack are unaffected. This ensures the bounds
    // checking is correct.
    char prestring3 = 0;
    char string3[3] = "string";
    char poststring3 = 0;
    assert_int(3, sizeof(string3), "char[] side effects on stack test 1");
    assert_int(3, sizeof(string3), "char[] side effects on stack test 2");
    assert_int('s', string3[0],    "char[] side effects on stack test 3");
    assert_int('t', string3[1],    "char[] side effects on stack test 4");
    assert_int('r', string3[2],    "char[] side effects on stack test 5");
    assert_int(0, prestring3,      "char[] side effects on stack test 6");
    assert_int(0, poststring3,     "char[] side effects on stack test 7");
}

static void test_assignment_side_effects() {
    // Check two string literals assigned to char[] have their own storage
    char s1[] = "foobar";
    char s2[] = "foobar";
    assert_string("foobar", s1, "char[] assignment to string literals side effect test 1");
    assert_string("foobar", s2, "char[] assignment to string literals side effect test 2");
    s1[5] = 'z';
    assert_string("foobaz", s1, "char[] assignment to string literals side effect test 3");
    assert_string("foobar", s2, "char[] assignment to string literals side effect test 4");
}

static void test_array_of_strings() {
    // Test array of char *
    char *apc[3] = {"foo", "bar", "bazzer"};
    assert_string("foo",    apc[0], "Array of pointer to chars 1");
    assert_string("bar",    apc[1], "Array of pointer to chars 2");
    assert_string("bazzer", apc[2], "Array of pointer to chars 3");

    // Test array of array of char *
    char *aapc[3][3] = {
        {"a",       "ab",       "abc"},
        {"abcd",    "abcde",    "abcdef"},
        {"abcdefg", "abcdefgh", "abcdefghi"},
    };
    assert_string("a",              aapc[0][0], "Array of array of pointer to chars 00");
    assert_string("ab",             aapc[0][1], "Array of array of pointer to chars 01");
    assert_string("abc",            aapc[0][2], "Array of array of pointer to chars 02");
    assert_string("abcd",           aapc[1][0], "Array of array of pointer to chars 10");
    assert_string("abcde",          aapc[1][1], "Array of array of pointer to chars 11");
    assert_string("abcdef",         aapc[1][2], "Array of array of pointer to chars 12");
    assert_string("abcdefg",        aapc[2][0], "Array of array of pointer to chars 20");
    assert_string("abcdefgh",       aapc[2][1], "Array of array of pointer to chars 21");
    assert_string("abcdefghi",      aapc[2][2], "Array of array of pointer to chars 22");
}

static void test_string_initializers() {
    char *pc0;
    pc0 = "foo";
    assert_string("foo",  pc0, "Assign string literal to char *");

    char *pc1 = "foo\n";
    assert_string("foo\n",  pc1, "char * initialization with a string literal");

    char *pc2 = {"pc2"};
    assert_string("pc2", pc2, "char* initialization from {}");

    char ca1[] = {"string"};
    assert_int(7, sizeof(ca1),    "char[] initialization from {} 1");
    assert_string("string", ca1, "char[] initialization from {} 2");

    char ca2[] = {("string2")};
    assert_int(8, sizeof(ca2),    "char[] initialization from {{}} 1");
    assert_string("string2", ca2, "char[] initialization from {{}} 2");

    char ca3[] = "foo";
    char *i1 = {ca3};
    assert_int(&ca3, i1, "Implicit pointer conversion in {} assignment");
    char *i2 = {{ca3}};
    assert_int(&ca3, i2, "Implicit pointer conversion in {{}} assignment");

    char *i3 = {"foo"};
    assert_int(1, i3 > 0, "Implicit pointer conversion in {} assignment");
    char *i4 = {{"foo"}};
    assert_int(1, i4 > 0, "Implicit pointer conversion in {{}} assignment");
    char *i5 = {{"foo"}};
    assert_string("foo", i5, "char* init with string literal in {{}}");

    char *apc[] = {"foo", {"bar"}, {{"baz"}}, {{{"qux"}}}};
    assert_int(32, sizeof(apc),  "Array of chars init with {}s 1");
    assert_string("foo", apc[0], "Array of chars init with {}s 2");
    assert_string("bar", apc[1], "Array of chars init with {}s 3");
    assert_string("baz", apc[2], "Array of chars init with {}s 4");
    assert_string("qux", apc[3], "Array of chars init with {}s 5");

    assert_int(1, sizeof("a"[0]),   "Sizeof indexed string 1");
    assert_int(1, sizeof("ab"[0]),  "Sizeof indexed string 2");
    assert_int(1, sizeof("abc"[0]), "Sizeof indexed string 3");

    char ca4[] = {'a', 'b', 'c', 0};
    assert_string("abc", ca4, "char[] initialization with chars in {}");

    char ca5[] = "string 1a";
    assert_int(10, sizeof(ca5), "char[] initialization with string literal");

    char ca6[] = ("string 1b");
    assert_int(10, sizeof(ca6), "char[] initialization with (string literal");

    char ca7[] = "";
    assert_int(1,  sizeof(ca7), "char[] initialization with \"\"");

    char ca8[3] = "string";
    ca8[2] = 'f';
    assert_int('f', ca8[2], "char[] element assignment");
}

static void test_zeroing() {
    int sum;
    char c1[2]    = {1}; sum = 0; for (int i = 1; i < sizeof(c1); i++) sum += c1[i]; assert_int(0, sum, "Zeroing 2");
    char c2[5]    = {1}; sum = 0; for (int i = 1; i < sizeof(c2); i++) sum += c2[i]; assert_int(0, sum, "Zeroing 5");
    char c3[15]   = {1}; sum = 0; for (int i = 1; i < sizeof(c3); i++) sum += c3[i]; assert_int(0, sum, "Zeroing 15");
    char c4[31]   = {1}; sum = 0; for (int i = 1; i < sizeof(c4); i++) sum += c4[i]; assert_int(0, sum, "Zeroing 31");
    char c5[1000] = {1}; sum = 0; for (int i = 1; i < sizeof(c5); i++) sum += c5[i]; assert_int(0, sum, "Zeroing 1000"); // memset version
}

static void test_global_sizeof_types() {
    assert_int(1, csize,  "Global initialization with sizeof 1");
    assert_int(2, ssize,  "Global initialization with sizeof 2");
    assert_int(4, isize,  "Global initialization with sizeof 3");
    assert_int(8, llsize, "Global initialization with sizeof 4");
}

void test_global_initialization() {
    assert_int(1, gc,                 "Global char initialization 1");
    assert_int(gpc1, &gc,             "Global char initialization 2");
    assert_int(gpc2, ((int) &gc) + 1, "Global char initialization 3");
    assert_int(gpc3, ((int) &gc) + 2, "Global char initialization 4");

    assert_int(1, gs,                 "Global short initialization 1");
    assert_int(gps1, &gs,             "Global short initialization 2");
    assert_int(gps2, ((int) &gs) + 2, "Global short initialization 3");
    assert_int(gps3, ((int) &gs) + 4, "Global short initialization 4");

    assert_int(1, gi,                 "Global int initialization 1");
    assert_int(gpi1, &gi,             "Global int initialization 2");
    assert_int(gpi2, ((int) &gi) + 4, "Global int initialization 3");
    assert_int(gpi3, ((int) &gi) + 8, "Global int initialization 4");
    assert_int(gpi4, ((int) &gi) + 4, "Global int initialization 5");
    assert_int(gpi5, ((int) &gi) - 4, "Global int initialization 6");

    assert_int(gpia1, &gia,              "Global int array initialization 1");
    assert_int(gpia2, &gia + 1,          "Global int array initialization 2");
    assert_int(gpia2, ((int) &gia) + 8,  "Global int array initialization 3");
    assert_int(gpia3, ((int) &gia) + 16, "Global int array initialization 4");

    assert_long(18446744073709551615, gll, "Global long initialization");
    assert_int(gplla1, &glla,              "Global long array initialization 1");
    assert_int(gplla2, &glla + 1,          "Global long array initialization 2");
    assert_int(gplla2, ((int) &glla) + 16, "Global long array initialization 3");
    assert_int(gplla3, ((int) &glla) + 32, "Global long array initialization 4");

    assert_int(1, gia2[0], "Global int array initialization 5");
    assert_int(2, gia2[1], "Global int array initialization 6");
    assert_int(0, gia2[2], "Global int array initialization 7");

    assert_int(12, sizeof(gia3), "Global incomplete array completion 1");
    assert_int(1, gia3[0], "Global incomplete array completion 2");
    assert_int(2, gia3[1], "Global incomplete array completion 3");
    assert_int(3, gia3[2], "Global incomplete array completion 4");

    assert_float(1.1, gf, "Global float initialization");
    assert_float(1.2, gd, "Global double initialization");
    assert_float(1.3, gld, "Global long double initialization");

    assert_int(0,  gpv1,   "Global pointer to void 1a");
    assert_int(0,  gpv2,   "Global pointer to void 1b");
    assert_int(1,  gpvc1,  "Global pointer to void 1c");
    assert_int(2,  gpvs1,  "Global pointer to void 1d");
    assert_int(4,  gpvi1,  "Global pointer to void 1e");
    assert_int(8,  gpvll1, "Global pointer to void 1f");
    assert_int(-1, gpvc2,  "Global pointer to void 1g");
    assert_int(-2, gpvs2,  "Global pointer to void 1h");
    assert_int(-4, gpvi2,  "Global pointer to void 1i");
    assert_int(-8, gpvll2, "Global pointer to void 1j");

    assert_int(gpvca,  ((int) &ca) + 10, "Global pointer to void 2a");
    assert_int(gpvsa,  ((int) &sa) + 20, "Global pointer to void 2b");
    assert_int(gpvia,  ((int) &ia) + 40, "Global pointer to void 2c");
    assert_int(gpvlla, ((int) &la) + 80, "Global pointer to void 2d");

    assert_int(gpvca1, ((int) &ca) + 1, "Global pointer to void 3a");
    assert_int(gpvsa1, ((int) &sa) + 2, "Global pointer to void 3b");
    assert_int(gpvia1, ((int) &ia) + 4, "Global pointer to void 3c");
    assert_int(gpvla1, ((int) &la) + 8, "Global pointer to void 3d");

    assert_int(gpvca2, ((int) &ca) + 2,  "Global pointer to void 4a");
    assert_int(gpvsa2, ((int) &sa) + 4,  "Global pointer to void 4b");
    assert_int(gpvia2, ((int) &ia) + 8,  "Global pointer to void 4c");
    assert_int(gpvla2, ((int) &la) + 16, "Global pointer to void 4d");

    assert_int(gpvia25_00,  ((int) &ia25) + 0,  "Global pointer to void 5a");
    assert_int(gpvia25_01,  ((int) &ia25) + 4,  "Global pointer to void 5b");
    assert_int(gpvia25_10,  ((int) &ia25) + 20, "Global pointer to void 5c");
    assert_int(gpvia25_11,  ((int) &ia25) + 24, "Global pointer to void 5d");
    assert_int(gpvia25_11x, ((int) &ia25) + 28, "Global pointer to void 5e");

    assert_string("string1", string1, "Global string initialization 1");
    assert_string("tring2",  string2, "Global string initialization 2");
    assert_string("ring3",   string3, "Global string initialization 3");

    assert_string("chararr1", ca1,    "Global char array initialization straight");
    assert_string("chararr2", ca2,    "Global char array initialization with {}");
    assert_int('f', ca3[0],           "Global char array initialization [2][0]");
    assert_int('o', ca3[1],           "Global char array initialization [2][1]");
    assert_int('f', ca4[0],           "Global char array initialization [3][0]");
    assert_int('o', ca4[1],           "Global char array initialization [3][1]");
    assert_int('o', ca4[2],           "Global char array initialization [3][2]");
    assert_string("foo", ca5,         "Global char array initialization [4]");
    assert_int('a', ca5post,          "Global char array initialization ca5post");
    assert_int(4, sizeof(ca6),        "Global char array initialization ca6 size");
    assert_string("foo", ca6,         "Global char array initialization ca6");
    assert_int(2, sizeof(ca7),        "Global char array initialization ca7 size");
    assert_int('f', ca7[0],           "Global char array initialization ca7[0]");

    assert_int(16, sizeof(wc1), "Global char array initialization wc1 size");
    assert_int('f', wc1[0],     "Global char array initialization wc1 f");
    assert_int('o', wc1[1],     "Global char array initialization wc1 o");
    assert_int('o', wc1[2],     "Global char array initialization wc1 o");
    assert_int(0,   wc1[3],     "Global char array initialization wc1 0");

    assert_int(8, sizeof(wc2), "Global char array initialization wc2 size");
    assert_int('f', wc2[0],    "Global char array initialization wc2 f");
    assert_int('o', wc2[1],    "Global char array initialization wc2 0");

    assert_string("foo",    apc[0], "Array of pointer to chars 1");
    assert_string("bar",    apc[1], "Array of pointer to chars 2");
    assert_string("bazzer", apc[2], "Array of pointer to chars 3");

    assert_string("a",              aapc[0][0], "Array of array of pointer to chars 00");
    assert_string("ab",             aapc[0][1], "Array of array of pointer to chars 01");
    assert_string("abc",            aapc[0][2], "Array of array of pointer to chars 02");
    assert_string("abcd",           aapc[1][0], "Array of array of pointer to chars 10");
    assert_string("abcde",          aapc[1][1], "Array of array of pointer to chars 11");
    assert_string("abcdef",         aapc[1][2], "Array of array of pointer to chars 12");
    assert_string("abcdefg",        aapc[2][0], "Array of array of pointer to chars 20");
    assert_string("abcdefgh",       aapc[2][1], "Array of array of pointer to chars 21");
    assert_string("abcdefghi",      aapc[2][2], "Array of array of pointer to chars 22");

    assert_int(1, ccast,  "Global cast from integer 1");
    assert_int(1, scast,  "Global cast from integer 2");
    assert_int(1, icast,  "Global cast from integer 3");
    assert_int(1, llcast, "Global cast from integer 4");

    assert_int(100, enum_const1, "Enum constant usage in constant expression 1");
    assert_int(101, enum_const2, "Enum constant usage in constant expression 2");

    assert_int(100, enum1, "Enum constant usage in constant expression 2");
    assert_int(101, enum2, "Enum constant usage in constant expression 3");

    assert_int(1, st2.i, "Global struct initialization 1");
    assert_int(2, st2.j, "Global struct initialization 2");
    assert_int(3, st2.k, "Global struct initialization 3");
    assert_int(0, st2.l, "Global struct initialization 4");

    assert_int(-1, bfs.i, "Global bit fields struct initialization 1");
    assert_int(-2, bfs.j, "Global bit fields struct initialization 2");
    assert_int(-3, bfs.k, "Global bit fields struct initialization 3");
    assert_int(-4, bfs.l, "Global bit fields struct initialization 4");
    assert_int(-5, bfs.m, "Global bit fields struct initialization 5");

    assert_int(spi1, &sv1.a,              "Constant expression with structs 1");
    assert_int(spi2, ((int) &sv1.a) + 4,  "Constant expression with structs 2");
    assert_int(spi3, ((int) &sv1.a) + 8,  "Constant expression with structs 3");
    assert_int(spi4, ((int) &sv1.b),      "Constant expression with structs 4");
    assert_int(spi5, ((int) &sv1.b) + 4,  "Constant expression with structs 5");

    assert_int(12, sizeof(s2),              "Static struct alignment csic size");
    assert_int(0, (int) &s2.c1 - (int) &s2, "Static struct alignment csic c1 offset");
    assert_int(2, (int) &s2.s1 - (int) &s2, "Static struct alignment csic s1 offset");
    assert_int(4, (int) &s2.i1 - (int) &s2, "Static struct alignment csic i1 offset");
    assert_int(8, (int) &s2.c2 - (int) &s2, "Static struct alignment csic c2 offset");
    assert_int(1, s2.c1,                    "Static struct alignment csic c1 value");
    assert_int(2, s2.s1,                    "Static struct alignment csic s1 value");
    assert_int(3, s2.i1,                    "Static struct alignment csic i1 value");
    assert_int(4, s2.c2,                    "Static struct alignment csic c2 value");

    assert_int(1,  s3[0].s,                 "Static struct alignment svi1 [0].s value");
    assert_int(2,  s3[0].v.s,               "Static struct alignment svi1 [0].v.s value");
    assert_int(3,  s3[0].v.ca[0],           "Static struct alignment svi1 [0].ca[0] value");
    assert_int(4,  s3[0].v.ca[1],           "Static struct alignment svi1 [0].ca[1] value");
    assert_int(5,  s3[0].v.ca[2],           "Static struct alignment svi1 [0].ca[2] value");
    assert_int(6,  s3[0].i,                 "Static struct alignment svi1 [0].i value");
    assert_int(7,  s3[1].s,                 "Static struct alignment svi1 [1].s value");
    assert_int(8,  s3[1].v.s,               "Static struct alignment svi1 [1].v.s value");
    assert_int(9,  s3[1].v.ca[0],           "Static struct alignment svi1 [1].ca[0] value");
    assert_int(10, s3[1].v.ca[1],           "Static struct alignment svi1 [1].ca[1] value");
    assert_int(11, s3[1].v.ca[2],           "Static struct alignment svi1 [1].ca[2] value");
    assert_int(12, s3[1].i,                 "Static struct alignment svi1 [1].i value");

    assert_int(24, sizeof(s4),               "Static struct alignment cllc size");
    assert_int(0,  (int) &s4.c1 - (int) &s4, "Static struct alignment cllc c1 offset");
    assert_int(8,  (int) &s4.ll - (int) &s4, "Static struct alignment cllc ll offset");
    assert_int(16, (int) &s4.c2 - (int) &s4, "Static struct alignment cllc c2 offset");

    assert_int(64, sizeof(s5),               "Static struct alignment floating point types size");
    assert_int(0,  (int) &s5.c1 - (int) &s5, "Static struct alignment floating point types c1 offset");
    assert_int(4,  (int) &s5.f  - (int) &s5, "Static struct alignment floating point types f offset");
    assert_int(8,  (int) &s5.c2 - (int) &s5, "Static struct alignment floating point types c2 offset");
    assert_int(16, (int) &s5.d  - (int) &s5, "Static struct alignment floating point types d offset");
    assert_int(24, (int) &s5.c3 - (int) &s5, "Static struct alignment floating point types c3 offset");
    assert_int(32, (int) &s5.ld - (int) &s5, "Static struct alignment floating point types ld offset");

    // Test conversions
    static int         i  = 1.1;  assert_int(1, i,          "Static type conversions 1");
    static float       f1 = 1;    assert_float(1, f1,       "Static type conversions 2");
    static float       f2 = 1.1;  assert_float(1.1, f2,     "Static type conversions 3");
    static long double ld = 1;    assert_long_double(1, ld, "Static type conversions 4");

    assert_int(1, global_func_ptr == plus,   "global_func_ptr = plus 1");
    assert_int(1, global_func_ptr == &plus,  "global_func_ptr = plus 2");
    assert_int(1, global_func_ptr2 == plus,  "global_func_ptr2 = &plus 1");
    assert_int(1, global_func_ptr2 == &plus, "global_func_ptr2 = &plus 2");

    // Structs, pointers, sizeof and offsetof
    assert_int(8, sizeof(ac1v), ". and -> ac1v");
    assert_int(8, sizeof(ac1c), ". and -> ac1c");
    assert_int(4, sizeof(ac1s), ". and -> ac1s");
    assert_int(2, sizeof(ac1i), ". and -> ac1i");
    assert_int(1, sizeof(ac1l), ". and -> ac1l");
    assert_int(8, sizeof(ac2),  ". and -> ac2");
    assert_int(8, sizeof(ac3),  ". and -> ac3");
    assert_int(8, sizeof(ac4),  ". and -> ac4");

    assert_string(pc, "Hello", "char *pc = ac");

    assert_int(1, *(int *) pgi, "void *pgi = (void *) &gi");
    assert_string(" ", space, "static char * const space[] = { (char *) \" \" }");
}

void func_with_static_string(char expected_first_char, char* message) {
    // Ensure this one doesn't change between calls
    char s[] = "Static string";
    s[0]++;
    assert_string("Ttatic string", s, message);

    // The first character should keep increasing between calls
    static char ss[] = "Static string";
    ss[0]++;
    assert_int(expected_first_char, ss[0], message);
}

void func_with_static_array(char expected0, char expected1, char* message) {
    static int sia[2] = {10, 10};
    sia[0]++;
    sia[1]--;
    assert_int(expected0, sia[0], message);
    assert_int(expected1, sia[1], message);
}

void func_with_static_struct(char expected_c, short expected_s, int expected_i, char* message) {
    static struct {char c; short s; int i;} s = {10, 11, 12};
    s.c++;
    s.s++;
    s.i++;
    assert_int(expected_c, s.c, message);
    assert_int(expected_s, s.s, message);
    assert_int(expected_i, s.i, message);
}

void test_static_local_initialization() {
    // This is a subset of the global ones. This is just a sanity test
    // to ensure that the global initialization cases also work for local
    // statics.

    static int i = 1;
    static int *pi = &i;
    assert_int(pi, &i, "Static local initialization 1");

    static char ca[] = "Local static char array";
    assert_string("Local static char array", ca, "Static local initialization 2");
    ca[0] = 'F';
    assert_string("Focal static char array", ca, "Static local initialization 3");

    func_with_static_string('T', "Static local string 1");
    func_with_static_string('U', "Static local string 2");

    func_with_static_array(11, 9, "Static local array 1");
    func_with_static_array(12, 8, "Static local array 2");

    func_with_static_struct(11, 12, 13, "Static local struct 1");
    func_with_static_struct(12, 13, 14, "Static local struct 2");

    static long ll = 0xffffffffffffffff;
    assert_long(18446744073709551615, ll, "Static long initialization");
}

int gi2 = 10;

void fn(int expected) {
    static int gi2 = 1; // Same identifier as a global of the same name
    assert_int(expected, gi2++, "Static int identifier reuse bug");
}

static void test_static_identifier_reuse_bug() {
    fn(1);
    fn(2);
    fn(3);
    assert_int(10, gi2, "Static int identifier reuse bug, gi2 is untouched");
}

struct s12 {
    long l[3];
    int i;
} s12 = { 1, 2, 3, 4 };

char ca8[4] = { 1, 2, 3, 4 };

int test_zero_padding_at_end_of_struct() {
    memset(&s12, 0, sizeof(struct s12));
    assert_int(1, ca8[0], "Zero padding at end of struct 1");
    assert_int(2, ca8[1], "Zero padding at end of struct 2");
    assert_int(3, ca8[2], "Zero padding at end of struct 3");
    assert_int(4, ca8[3], "Zero padding at end of struct 4");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    // Sanity
    test_scalar_initializers();

    // Arrays
    test_array_init1();
    test_array_init2();
    test_array_init3();
    test_array_init4();
    test_array_init5();
    test_array_init6();
    test_array_init7();
    test_array_init8();
    test_array_init9();
    test_array_init10();
    test_array_init11();
    test_array_init12();
    test_array_init13();
    test_array_init14();
    test_array_init15();
    test_array_init16();
    test_array_init17();

    // Structs
    test_struct_init1();
    test_struct_init2();
    test_struct_init3();
    test_struct_init4();
    test_struct_init5();
    test_struct_init6();
    test_struct_init7();
    test_struct_init8();

    // Strings
    test_char_array_string_literal_inits();
    test_char_array_bound_checks();
    test_assignment_side_effects();
    test_array_of_strings();
    test_string_initializers();
    test_wide_char_array_string_literal_inits();

    // Misc
    test_zeroing();

    // In case you're wondering, the mad numbering is a leftover from a much earlier
    // version of cwwc, written in Cornwall, I lifted many tests from there.

    // Initialization of objects with static storage duration
    test_global_sizeof_types();
    test_global_initialization();
    test_static_local_initialization();
    test_static_identifier_reuse_bug();
    test_zero_padding_at_end_of_struct();

    finalize();
}
