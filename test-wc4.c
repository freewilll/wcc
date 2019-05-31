#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int check_stack_alignment();

int verbose;
int passes;
int failures;

int g;

int gcvi, gcvj;

struct s1 {
    int i;
};

struct s2 {
    int    i1;
    long   l1;
    int    i2;
    short  s1;
};

typedef struct s1 S1;

typedef struct _s2_ { int i; } S2;

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

struct sc1 { char  c1;           };
struct sc2 { char  c1; char c2;  };
struct ss1 { short c1;           };
struct ss2 { short c1; short s1; };
struct si1 { int   c1;           };
struct si2 { int   c1; int i1;   };
struct sl1 { long  c1;           };
struct sl2 { long  c1; long l1;  };

struct cc { char c1; char  c2; };
struct cs { char c1; short s1; };
struct ci { char c1; int   i1; };
struct cl { char c1; long  l1; };

struct ccc { char c1; char  c2; char c3; };
struct csc { char c1; short c2; char c3; };
struct cic { char c1; int   c2; char c3; };
struct clc { char c1; long  c2; char c3; };

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

struct ups {
    int i, j;
};

struct csrs {
    int i;
};

void assert_int(int expected, int actual, char *message) {
    if (expected != actual) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %d got %d\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

void assert_long(long expected, long actual, char *message) {
    if (expected != actual) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected %ld got %ld\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

void assert_string(char *expected, char *actual, char *message) {
    if (strcmp(expected, actual)) {
        failures++;
        printf("%-60s ", message);
        printf("failed, expected \"%s\" got \"%s\"\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) {
            printf("%-60s ", message);
            printf("ok\n");
        }
    }
}

void finalize() {
    if (failures == 0) {
        printf("All %d tests passed\n", passes + failures);
    }
    else {
        printf("%d out of %d tests failed\n", failures, passes + failures);
        exit(1);
    }
}

void test_expr() {
    int i, j, k, l;

    i = 1; j = 2; assert_long( 1,  1,               "1");
    i = 1; j = 2; assert_long( 3,  1+2,             "1+2 a");
    i = 1; j = 2; assert_long( 3,  i+2,             "1+2 b");
    i = 1; j = 2; assert_long( 3,  1+j,             "1+2 c");
    i = 1; j = 2; assert_long( 3,  i+j,             "1+2 d");
    i = 1; j = 3; assert_long( 2,  3-1,             "3-1 a");
    i = 1; j = 3; assert_long( 2,  j-1,             "3-1 b");
    i = 1; j = 3; assert_long( 2,  3-i,             "3-1 c");
    i = 1; j = 3; assert_long( 2,  j-i,             "3-1 d");
    i = 1; j = 2; assert_long( 1,  3-2,             "3-2");
    i = 1; j = 2; assert_long( 0,  3-2-1,           "3-2-1");
    i = 1; j = 2; assert_long( 4,  3+2-1,           "3+2-1");
    i = 1; j = 2; assert_long( 2,  3-2+1,           "3-2+1");
    i = 2; j = 3; assert_long( 6,  2*3,             "2*3 a");
    i = 2; j = 3; assert_long( 6,  i*3,             "2*3 b");
    i = 2; j = 3; assert_long( 6,  2*j,             "2*3 c");
    i = 2; j = 3; assert_long( 6,  i*j,             "2*3 d");
    i = 1; j = 2; assert_long( 7,  1+2*3,           "1+2*3");
    i = 1; j = 2; assert_long(10,  2*3+4,           "2*3+4");
    i = 1; j = 2; assert_long( 3,  2*3/2,           "2*3/2");
    i = 6; j = 2; assert_long( 3,  6/2,             "6/2 a");
    i = 6; j = 2; assert_long( 3,  i/2,             "6/2 b");
    i = 6; j = 2; assert_long( 3,  6/j,             "6/2 c");
    i = 6; j = 2; assert_long( 3,  i/j,             "6/2 d");
    i = 6; j = 2; assert_long( 0,  6%2,             "6%2 a");
    i = 6; j = 2; assert_long( 0,  i%2,             "6%2 b");
    i = 6; j = 2; assert_long( 0,  6%j,             "6%2 c");
    i = 6; j = 2; assert_long( 0,  i%j,             "6%2 d");
    i = 7; j = 2; assert_long( 1,  7%2,             "7%2 a");
    i = 7; j = 2; assert_long( 1,  i%2,             "7%2 b");
    i = 7; j = 2; assert_long( 1,  7%j,             "7%2 c");
    i = 7; j = 2; assert_long( 1,  i%j,             "7%2 d");
    i = 1; j = 2; assert_long(64,  1*2+3*4+5*10,    "1*2+3*4+5*10");
    i = 1; j = 2; assert_long(74,  1*2+3*4+5*10+10, "1*2+3*4+5*10+10");
    i = 1; j = 2; assert_long( 9,  (1+2)*3,         "(1+2)*3");
    i = 1; j = 2; assert_long( 7,  (2*3)+1,         "(2*3)+1");
    i = 1; j = 2; assert_long( 0,  3-(2+1),         "3-(2+1)");
    i = 1; j = 2; assert_long(-3,  3-(2+1)*2,       "3-(2+1)*2");
    i = 1; j = 2; assert_long( 7,  3-(2+1)*2+10,    "3-(2+1)*2+10");
    i = 1; j = 2; assert_long(-1,  -1,              "-1");
    i = 1; j = 2; assert_long( 1,  -1+2,            "-1+2");
    i = 1; j = 2; assert_long(-2,  -1-1,            "-1-1");
    i = 1; j = 2; assert_long( 3,  2- -1,           "2- -1");
    i = 2; j = 3; assert_long(-6,  -(2*3),          "-(2*3) a");
    i = 2; j = 3; assert_long(-6,  -(i*3),          "-(2*3) b");
    i = 2; j = 3; assert_long(-6,  -(2*j),          "-(2*3) c");
    i = 2; j = 3; assert_long(-6,  -(i*j),          "-(2*3) d");
    i = 1; j = 2; assert_long(-5,  -(2*3)+1,        "-(2*3)+1");
    i = 1; j = 2; assert_long(-11, -(2*3)*2+1,      "-(2*3)*2+1");

    i = 0; j = 1; k = 2; l = 3;

    assert_long(                   1, 1 == 1,            "1 == 1"      );
    assert_long(                   1, j == 1,            "j == 1"      );
    assert_long(                   1, 1 == j,            "i == i"      );
    assert_long(                   1, 2 == 2,            "2 == 2"      );
    assert_long(                   0, 1 == 0,            "1 == 0"      );
    assert_long(                   0, 0 == 1,            "0 == 1"      );
    assert_long(                   0, 1 != 1,            "1 != 1"      );
    assert_long(                   0, 2 != 2,            "2 != 2"      );
    assert_long(                   1, 1 != 0,            "1 != 0"      );
    assert_long(                   1, 0 != 1,            "0 != 1"      );
    assert_long(                   1, !0,                "!0 a"        );
    assert_long(                   1, !i,                "!0 b"        );
    assert_long(                   0, !1,                "!1 a"        );
    assert_long(                   0, !j,                "!1 b"        );
    assert_long(                   0, !2,                "!2 a"        );
    assert_long(                   0, !k,                "!2 b"        );
    assert_long(                  -1, ~0,                "~0 a"        );
    assert_long(                  -1, ~i,                "~0 b"        );
    assert_long(                  -2, ~1,                "~1 a"        );
    assert_long(                  -2, ~j,                "~1 b"        );
    assert_long(                  -3, ~2,                "~2 a"        );
    assert_long(                  -3, ~k,                "~2 b"        );

    assert_long(                   1, 0 <  1,            "0 <  1 a"     );
    assert_long(                   1, i <  1,            "0 <  1 b"     );
    assert_long(                   1, 0 <  j,            "0 <  1 c"     );
    assert_long(                   1, i <  j,            "0 <  1 d"     );
    assert_long(                   1, 0 <= 1,            "0 <= 1 a"     );
    assert_long(                   1, i <= 1,            "0 <= 1 b"     );
    assert_long(                   1, 0 <= j,            "0 <= 1 c"     );
    assert_long(                   1, i <= j,            "0 <= 1 d"     );
    assert_long(                   0, 0 >  1,            "0 >  1 a"     );
    assert_long(                   0, i >  1,            "0 >  1 b"     );
    assert_long(                   0, 0 >  j,            "0 >  1 c"     );
    assert_long(                   0, i >  j,            "0 >  1 d"     );
    assert_long(                   0, 0 >= 1,            "0 >= 1 a"     );
    assert_long(                   0, i >= 1,            "0 >= 1 b"     );
    assert_long(                   0, 0 >= j,            "0 >= 1 c"     );
    assert_long(                   0, i >= j,            "0 >= 1 d"     );
    assert_long(                   0, 1 <  1,            "1 <  1 a"     );
    assert_long(                   0, i >= 1,            "0 >= 1 b"     );
    assert_long(                   0, 0 >= j,            "0 >= 1 c"     );
    assert_long(                   1, i <= j,            "1 <= 1 d"     );
    assert_long(                   1, 1 <= 1,            "1 <= 1 a"     );
    assert_long(                   1, i <= 1,            "1 <= 1 b"     );
    assert_long(                   0, 1 >  j,            "1 >  1 c"     );
    assert_long(                   0, i >  j,            "1 >  1 d"     );
    assert_long(                   0, 1 >  1,            "1 >  1 a"     );
    assert_long(                   1, j >= 1,            "1 >= 1 b"     );
    assert_long(                   1, 1 >= j,            "1 >= 1 c"     );
    assert_long(                   1, j >= j,            "1 >= 1 d"     );

    assert_long(                   0, 0 || 0,            "0 || 0"      );
    assert_long(                   1, 0 || 1,            "0 || 1"      );
    assert_long(                   1, 1 || 0,            "1 || 0"      );
    assert_long(                   1, 1 || 1,            "1 || 1"      );
    assert_long(                   0, 0 && 0,            "0 && 0"      );
    assert_long(                   0, 0 && 1,            "0 && 1"      );
    assert_long(                   0, 1 && 0,            "1 && 0"      );
    assert_long(                   1, 1 && 1,            "1 && 1"      );

    assert_long(                   1, 2 || 0,            "2 || 0"      ); // Ensure that the result is always 1 or zero
    assert_long(                   1, 0 || 2,            "0 || 2"      );
    assert_long(                   1, 2 && 3,            "2 && 3"      );

    i = 3; j = 5;
    assert_long(                   1, 3 & 5,             "3 & 5 a"     );
    assert_long(                   1, i & 5,             "3 & 5 b"     );
    assert_long(                   1, 3 & j,             "3 & 5 c"     );
    assert_long(                   1, i & j,             "3 & 5 d"     );
    assert_long(                   7, 3 | 5,             "3 | 5 a"     );
    assert_long(                   7, i | 5,             "3 | 5 b"     );
    assert_long(                   7, 3 | j,             "3 | 5 c"     );
    assert_long(                   7, i | j,             "3 | 5 d"     );
    assert_long(                   6, 3 ^ 5,             "3 ^ 5 a"     );
    assert_long(                   6, i ^ 5,             "3 ^ 5 b"     );
    assert_long(                   6, 3 ^ j,             "3 ^ 5 c"     );
    assert_long(                   6, i ^ j,             "3 ^ 5 d"     );

    i = 1; j = 2;
    assert_long(                  4, 1 << 2,            "1 << 2 a");
    assert_long(                  4, i << 2,            "1 << 2 b");
    assert_long(                  4, 1 << j,            "1 << 2 c");
    assert_long(                  4, i << j,            "1 << 2 d");
    assert_long(                  8, 1 << 3,            "1 << 3"  );
    assert_long(         1073741824, 1 << 30,           "1 << 31" );
    assert_long(         2147483648, (long) 1 << 31,    "1 << 31" );
    assert_long(4611686018427387904, (long) 1 << 62,    "1 << 62" );

    i = 256; j = 2;
    assert_long(                  64, 256 >> 2,          "256 >> 2 a"    );
    assert_long(                  64, i   >> 2,          "256 >> 2 b"    );
    assert_long(                  64, 256 >> j,          "256 >> 2 c"    );
    assert_long(                  64, i   >> j,          "256 >> 2 d"    );
    assert_long(                  32, 256 >> 3,          "256 >> 3"    );
    assert_long(                  32, 8192 >> 8,         "8192 >> 8"   );

    assert_long(                   1, 1+2==3,            "1+2==3"      );
    assert_long(                   1, 1+2>=3==1,         "1+2>=3==1"   );
    assert_long(                   0, 0 && 0 || 0,       "0 && 0 || 0" ); // && binds more strongly than ||
    assert_long(                   1, 0 && 0 || 1,       "0 && 0 || 1" );
    assert_long(                   0, 0 && 1 || 0,       "0 && 1 || 0" );
    assert_long(                   1, 0 && 1 || 1,       "0 && 1 || 1" );
    assert_long(                   0, 1 && 0 || 0,       "1 && 0 || 0" );
    assert_long(                   1, 1 && 0 || 1,       "1 && 0 || 1" );
    assert_long(                   1, 1 && 1 || 0,       "1 && 1 || 0" );
    assert_long(                   1, 1 && 1 || 1,       "1 && 1 || 1" );
    assert_long(                   0, 0 + 0 && 0,        "0 + 0 && 0"  ); // + binds more strongly than &&
    assert_long(                   0, 0 + 0 && 1,        "0 + 0 && 1"  );
    assert_long(                   0, 0 + 1 && 0,        "0 + 1 && 0"  );
    assert_long(                   1, 0 + 1 && 1,        "0 + 1 && 1"  );
    assert_long(                   0, 1 + 0 && 0,        "1 + 0 && 0"  );
    assert_long(                   1, 1 + 0 && 1,        "1 + 0 && 1"  );
    assert_long(                   0, 1 + 1 && 0,        "1 + 1 && 0"  );
    assert_long(                   1, 1 + 1 && 1,        "1 + 1 && 1"  );
    assert_long(                   0, 0 ==  1  & 0,      "0 ==  1  & 0");
    assert_long(                   2, 1 &   1  ^ 3,      "1 &   1  ^ 3");
    assert_long(                   1, 1 ^   1  | 1,      "1 ^   1  | 1");
    assert_long(                  32, 1 + 1 << 4,        "1 + 1 << 4"  ); // + binds more strongly than <<
    assert_long(                   2, 1 + 16 >> 3,       "1 + 16 >> 3" ); // + binds more strongly than >>
    assert_long(                   0, 0x0,               "0x0"         );
    assert_long(                   1, 0x1,               "0x1"         );
    assert_long(                   9, 0x9,               "0x9"         );
    assert_long(                  10, 0xa,               "0xa"         );
    assert_long(                  15, 0xf,               "0xf"         );
    assert_long(                  16, 0x10,              "0x10"        );
    assert_long(                  64, 0x40,              "0x40"        );
    assert_long(                 255, 0xff,              "0xff"        );
    assert_long(                 256, 0x100,             "0x100"       );
}

int fca0() {
    assert_int(1, 1, "function call with 0 args");
}

int fca1(int i1) {
    assert_int(1, i1, "function call with 1 arg");
}

int fca2(int i1, int i2) {
    assert_int(12, 10 * i1 + i2, "function call with 2 args");
}

int fca3(int i1, int i2, int i3) {
    assert_int(123, 100 * i1 + 10 * i2 + i3, "function call with 3 args");
}

int fca4(int i1, int i2, int i3, int i4) {
    assert_int(1234, 1000 * i1 + 100 * i2 + 10 * i3 + i4, "function call with 4 args");
}

int fca5(int i1, int i2, int i3, int i4, int i5) {
    assert_int(12345, 10000 * i1 + 1000 * i2 + 100 * i3 + 10 * i4 + i5, "function call with 5 args");
}

int fca6(int i1, int i2, int i3, int i4, int i5, int i6) {
    assert_int(123456, 100000 * i1 + 10000 * i2 + 1000 * i3 + 100 * i4 + 10 * i5 + i6, "function call with 6 args");
}

int fca7(int i1, int i2, int i3, int i4, int i5, int i6, int i7) {
    assert_int(1234567, 1000000 * i1 + 100000 * i2 + 10000 * i3 + 1000 * i4 + 100 * i5 + 10 * i6 + i7, "function call with 7 args");
}

int fca8(int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8) {
    assert_int(12345678, 10000000 * i1 + 1000000 * i2 + 100000 * i3 + 10000 * i4 + 1000 * i5 + 100 * i6 + 10 * i7 + i8, "function call with 8 args");
}

void test_function_call_args() {
    fca0();
    fca1(1);
    fca2(1, 2);
    fca3(1, 2, 3);
    fca4(1, 2, 3, 4);
    fca5(1, 2, 3, 4, 5);
    fca6(1, 2, 3, 4, 5, 6);
    fca7(1, 2, 3, 4, 5, 6, 7);
    fca8(1, 2, 3, 4, 5, 6, 7, 8);
}

int nfc(int i) { return i + 1; }

void test_nested_func_calls() {
    assert_int(1, nfc(0),                      "nested function calls 1");
    assert_int(2, nfc(1),                      "nested function calls 2");
    assert_int(3, nfc(nfc(1)),                 "nested function calls 3");
    assert_int(4, nfc(nfc(nfc(1))),            "nested function calls 4");
    assert_int(5, nfc(nfc(1) + nfc(1)),        "nested function calls 5");
    assert_int(6, nfc(nfc(1) + nfc(1))+nfc(0), "nested function calls 6");
}

int get_g() {
    return g;
}

int fc1(int a) {
    return get_g() + a;
}

void test_function_call_with_global() {
    g = 2;
    assert_int(3, fc1(1), "function call with global assignment");
}

int splfoo();
int splbar(int i);

int splfoo() {
    return 1;
}

int splbar(int i) {
    return i;
}

void test_split_function_declaration_and_definition() {
    assert_int(1, splfoo(),  "split function declaration 1");
    assert_int(1, splbar(1), "split function declaration 2");
    assert_int(2, splbar(2), "split function declaration 3");
}

void vrf() {
    g = 1;
    return;
    g = 2;
}

void test_void_return() {
    g = 0;
    vrf();
    assert_int(g, 1, "void return");
}

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

void test_prefix_inc_dec() {
    int i;
    i = 0;
    assert_int(1, ++i, "prefix inc dec 1");
    assert_int(2, ++i, "prefix inc dec 2");
    assert_int(1, --i, "prefix inc dec 3");
    assert_int(0, --i, "prefix inc dec 4");
}

void test_postfix_inc_dec() {
    int i;
    i = 0;
    assert_int(0, i++, "postfix inc dec 1");
    assert_int(1, i++, "postfix inc dec 2");
    assert_int(2, i--, "postfix inc dec 3");
    assert_int(1, i--, "postfix inc dec 4");
}

void test_inc_dec_sizes() {
    char c;
    char *pc;
    char **ppc;
    short s;
    short *ps;
    short **pps;
    int i;
    int *pi;
    int **ppi;
    long l;
    long *pl;
    long **ppl;

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

    c++;   assert_int(1, c,   "++- size 1a");    c--; assert_int(0, c,   "++- size 1b"); ++c;   assert_int(1, c,   "++- size 1c");    c--; assert_int(0, c,   "++- size 1d");
    pc++;  assert_int(1, pc,  "++- size 2a");   pc--; assert_int(0, pc,  "++- size 2b"); ++pc;  assert_int(1, pc,  "++- size 2c");   pc--; assert_int(0, pc,  "++- size 2d");
    ppc++; assert_int(8, ppc, "++- size 3a");  ppc--; assert_int(0, ppc, "++- size 3b"); ++ppc; assert_int(8, ppc, "++- size 3c");  ppc--; assert_int(0, ppc, "++- size 3d");
    s++;   assert_int(1, s,   "++- size 4a");    s--; assert_int(0, s,   "++- size 4b"); ++s;   assert_int(1, s,   "++- size 4c");    s--; assert_int(0, s,   "++- size 4d");
    ps++;  assert_int(2, ps,  "++- size 5a");   ps--; assert_int(0, ps,  "++- size 5b"); ++ps;  assert_int(2, ps,  "++- size 5c");   ps--; assert_int(0, ps,  "++- size 5d");
    pps++; assert_int(8, pps, "++- size 6a");  pps--; assert_int(0, pps, "++- size 6b"); ++pps; assert_int(8, pps, "++- size 6c");  pps--; assert_int(0, pps, "++- size 6d");
    i++;   assert_int(1, i,   "++- size 7a");    i--; assert_int(0, i,   "++- size 7b"); ++i;   assert_int(1, i,   "++- size 7c");    i--; assert_int(0, i,   "++- size 7d");
    pi++;  assert_int(4, pi,  "++- size 8a");   pi--; assert_int(0, pi,  "++- size 8b"); ++pi;  assert_int(4, pi,  "++- size 8c");   pi--; assert_int(0, pi,  "++- size 8d");
    ppi++; assert_int(8, ppi, "++- size 9a");  ppi--; assert_int(0, ppi, "++- size 9b"); ++ppi; assert_int(8, ppi, "++- size 9c");  ppi--; assert_int(0, ppi, "++- size 9d");
    l++;   assert_int(1, l,   "++- size aa");    l--; assert_int(0, l,   "++- size ab"); ++l;   assert_int(1, l,   "++- size ac");    l--; assert_int(0, l,   "++- size ad");
    pl++;  assert_int(8, pl,  "++- size ba");   pl--; assert_int(0, pl,  "++- size bb"); ++pl;  assert_int(8, pl,  "++- size bc");   pl--; assert_int(0, pl,  "++- size bd");
    ppl++; assert_int(8, ppl, "++- size ca");  ppl--; assert_int(0, ppl, "++- size cb"); ++ppl; assert_int(8, ppl, "++- size cc");  ppl--; assert_int(0, ppl, "++- size cd");
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

// Test 64 bit constants are assigned properly
void test_long_constant() {
    long l;

    l = 0x1000000001010101;
    assert_int(1, l && 0x1000000000000000, "64 bit constants");
}

void test_integer_sizes() {
    int i;
    char *data;

    data = malloc(8);

    assert_int(1, sizeof(void),    "sizeof void");
    assert_int(1, sizeof(char),    "sizeof char");
    assert_int(2, sizeof(short),   "sizeof short");
    assert_int(4, sizeof(int),     "sizeof int");
    assert_int(8, sizeof(long),    "sizeof long");
    assert_int(8, sizeof(void *),  "sizeof void *");
    assert_int(8, sizeof(char *),  "sizeof char *");
    assert_int(8, sizeof(short *), "sizeof short *");
    assert_int(8, sizeof(int *),   "sizeof int *");
    assert_int(8, sizeof(long *),  "sizeof long *");
    assert_int(8, sizeof(int **),  "sizeof int **");
    assert_int(8, sizeof(char **), "sizeof char **");
    assert_int(8, sizeof(short **),"sizeof short **");
    assert_int(8, sizeof(int **),  "sizeof int **");
    assert_int(8, sizeof(long **), "sizeof long **");

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

void test_malloc() {
    int *pi;

    pi = malloc(32);
    *pi = 1;
    *++pi = 2;
    *++pi = -1; // Gets overwritten
    *pi++ = 3;
    *pi = 4;

    assert_int(1, *(pi - 3), "malloc 1");
    assert_int(2, *(pi - 2), "malloc 2");
    assert_int(3, *(pi - 1), "malloc 3");
    assert_int(4, *pi,       "malloc 4");
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

void test_pointer_with_non_constant_non_pointer_addition() {
    int *pi;
    int j;

    pi = malloc(sizeof(int));
    j = 0;
    *(pi + j) = 1;
    assert_int(1, *pi, "Pointer with non-constant non-pointer addition");
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

void test_while() {
    int i;
    int c1, c2;

    c1 = c2 = 0;
    i = 0; while (i < 3) { c1 = c1 * 10 + i; i++; }
    i = 0; while (i++ < 3) c2 = c2 * 10 + i;
    assert_int( 12, c1, "while 1");
    assert_int(123, c2, "while 2");
}

void test_for() {
    int i;
    int c1, c2;

    c1 = c2 = 0;

    for (i = 0; i < 10; i++) c1 = c1 * 10 + i;

    for (i = 0; i < 10; i++) {
        if (i == 5) continue;
        c2 = c2 * 10 + i;
    }

    assert_int(123456789, c1, "for 1");
    assert_int(12346789,  c2, "for 2");
}

void test_for_statement_combinations() {
    int i;
    int c;

    i = 0;        for(    ;        ;    ) { if (i == 10) break; i++; } assert_int(10, i, "for combinations 1");
    i = 0;        for(    ;        ; i++) { if (i == 10) break; }      assert_int(10, i, "for combinations 2");
    i = 0;        for(    ;  i<10  ;    ) i++;                         assert_int(10, i, "for combinations 3");
    i = 0; c = 0; for(    ;  i<10  ; i++) c++;                         assert_int(10, i, "for combinations 4");
                  for(i=0 ;        ;    ) { if (i == 10) break; i++; } assert_int(10, i, "for combinations 5");
                  for(i=0 ;        ; i++) { if (i == 10) break; }      assert_int(10, i, "for combinations 6");
                  for(i=0 ; i<10   ;    ) i++;                         assert_int(10, i, "for combinations 7");
           c = 0; for(i=0 ; i<10   ; i++) c++;                         assert_int(10, i, "for combinations 8");

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

void test_while_continue() {
    int i, c1;

    c1 = i = 0;
    while (i++ < 5) {
        c1 = c1 * 10 + i;
        continue;
        c1 = c1 * 10 + 9;
    }
    assert_int(12345, c1, "while continue");
}

void test_if_else() {
    int i, t1, t2, t3, t4;

    i = t1 = t2 = t3 = t4 = 0;
    if (i == 0) t1 = 1;
    if (i != 0) t2 = 1;

    i = 1;
    if (i == 0) t3 = 1;
    if (i != 0) t4 = 1;

    assert_int(1, t1, "if/else 1");
    assert_int(0, t2, "if/else 2");
    assert_int(0, t3, "if/else 3");
    assert_int(1, t4, "if/else 4");
}

void test_and_or_shortcutting() {
    int i, t1, t2, t3, t4;

    i = t1 = t2 = t3 = t4 = 0;
    i == 0 || t1++;
    i == 1 || t2++;
    i == 0 && t3++;
    i == 1 && t4++;

    assert_int(0, t1, "test && || shortcutting 1");
    assert_int(1, t2, "test && || shortcutting 2");
    assert_int(1, t3, "test && || shortcutting 3");
    assert_int(0, t4, "test && || shortcutting 4");
}

void test_ternary() {
    int i;

    assert_int(1, (1 ? 1 : 2),          "ternary 1");
    assert_int(2, (0 ? 1 : 2),          "ternary 2");
    assert_int(6,  0 + 2 * (1 ? 3 : 4), "ternary 3");
    assert_int(0, strcmp("foo", 1 ? "foo" : "bar"), "ternary 4");
    assert_int(0, strcmp("bar", 0 ? "foo" : "bar"), "ternary 5");

}

int ternary_cast() {
    return 2;
}

void test_ternary_cast() {
    int b;
    long l;

    b = 0; l = b ? 1 : ternary_cast(); assert_int(2, l, "ternary cast 1");
    b = 1; l = b ? 1 : ternary_cast(); assert_int(1, l, "ternary cast 2");
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

void test_casting() {
    int *pi;
    int *pj;
    pi = 0;
    pj = pi + 1;
    assert_int(4, (long) pj - (long) pi, "casting 1");
    pj = (int *) (((char *) pi) + 1);
    assert_int(1, (long) pj - (long) pi, "casting 2");
}

void test_local_comma_var_declarations() {
    int i, *pi;
    i = 1;
    pi = &i;
    assert_int(1, *pi, "comma var declaration 1");
    *pi = 2;
    assert_int(2, *pi, "comma var declaration 2");
}

void test_global_comma_var_declarations() {
    gcvi = 1;
    gcvj = 2;
    assert_int(1, gcvi, "global comma var declaration 1");
    assert_int(2, gcvj, "global comma var declaration 2");
}

void test_free() {
    int *pi;
    pi = malloc(17);
    free(pi);
}

void test_mem_functions() {
    long *pi1, *pi2;

    pi1 = malloc(32);
    pi2 = malloc(32);
    memset(pi1, 0, 32);
    assert_int(0, pi1[0], "memory functions 1");
    assert_int(0, pi1[3], "memory functions 2");
    memset(pi1, -1, 32);
    assert_int(-1, pi1[0], "memory functions 2");
    assert_int(-1, pi1[3], "memory functions 3");

    assert_int(255, memcmp(pi1, pi2, 32), "memory functions 4");

    // gcc's strcmp is builtin and returns different numbers than the
    // std library's. Only the sign is the same. Hence, we have to use <
    // and > instead of ==.
    assert_int(1, strcmp("foo", "foo") == 0, "memory functions 5");
    assert_int(1, strcmp("foo", "aaa")  > 0, "memory functions 6");
    assert_int(1, strcmp("foo", "ggg")  < 0, "memory functions 7");
}

void test_open_read_write_close() {
    void *f;
    int i;
    char *data;

    f = fopen("/tmp/write-test", "w");
    assert_int(1, f > 0, "open file for writing");
    i = fwrite("foo\n", 1, 4, f);
    assert_int(4, i, "write 4 bytes");
    fclose(f);

    data = malloc(16);
    memset(data, 0, 16);
    f = fopen("/tmp/write-test", "r");
    assert_int(1, f > 0, "open file for reading");
    assert_int(4, fread(data, 1, 16, f), "read 4 bytes");
    fclose(f);

    assert_int(0, memcmp(data, "foo", 3), "read/write bytes match");
}

void test_assign_operation() {
    char c, *pc;
    short s, *ps;
    int i, *pi;
    long l, *pl;

    c = 0; pc = 0; s = 0; ps = 0; i = 0; pi = 0; l = 0; pl = 0;
    c += 2; pc += 2;
    s += 2; ps += 2;
    i += 2; pi += 2;
    l += 2; pl += 2;

    assert_int(2,  c,  "char += 2");
    assert_int(2,  pc, "*char += 2");
    assert_int(2,  s,  "short += 2");
    assert_int(4,  ps, "*short += 2");
    assert_int(2,  i,  "int += 2");
    assert_int(8,  pi, "*int += 2");
    assert_int(2,  l,  "long += 2");
    assert_int(16, pl, "*long += 2");
    pc -= 3; ps -= 3; pi -= 3; pl -= 3;

    assert_int( 2,  c,  "char -= 2");
    assert_int(-1,  pc, "*char -= 2");
    assert_int( 2,  s,  "short -= 2");
    assert_int(-2,  ps, "*short -= 2");
    assert_int( 2,  i,  "int -= 2");
    assert_int(-4,  pi, "*int -= 2");
    assert_int( 2,  l,  "long -= 2");
    assert_int(-8,  pl, "*long -= 2");
}

// Test precedence error combined with expression parser not stopping at unknown tokens
void test_cast_in_function_call() {
    char *pi;
    pi = "foo";
    assert_int(0, strcmp((char *) pi, "foo"), "cast in function call");
}

void test_array_lookup_of_string_literal() {
    assert_string("foobar", &"foobar"[0], "array lookup of string literal 1");
    assert_string("bar",    &"foobar"[3], "array lookup of string literal 2");
}

void test_nested_while_continue() {
    int i, c1;
    char *s;

    i = c1 = 0;
    s = "foo";

    while (s[i++] != 0) {
        c1 = c1 * 10 + i;
        while (0);
        continue;
    }

    assert_int(123, c1, "nested while/continue");
}

int fr_lvalue() {
    int t;
    t = 1;
    return t;
}

void test_func_returns_are_lvalues() {
    int i;
    i = fr_lvalue();
    assert_int(1, i, "function returns are lvalues");
}

void test_bad_or_and_stack_consumption() {
    long ip;
    ip = 0;
    while ((1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1 || 1) && ip < 2) {
        ip += 1;
    }
}

void test_double_deref_assign_with_cast() {
    // This test is a bit bonkers.
    // stack contains longs, but they are really pointers to longs.
    long i, a, *sp, *stack;

    stack = malloc(32);
    sp = stack;
    i = 10;
    a = 20;

    // Assign a pointer to a long (i) to sp
    *sp = &i;

    // With explicit parentheses:
    // (*((long *) *sp))++ = a;
    //
    // 1. Dereference sp (which is a long, but is equal to &i)
    // 2. Cast it to a pointer to long (long *)
    // 3. Assign to the long*, which is &i, thus modifying the value of i
    // 4. Increment sp
    *(long *) *sp++ = a;

    assert_int(20, i, "double deref assign with a cast");
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

void test_double_assign() {
    long a, b;
    a = b = 1;
    assert_int(1, a, "double assign 1");
    assert_int(1, b, "double assign 2");
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

void test_first_arg_to_or_and_and_must_be_rvalue() {
    long *sp;
    long a;
    sp = malloc(32);
    *sp = 0;
    a = 1;
    a = *(sp++) || a;
    assert_int(1, a, "first arg to || must be an lvalue");
}

void test_enum() {
    assert_int(0,  A, "enum 1");
    assert_int(1,  B, "enum 2");
    assert_int(2,  C, "enum 3");
    assert_int(3,  D, "enum 4");
    assert_int(-2, E, "enum 5");
    assert_int(-1, F, "enum 6");
}

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
    struct cc *vc;
    struct cs *vs;
    struct ci *vi;
    struct cl *vl;

    assert_int( 1, sizeof(struct sc1), "struct member alignment c1");
    assert_int( 2, sizeof(struct sc2), "struct member alignment c2");
    assert_int( 2, sizeof(struct ss1), "struct member alignment s1");
    assert_int( 4, sizeof(struct ss2), "struct member alignment s2");
    assert_int( 4, sizeof(struct si1), "struct member alignment i1");
    assert_int( 8, sizeof(struct si2), "struct member alignment i2");
    assert_int( 8, sizeof(struct sl1), "struct member alignment l1");
    assert_int(16, sizeof(struct sl2), "struct member alignment l2");

    assert_int( 2, sizeof(struct cc),  "struct member alignment c1");
    assert_int( 4, sizeof(struct cs),  "struct member alignment s1");
    assert_int( 8, sizeof(struct ci),  "struct member alignment i1");
    assert_int(16, sizeof(struct cl),  "struct member alignment l1");
    assert_int( 3, sizeof(struct ccc), "struct member alignment c2");
    assert_int( 6, sizeof(struct csc), "struct member alignment s2");
    assert_int(12, sizeof(struct cic), "struct member alignment i2");
    assert_int(24, sizeof(struct clc), "struct member alignment l2");

    vc = 0;
    vs = 0;
    vi = 0;
    vl = 0;

    assert_int(1, (long) &(vc->c2) - (long) &(vc->c1), "struct member alignment c");
    assert_int(2, (long) &(vs->s1) - (long) &(vs->c1), "struct member alignment s");
    assert_int(4, (long) &(vi->i1) - (long) &(vi->c1), "struct member alignment i");
    assert_int(8, (long) &(vl->l1) - (long) &(vl->c1), "struct member alignment l");
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

void test_struct_additions() {
    struct sas *s;

    s = 0;
    s++;

    assert_int(16, s,     "struct additions 1");
    assert_int(32, s + 1, "struct additions 2");
    assert_int(16, s++,   "struct additions 3");
    assert_int(48, ++s,   "struct additions 4");
}

void test_struct_casting() {
    int i;
    struct scs *s;

    i = 1;
    s = (struct scs*) &i;
    assert_int(1, s->i, "struct casting");
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

void test_typedef1() {
    S1 *s;

    s = malloc(sizeof(S1));
    s->i = 1;
    assert_int(1, s->i, "Struct typedef 1");
}

void test_typedef2() {
    S2 *s;

    s = malloc(sizeof(S2));
    s->i = 1;
    assert_int(1, s->i, "Struct typedef 2");
}

void test_unary_precedence() {
    struct ups *s;
    s = malloc(sizeof(struct ups));
    s->i = 0;
    assert_int( 1, !s[0].i,                       "unary precedence !");
    assert_int(-1, ~s[0].i,                       "unary precedence ~");
    assert_int( 0, -s[0].i,                       "unary precedence -");
    assert_int( 4, (long) &s[0].j - (long) &s[0], "unary precedence");
}

// Create an expression which undoubtedly will exhaust all registers, forcing
// the spilling code into action
void test_spilling_stress() {
    int i;
    i = 1;
    i = (i + 2) * (i + (i + 2) * (i + (i + 2) * (i + i + (i + 2) * (i + 3))));
    assert_int(390, i, "spilling stress");
}

int csr() {
    return 2;
}

// This test is a bit fragile. It does the job right now, but register
// candidates will likely change. Currently, foo() uses rbx, which is also
// used to store the lvalue for s->i. If rbx wasn't preserved, this code
// would segfault.
void test_callee_saved_registers() {
    struct csrs *s;

    s = malloc(sizeof(struct csrs));
    s->i = csr();
    assert_int(2, s->i, "callee saved registers");
}

int set_rax(int i) {
    return i;
}

// Variadic functions have the number of floating point arguments passed in al.
// Since floating point numbers isn't implemented, this should be set to zero.
// A bug was present where this wasn't being set in sprintf. This code below
// would segfault at the second sprintf.
void test_variadic_arg_bug() {
    char *s;

    s = malloc(10);

    set_rax(0);
    sprintf(s, "1\n");

    set_rax(1);
    sprintf(s, "2\n");
}

void test_backwards_jumps() {
    int i, j, k;
    int *r;

    r = malloc(sizeof(int) * 3);
    i = 0;
    j = 0;
    for (i = 0; i < 3; i++) {
        j += 10;
        r[i] = j;
        k = j + 1; // Trigger register reuse in the buggy case
    }

    assert_int(10, r[0], "backwards jumps liveness 1");
    assert_int(20, r[1], "backwards jumps liveness 2");
    assert_int(30, r[2], "backwards jumps liveness 3");
}

void test_first_declaration_in_if_in_for_liveness() {
    int i, j, k, l;

    i = 0;
    for (i = 0; i < 3; i++) {
        if (i == 0) j = 1; else k = j * 2;
        l = l + (l + (l + (l + (l + (l + (l + (l + (l + (l + (l + (l + (l + 1))))))))))));
    }

    assert_int(2, k, "liveness extension for conditional declaration inside loop");
}

void test_spilling_locals_to_stack_bug() {
    int i, j, k;
    int r1, r2, r3, r4, r5, r6, r7, r8, r9, r10;
    int q1, q2, q3, q4, q5, q6, q7, q8, q9, q10;

    r1 = r2 = r3 = r4 = r5 = r6 = r7 = r8 = r9 = r10 = 1;
    assert_int(1, r1,  "Local stack spilling clobber bug 1");
    assert_int(1, r2,  "Local stack spilling clobber bug 2");
    assert_int(1, r3,  "Local stack spilling clobber bug 3");
    assert_int(1, r4,  "Local stack spilling clobber bug 4");
    assert_int(1, r5,  "Local stack spilling clobber bug 5");
    assert_int(1, r6,  "Local stack spilling clobber bug 6");
    assert_int(1, r7,  "Local stack spilling clobber bug 7");
    assert_int(1, r8,  "Local stack spilling clobber bug 8");
    assert_int(1, r9,  "Local stack spilling clobber bug 9");
    assert_int(1, r10, "Local stack spilling clobber bug 10");

    r10++; // This forces a spill of r10
}

int sa0()                                                               { assert_int(0, check_stack_alignment(), "SA 0"); }
int sa1(int i1)                                                         { assert_int(0, check_stack_alignment(), "SA 1"); }
int sa2(int i1, int i2)                                                 { assert_int(0, check_stack_alignment(), "SA 2"); }
int sa3(int i1, int i2, int i3)                                         { assert_int(0, check_stack_alignment(), "SA 3"); }
int sa4(int i1, int i2, int i3, int i4)                                 { assert_int(0, check_stack_alignment(), "SA 4"); }
int sa5(int i1, int i2, int i3, int i4, int i5)                         { assert_int(0, check_stack_alignment(), "SA 5"); }
int sa6(int i1, int i2, int i3, int i4, int i5, int i6)                 { assert_int(0, check_stack_alignment(), "SA 6"); }
int sa7(int i1, int i2, int i3, int i4, int i5, int i6, int i7)         { assert_int(0, check_stack_alignment(), "SA 7"); }
int sa8(int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8) { assert_int(0, check_stack_alignment(), "SA 8"); }

int test_function_call_stack_alignment() {
    sa0();
    sa1(1);
    sa2(1, 1);
    sa3(1, 1, 1);
    sa4(1, 1, 1, 1);
    sa4(1, 1, 1, 1);
    sa5(1, 1, 1, 1, 1);
    sa6(1, 1, 1, 1, 1, 1);
    sa7(1, 1, 1, 1, 1, 1, 1);
    sa8(1, 1, 1, 1, 1, 1, 1, 1);
}

void test_local_var_stack_alignment0() {           assert_int(0, check_stack_alignment(), "Stack alignment 0"); }
void test_local_var_stack_alignment1() { int i;    assert_int(0, check_stack_alignment(), "Stack alignment 1"); }
void test_local_var_stack_alignment2() { int i, j; assert_int(0, check_stack_alignment(), "Stack alignment 2"); }

void test_local_var_stack_alignment() {
    test_local_var_stack_alignment0();
    test_local_var_stack_alignment1();
    test_local_var_stack_alignment2();
}

void rssa1(int nt) {
    char *buf;

    buf = malloc(6);

    if (nt == 100) return;
    asprintf(&buf, "nt%03d", nt);
    assert_string("nt001", buf, "Return statement stack alignment 1");
}

void rssa2(int nt) {
    char *buf;

    buf = malloc(6);

    if (nt == 100) return;
    if (nt == 101) return;

    asprintf(&buf, "nt%03d", nt);
    assert_string("nt001", buf, "Return statement stack alignment 1");
}

void test_return_statement_stack_alignment() {
    // Test but where return statements would lead to an invalid push count
    rssa1(1);
    rssa2(1);
}

void test_ssa_label_merge_bug() {
    int i;

         if (0) i = 1;
    else if (1) i = 2;
    else if (0) i = 3;

    assert_int(2, i, "Label merge bug in three level if/else");
}

void tsmab(int *i) {}

void test_ssa_memory_alocation_bug() {
    int a, b;
    int i;

    i = 0;
    tsmab(&i);
}

int test_ssa_continue_with_statements_afterwards() {
    // A silly example, but a bug was leading to incorrect liveness, leading to
    // in correct register allocations.
    int i;

    i = 0;
    while (i++ < 3) {
        continue;
        i = 1;
    }
}

int test_ssa_break_with_statements_afterwards() {
    // A silly example, but a bug was leading to incorrect liveness, leading to
    // in correct register allocations.
    int i;

    i = 0;
    while (i++ < 3) {
        continue;
        i = 1;
    }
}

void test_ssa_arithmetic_optimizations() {
    int i;

    i = 10;

    assert_int( 0, i * 0, "Arithetic optimization i * 0");
    assert_int(10, i * 1, "Arithetic optimization i * 1");
    assert_int(20, i * 2, "Arithetic optimization i * 2");
    assert_int(30, i * 3, "Arithetic optimization i * 3");
    assert_int(40, i * 4, "Arithetic optimization i * 4");
    assert_int(50, i * 5, "Arithetic optimization i * 5");
    assert_int(60, i * 6, "Arithetic optimization i * 6");
    assert_int(70, i * 7, "Arithetic optimization i * 7");
    assert_int(80, i * 8, "Arithetic optimization i * 8");
}

int main(int argc, char **argv) {
    int help;

    help = verbose = 0;

    argc--;
    argv++;
    while (argc > 0 && *argv[0] == '-') {
             if (argc > 0 && !memcmp(argv[0], "-h",   3)) { help = 0;    argc--; argv++; }
        else if (argc > 0 && !memcmp(argv[0], "-v",   2)) { verbose = 1; argc--; argv++; }
        else {
            printf("Unknown parameter %s\n", argv[0]);
            exit(1);
        }
    }

    if (help) {
        printf("Usage: test-wc4 [-v]\n\n");
        printf("Flags\n");
        printf("-v      Verbose mode; show all tests\n");
        printf("-h      Help\n");
        exit(1);
    }

    passes = 0;
    failures = 0;

    test_expr();
    test_function_call_args();
    test_nested_func_calls();
    test_function_call_with_global();
    test_split_function_declaration_and_definition();
    test_void_return();
    test_pointer_to_int1();
    test_pointer_to_int2();
    test_pointer_to_int3();
    test_pointer_to_int4();
    test_pointer_to_int5();
    test_pointer_to_int6();
    test_pointer_to_char();
    test_prefix_inc_dec();
    test_postfix_inc_dec();
    test_inc_dec_sizes();
    test_dereferenced_pointer_inc_dec();
    test_pointer_arithmetic();
    test_long_constant();
    test_integer_sizes();
    test_malloc();
    test_char_pointer_arithmetic();
    test_pointer_pointer_subtraction1();
    test_pointer_pointer_subtraction2();
    test_pointer_with_non_constant_non_pointer_addition();
    test_assignment_of_pointer_to_array();
    test_while();
    test_for();
    test_for_statement_combinations();
    test_string_copy();
    test_while_continue();
    test_if_else();
    test_and_or_shortcutting();
    test_ternary();
    test_ternary_cast();
    test_bracket_lookup();
    test_casting();
    test_local_comma_var_declarations();
    test_global_comma_var_declarations();
    test_free();
    test_mem_functions();
    test_open_read_write_close();
    test_assign_operation();
    test_cast_in_function_call();
    test_array_lookup_of_string_literal();
    test_nested_while_continue();
    test_func_returns_are_lvalues();
    test_bad_or_and_stack_consumption();
    test_double_deref_assign_with_cast();
    test_double_deref_precision1();
    test_double_deref_precision2();
    test_double_assign();
    test_int_char_interbreeding();
    test_first_arg_to_or_and_and_must_be_rvalue();
    test_enum();
    test_simple_struct();
    test_struct_member_alignment();
    test_struct_indirect_sizes();
    test_struct_alignment_bug();
    test_nested_struct();
    test_struct_double_indirect_assign_to_global();
    test_function_returning_a_pointer_to_a_struct();
    test_function_with_a_pointer_to_a_struct_argument();
    test_struct_additions();
    test_struct_casting();
    test_packed_struct();
    test_incomplete_struct();
    test_double_dereference1();
    test_double_dereference2();
    test_struct_member_array_lookup();
    test_typedef1();
    test_typedef2();
    test_unary_precedence();
    test_spilling_stress();
    test_callee_saved_registers();
    test_variadic_arg_bug();
    test_backwards_jumps();
    test_first_declaration_in_if_in_for_liveness();
    test_spilling_locals_to_stack_bug();
    test_local_var_stack_alignment();
    test_function_call_stack_alignment();
    test_return_statement_stack_alignment();
    test_ssa_label_merge_bug();
    test_ssa_memory_alocation_bug();
    test_ssa_continue_with_statements_afterwards();
    test_ssa_break_with_statements_afterwards();
    test_ssa_arithmetic_optimizations();

    finalize();
}
