#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

int check_stack_alignment();

int verbose;
int passes;
int failures;

int gcvi, gcvj;
unsigned int gcvui, gcvuj;

char gc, *gpc;
short gs, *gps;
int gi, *gpi;
long gl, *gpl;

// Test 64 bit constants are assigned properly
void test_long_constant() {
    long l;

    l = 0x1000000001010101ul;
    assert_int(1, l && 0x1000000000000000ul, "64 bit constants");
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
    assert_long(                   0, 0X0,               "0X0"         ); // Capital x
    assert_long(                   1, 0x1,               "0x1"         );
    assert_long(                   9, 0x9,               "0x9"         );
    assert_long(                  10, 0xa,               "0xa"         );
    assert_long(                  15, 0xf,               "0xf"         );
    assert_long(                  16, 0x10,              "0x10"        );
    assert_long(                  64, 0x40,              "0x40"        );
    assert_long(                 255, 0xff,              "0xff"        );
    assert_long(                 256, 0x100,             "0x100"       );
    assert_long(                 256, 0x100,             "0x100"       );
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

    gcvui = 1;
    gcvuj = 2;
    assert_int(1, gcvui, "global comma var declaration 3");
    assert_int(2, gcvuj, "global comma var declaration 4");
}

void test_double_assign() {
    long a, b;
    a = b = 1;
    assert_int(1, a, "double assign 1");
    assert_int(1, b, "double assign 2");
}

void test_assign_operations() {
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

static void test_assign_to_globals() {
    char c, *pc;
    short s, *ps;
    int i, *pi;
    long l, *pl;

    c = s = i = l = 1;

    c++; gc = c; assert_long(gc, c, "gc = c");
    c++; gs = c; assert_long(gs, c, "gs = c");
    c++; gi = c; assert_long(gi, c, "gi = c");
    c++; gl = c; assert_long(gl, c, "gl = c");

    s++; gc = s; assert_long(gc, s, "gc = s");
    s++; gs = s; assert_long(gs, s, "gs = s");
    s++; gi = s; assert_long(gi, s, "gi = s");
    s++; gl = s; assert_long(gl, s, "gl = s");

    i++; gc = i; assert_long(gc, i, "gc = i");
    i++; gs = i; assert_long(gs, i, "gs = i");
    i++; gi = i; assert_long(gi, i, "gi = i");
    i++; gl = i; assert_long(gl, i, "gl = i");

    l++; gc = l; assert_long(gc, l, "gc = l");
    l++; gs = l; assert_long(gs, l, "gs = l");
    l++; gi = l; assert_long(gi, l, "gi = l");
    l++; gl = l; assert_long(gl, l, "gl = l");

    pc = ps = pi = pl = 1;

    pc++; gpc = pc; assert_long(gpc, pc, "pgc = pc");
    pc++; gps = pc; assert_long(gps, pc, "pgs = pc");
    pc++; gpi = pc; assert_long(gpi, pc, "pgi = pc");
    pc++; gpl = pc; assert_long(gpl, pc, "pgl = pc");

    ps++; gpc = ps; assert_long(gpc, ps, "pgc = ps");
    ps++; gps = ps; assert_long(gps, ps, "pgs = ps");
    ps++; gpi = ps; assert_long(gpi, ps, "pgi = ps");
    ps++; gpl = ps; assert_long(gpl, ps, "pgl = ps");

    pi++; gpc = pi; assert_long(gpc, pi, "pgc = pi");
    pi++; gps = pi; assert_long(gps, pi, "pgs = pi");
    pi++; gpi = pi; assert_long(gpi, pi, "pgi = pi");
    pi++; gpl = pi; assert_long(gpl, pi, "pgl = pi");

    pl++; gpc = pl; assert_long(gpc, pl, "pgc = pl");
    pl++; gps = pl; assert_long(gps, pl, "pgs = pl");
    pl++; gpi = pl; assert_long(gpi, pl, "pgi = pl");
    pl++; gpl = pl; assert_long(gpl, pl, "pgl = pl");
}

static void test_add_operation_sign() {
    int si1, si2;
    unsigned int ui1, ui2;

    si1 = -1;
    si2 = -2;
    ui1 = -1;
    ui2 = -2;

    // The add operation should return a signed value
    assert_int(1, (si1 + si2) < 0,          "adding two signed ints 1");
    assert_int(0, (si1 + si2) > 2147483647, "adding two signed ints 2");

    // The add operation should return an unsigned value
    assert_int(0, (ui1 + ui2) < 0,          "adding two unsigned ints 1");
    assert_int(1, (ui1 + ui2) > 2147483647, "adding two unsigned ints 2");

    // The add operation should return an unsigned value
    assert_int(0, (ui1 + si2) < 0,          "adding a signed and unsigned int 1");
    assert_int(1, (ui1 + si2) > 2147483647, "adding a signed and unsigned int 2");
}

static void test_logical_or_operation_sign() {
    int si1, si2;
    unsigned int ui1, ui2;

    si1 = -1;
    si2 = -2;
    ui1 = -1;
    ui2 = -2;

    // The logical or operation should return a signed value
    assert_int(1, (si1 | si2) < 0,          "logical orring signed ints 1");
    assert_int(0, (si1 | si2) > 2147483647, "logical orring signed ints 2");

    // The logical or operation should return an unsigned value
    assert_int(0, (ui1 | ui2) < 0,          "logical orring unsigned ints 1");
    assert_int(1, (ui1 | ui2) > 2147483647, "logical orring unsigned ints 2");

    // The logical or operation should return an unsigned value
    assert_int(0, (ui1 | si2) < 0,          "logical orring a signed and unsigned int 1");
    assert_int(1, (ui1 | si2) > 2147483647, "logical orring a signed and unsigned int 2");
}

void test_integer_sizes() {
    assert_int(1, sizeof(void),         "sizeof void");
    assert_int(1, sizeof(char),         "sizeof char");
    assert_int(2, sizeof(short),        "sizeof short");
    assert_int(2, sizeof(short int),    "sizeof short int");
    assert_int(4, sizeof(int),          "sizeof int");
    assert_int(8, sizeof(long),         "sizeof long");
    assert_int(8, sizeof(long int),     "sizeof long int");
    assert_int(8, sizeof(long long),    "sizeof long long");
    assert_int(8, sizeof(long long int),"sizeof long long int");
    assert_int(8, sizeof(void *),       "sizeof void *");
    assert_int(8, sizeof(char *),       "sizeof char *");
    assert_int(8, sizeof(short *),      "sizeof short *");
    assert_int(8, sizeof(int *),        "sizeof int *");
    assert_int(8, sizeof(long *),       "sizeof long *");
    assert_int(8, sizeof(int **),       "sizeof int **");
    assert_int(8, sizeof(char **),      "sizeof char **");
    assert_int(8, sizeof(short **),     "sizeof short **");
    assert_int(8, sizeof(int **),       "sizeof int **");
    assert_int(8, sizeof(long **),      "sizeof long **");

    assert_int(1, sizeof(signed char),         "sizeof signed char");          assert_int(1, sizeof(unsigned char),         "sizeof unsigned char");
    assert_int(2, sizeof(signed short),        "sizeof signed short");         assert_int(2, sizeof(unsigned short),        "sizeof unsigned short");
    assert_int(2, sizeof(signed short int),    "sizeof signed short int");     assert_int(2, sizeof(unsigned short int),    "sizeof unsigned short int");
    assert_int(4, sizeof(signed),              "sizeof signed");               assert_int(4, sizeof(unsigned),              "sizeof unsigned");
    assert_int(4, sizeof(signed int),          "sizeof signed int");           assert_int(4, sizeof(unsigned int),          "sizeof unsigned int");
    assert_int(8, sizeof(signed long),         "sizeof signed long");          assert_int(8, sizeof(unsigned long),         "sizeof unsigned long");
    assert_int(8, sizeof(signed long int),     "sizeof signed long int");      assert_int(8, sizeof(unsigned long int),     "sizeof unsigned long int");
    assert_int(8, sizeof(signed long long),    "sizeof signed long long");     assert_int(8, sizeof(unsigned long long),    "sizeof unsigned long long");
    assert_int(8, sizeof(signed long long int),"sizeof signed long long int"); assert_int(8, sizeof(unsigned long long int),"sizeof unsigned long long int");
    assert_int(8, sizeof(signed char *),       "sizeof signed char *");        assert_int(8, sizeof(unsigned char *),       "sizeof unsigned char *");
    assert_int(8, sizeof(signed short *),      "sizeof signed short *");       assert_int(8, sizeof(unsigned short *),      "sizeof unsigned short *");
    assert_int(8, sizeof(signed int *),        "sizeof signed int *");         assert_int(8, sizeof(unsigned int *),        "sizeof unsigned int *");
    assert_int(8, sizeof(signed long *),       "sizeof signed long *");        assert_int(8, sizeof(unsigned long *),       "sizeof unsigned long *");
    assert_int(8, sizeof(signed int **),       "sizeof signed int **");        assert_int(8, sizeof(unsigned int **),       "sizeof unsigned int **");
    assert_int(8, sizeof(signed char **),      "sizeof signed char **");       assert_int(8, sizeof(unsigned char **),      "sizeof unsigned char **");
    assert_int(8, sizeof(signed short **),     "sizeof signed short **");      assert_int(8, sizeof(unsigned short **),     "sizeof unsigned short **");
    assert_int(8, sizeof(signed int **),       "sizeof signed int **");        assert_int(8, sizeof(unsigned int **),       "sizeof unsigned int **");
    assert_int(8, sizeof(signed long **),      "sizeof signed long **");       assert_int(8, sizeof(unsigned long **),      "sizeof unsigned long **");
}

void test_sizeof_expr() {
    char c1, c2;
    short s1, s2;
    int i1, i2;
    long l1, l2;

    assert_int(4, sizeof(c1 + c2), "sizeof c+c");
    assert_int(4, sizeof(c1 + s2), "sizeof c+s");
    assert_int(4, sizeof(c1 + i2), "sizeof c+i");
    assert_int(8, sizeof(c1 + l2), "sizeof c+l");
    assert_int(4, sizeof(s1 + s2), "sizeof s+s");
    assert_int(4, sizeof(s1 + i2), "sizeof s+i");
    assert_int(8, sizeof(s1 + l2), "sizeof s+l");
    assert_int(4, sizeof(i1 + i2), "sizeof i+i");
    assert_int(8, sizeof(l1 + l2), "sizeof l+l");

    assert_int(4, sizeof(c1 << c2), "sizeof c << c");
    assert_int(4, sizeof(c1 << s1), "sizeof c << s");
    assert_int(4, sizeof(c1 << i1), "sizeof c << i");
    assert_int(4, sizeof(c1 << l1), "sizeof c << l");
    assert_int(4, sizeof(s1 << c1), "sizeof s << c");
    assert_int(4, sizeof(i1 << c1), "sizeof i << c");
    assert_int(8, sizeof(l1 << c1), "sizeof l << c");
}

void test_conditional_jumps() {
    int i1, i2;
    unsigned int ui1, ui2;
    int r;

    // signed
    i1 = 1; i2 = 1; if (i1 == i2) r = 1; else r = 0; assert_int(1, r, "i1 == i2");
    i1 = 1; i2 = 2; if (i1 == i2) r = 1; else r = 0; assert_int(0, r, "i1 == i2");
    i1 = 1; i2 = 1; if (i1 != i2) r = 1; else r = 0; assert_int(0, r, "i1 != i2");
    i1 = 1; i2 = 2; if (i1 != i2) r = 1; else r = 0; assert_int(1, r, "i1 != i2");

    i1 = -1; i2 = 0; if (i1 <  i2) r = 1; else r = 0; assert_int(1, r, "i1 < i2");
    i1 =  1; i2 = 0; if (i1 <  i2) r = 1; else r = 0; assert_int(0, r, "i1 < i2");
    i1 =  1; i2 = 1; if (i1 <  i2) r = 1; else r = 0; assert_int(0, r, "i1 < i2");
    i1 =  1; i2 = 2; if (i1 <  i2) r = 1; else r = 0; assert_int(1, r, "i1 < i2");

    i1 = 0; i2 = -1; if (i1 >  i2) r = 1; else r = 0; assert_int(1, r, "i1 > i2");
    i1 = 0; i2 =  1; if (i1 >  i2) r = 1; else r = 0; assert_int(0, r, "i1 > i2");
    i1 = 1; i2 =  1; if (i1 >  i2) r = 1; else r = 0; assert_int(0, r, "i1 > i2");
    i1 = 2; i2 =  1; if (i1 >  i2) r = 1; else r = 0; assert_int(1, r, "i1 > i2");

    i1 = -1; i2 = 0; if (i1 <= i2) r = 1; else r = 0; assert_int(1, r, "i1 <= i2");
    i1 =  1; i2 = 0; if (i1 <= i2) r = 1; else r = 0; assert_int(0, r, "i1 <= i2");
    i1 =  1; i2 = 1; if (i1 <= i2) r = 1; else r = 0; assert_int(1, r, "i1 <= i2");
    i1 =  1; i2 = 2; if (i1 <= i2) r = 1; else r = 0; assert_int(1, r, "i1 <= i2");

    i1 = 0; i2 = -1; if (i1 >= i2) r = 1; else r = 0; assert_int(1, r, "i1 >= i2");
    i1 = 0; i2 =  1; if (i1 >= i2) r = 1; else r = 0; assert_int(0, r, "i1 >= i2");
    i1 = 1; i2 =  1; if (i1 >= i2) r = 1; else r = 0; assert_int(1, r, "i1 >= i2");
    i1 = 2; i2 =  1; if (i1 >= i2) r = 1; else r = 0; assert_int(1, r, "i1 >= i2");

    /// unsigned
    ui1 = 1; ui2 = 1; if (ui1 == ui2) r = 1; else r = 0; assert_int(1, r, "ui1 == ui2");
    ui1 = 1; ui2 = 2; if (ui1 == ui2) r = 1; else r = 0; assert_int(0, r, "ui1 == ui2");
    ui1 = 1; ui2 = 1; if (ui1 != ui2) r = 1; else r = 0; assert_int(0, r, "ui1 != ui2");
    ui1 = 1; ui2 = 2; if (ui1 != ui2) r = 1; else r = 0; assert_int(1, r, "ui1 != ui2");

    ui1 = -1; ui2 = 0; if (ui1 <  ui2) r = 1; else r = 0; assert_int(0, r, "ui1 < ui2");
    ui1 =  1; ui2 = 0; if (ui1 <  ui2) r = 1; else r = 0; assert_int(0, r, "ui1 < ui2");
    ui1 =  1; ui2 = 1; if (ui1 <  ui2) r = 1; else r = 0; assert_int(0, r, "ui1 < ui2");
    ui1 =  1; ui2 = 2; if (ui1 <  ui2) r = 1; else r = 0; assert_int(1, r, "ui1 < ui2");

    ui1 = 0; ui2 = -1; if (ui1 >  ui2) r = 1; else r = 0; assert_int(0, r, "ui1 > ui2");
    ui1 = 0; ui2 =  1; if (ui1 >  ui2) r = 1; else r = 0; assert_int(0, r, "ui1 > ui2");
    ui1 = 1; ui2 =  1; if (ui1 >  ui2) r = 1; else r = 0; assert_int(0, r, "ui1 > ui2");
    ui1 = 2; ui2 =  1; if (ui1 >  ui2) r = 1; else r = 0; assert_int(1, r, "ui1 > ui2");

    ui1 = -1; ui2 = 0; if (ui1 <= ui2) r = 1; else r = 0; assert_int(0, r, "ui1 <= ui2");
    ui1 =  1; ui2 = 0; if (ui1 <= ui2) r = 1; else r = 0; assert_int(0, r, "ui1 <= ui2");
    ui1 =  1; ui2 = 1; if (ui1 <= ui2) r = 1; else r = 0; assert_int(1, r, "ui1 <= ui2");
    ui1 =  1; ui2 = 2; if (ui1 <= ui2) r = 1; else r = 0; assert_int(1, r, "ui1 <= ui2");

    ui1 = 0; ui2 = -1; if (ui1 >= ui2) r = 1; else r = 0; assert_int(0, r, "ui1 >= ui2");
    ui1 = 0; ui2 =  1; if (ui1 >= ui2) r = 1; else r = 0; assert_int(0, r, "ui1 >= ui2");
    ui1 = 1; ui2 =  1; if (ui1 >= ui2) r = 1; else r = 0; assert_int(1, r, "ui1 >= ui2");
    ui1 = 2; ui2 =  1; if (ui1 >= ui2) r = 1; else r = 0; assert_int(1, r, "ui1 >= ui2");
}

static void test_integer_constant_sizes() {
    // Sizes of signed integer constants
    assert_int(4,  sizeof(1),                    "Integer constant size 1");
    assert_int(4,  sizeof(127),                  "Integer constant size 2");
    assert_int(4,  sizeof(128),                  "Integer constant size 3");
    assert_int(4,  sizeof(255),                  "Integer constant size 4");
    assert_int(4,  sizeof(256),                  "Integer constant size 5");
    assert_int(4,  sizeof(32767),                "Integer constant size 6");
    assert_int(4,  sizeof(32768),                "Integer constant size 7");
    assert_int(4,  sizeof(268435456),            "Integer constant size 8");
    assert_int(4,  sizeof(2147483647),           "Integer constant size 9");
    assert_int(8,  sizeof(2147483648),           "Integer constant size 10");
    assert_int(8,  sizeof(4294967295),           "Integer constant size 11");
    assert_int(8,  sizeof(4294967296),           "Integer constant size 12");
    assert_int(8,  sizeof(9223372036854775807),  "Integer constant size 13");
    // Note: not going beyond 9223372036854775807 since gcc and clang disagree
    // on the size. clang says 8, gcc says 16. What gcc is doing baffles me.

    // Sizes of unsigned hex constants
    assert_int(4,  sizeof(0x1),                 "Hex constant size 1");
    assert_int(4,  sizeof(0x7f),                "Hex constant size 2");
    assert_int(4,  sizeof(0x80),                "Hex constant size 3");
    assert_int(4,  sizeof(0xff),                "Hex constant size 4");
    assert_int(4,  sizeof(0x100),               "Hex constant size 5");
    assert_int(4,  sizeof(0x7fff),              "Hex constant size 6");
    assert_int(4,  sizeof(0x8000),              "Hex constant size 7");
    assert_int(4,  sizeof(0x10000000),          "Hex constant size 8");
    assert_int(4,  sizeof(0x7fffffff),          "Hex constant size 9");
    assert_int(4,  sizeof(0x80000000),          "Hex constant size 10");
    assert_int(4,  sizeof(0xffffffff),          "Hex constant size 11");
    assert_int(8,  sizeof(0x100000000),         "Hex constant size 12");
    assert_int(8,  sizeof(0x7fffffffffffffff),  "Hex constant size 13");
    assert_int(8,  sizeof(0x8000000000000000),  "Hex constant size 14");
    assert_int(8,  sizeof(0xffffffffffffffff),  "Hex constant size 15");
}

static void test_hex_and_octal_constants() {
    // Hex constants
    assert_int(1,                     0x1,                      "Hex constants 1");
    assert_int(9,                     0x9,                      "Hex constants 2");
    assert_int(10,                    0xa,                      "Hex constants 3");
    assert_int(15,                    0xf,                      "Hex constants 4");
    assert_int(255,                   0xff,                     "Hex constants 5");
    assert_int(65535,                 0xffff,                   "Hex constants 6");
    assert_int(16777215,              0xffffff,                 "Hex constants 7");
    assert_int(-1,                    0xffffffff,               "Hex constants 8");
    assert_long(1099511627775,        0xffffffffff,             "Hex constants 9");
    assert_long(281474976710655,      0xffffffffffff,           "Hex constants 10");
    assert_long(72057594037927935,    0xffffffffffffff,         "Hex constants 11");
    assert_long(18446744073709551615u, 0xfffffffffffffffful,    "Hex constants 12");
    assert_long(305419896,            0x12345678,               "Hex constants 13");
    assert_long(11259375,             0xabcdef,                 "Hex constants 14");
    assert_long(11259375,             0xABCDEF,                 "Hex constants 15");

    // Octal constants
    assert_int(1,                     01,                       "Octal constants 1");
    assert_int(6,                     06,                       "Octal constants 2");
    assert_int(7,                     07,                       "Octal constants 3");
    assert_int(63,                    077,                      "Octal constants 4");
    assert_int(511,                   0777,                     "Octal constants 5");
    assert_int(4095,                  07777,                    "Octal constants 6");
    assert_int(32767,                 077777,                   "Octal constants 7");
    assert_int(262143,                0777777,                  "Octal constants 8");
    assert_int(2097151,               07777777,                 "Octal constants 9");
    assert_int(16777215,              077777777,                "Octal constants 10");
    assert_int(134217727,             0777777777,               "Octal constants 11");
    assert_int(1073741823,            07777777777,              "Octal constants 12");
    assert_long(8589934591,           077777777777,             "Octal constants 13");
    assert_long(4398046511103,        077777777777777,          "Octal constants 14");
    assert_long(2251799813685247,     077777777777777777,       "Octal constants 15");
    assert_long(1152921504606846975,  077777777777777777777,    "Octal constants 16");
}

void test_constant_suffixes() {
    // L and has no effect since longs and ints are the same
    assert_int(8, sizeof(1L),           "Constant suffix L 1a");
    assert_int(8, sizeof(2147483648L),  "Constant suffix L 1b");
    assert_int(8, sizeof(4294967296L),  "Constant suffix L 1c");
    assert_int(4, sizeof(0x1),          "Constant suffix L 2a");
    assert_int(8, sizeof(0x80000000L),  "Constant suffix L 2b");
    assert_int(8, sizeof(0x100000000L), "Constant suffix L 2c");

    // U. The effect of U is that all values are positive, even when negated
    assert_int(1, -1U           > 0, "Constant suffix U 1a");
    assert_int(1, -2147483648U  > 0, "Constant suffix U 1b");
    assert_int(1, -4294967296U  > 0, "Constant suffix U 1c");
    assert_int(1, -0x1U         > 0, "Constant suffix U 2a");
    assert_int(1, -0x80000000U  > 0, "Constant suffix U 2b");
    assert_int(1, -0x100000000U > 0, "Constant suffix U 2c");
    assert_int(1, -01U         > 0,  "Constant suffix U 3a");

    // LL
    assert_int(8, sizeof(1LL),   "Constant suffix LL 1a");
    assert_int(8, sizeof(0x1LL), "Constant suffix LL 2a");

    // U with combinations
    assert_int(1, -1UL > 0,  "Constant suffix UL");
    assert_int(1, -1LU > 0,  "Constant suffix LU");
    assert_int(1, -1ULL > 0, "Constant suffix ULL");
    assert_int(1, -1LLU > 0, "Constant suffix LLU");

    // // LL with combinations
    assert_int(8, sizeof(1LLU),   "Constant suffix LLU 1a");
    assert_int(8, sizeof(1ULL),   "Constant suffix ULL 1a");

    // // Some lower case sanity checks
    assert_int(8, sizeof(1l),   "Constant suffix lowercase l");
    assert_int(1, -1u > 0,      "Constant suffix lowercase u");
    assert_int(8, sizeof(1ll),  "Constant suffix lowercase ll");
    assert_int(1, -1ul > 0,     "Constant suffix lowercase ul");
    assert_int(8, sizeof(1llu), "Constant suffix lowercase llu");
}

void test_800000000_bug() {
    int i;
    long l;

    // 0x80000000 is an unsigned int
    // 0x80000000l is a signed long
    // The - happens after the (positive) constant is lexed
    assert_long(0x80000000,            -0x80000000, "0x80000000 == -0x80000000");
    assert_long(0xffffffff80000000ul, -0x80000000l, "0xffffffff80000000ul == -0x80000000l");

    l = 0x80000000; if (l >= -0x80000000)  i = 1; else i = 0; assert_int(1, i, "0x80000000 >= -0x80000000\n");
    l = 0x7fffffff; if (l >= -0x80000000)  i = 1; else i = 0; assert_int(0, i, "0x7fffffff >= -0x80000000\n");
    l = 0x80000000; if (l >= -0x80000000l) i = 1; else i = 0; assert_int(1, i, "0x80000000 >= -0x80000000l\n");
    l = 0x7fffffff; if (l >= -0x80000000l) i = 1; else i = 0; assert_int(1, i, "0x7fffffff >= -0x80000000l\n");
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

void func_c(char  c, long value, char *message) { assert_long((long) c, value, message); }
void func_s(short s, long value, char *message) { assert_long((long) s, value, message); }
void func_i(int   i, long value, char *message) { assert_long((long) i, value, message); }
void func_l(long  l, long value, char *message) { assert_long((long) l, value, message); }

void func_uc(unsigned char  uc, long value, char *message) { assert_long((long) uc, value, message); }
void func_us(unsigned short us, long value, char *message) { assert_long((long) us, value, message); }
void func_ui(unsigned int   ui, long value, char *message) { assert_long((long) ui, value, message); }
void func_ul(unsigned long  ul, long value, char *message) { assert_long((long) ul, value, message); }

void test_int_int_assignment() {
    char c1, c2;
    int i1, i2;
    short s1, s2;
    long l1, l2;

    c1 = s1 = i1 = l1 = 1;

    // Test self assignments by piggybacking on function register assignment
    // Otherwise, coalescing registers may get in the way and the rule isn't tested.
    func_c(c1, 1, "uc = uc");
    func_s(s1, 1, "us = us");
    func_i(i1, 1, "ui = ui");
    func_l(l1, 1, "ul = ul");

              func_c(c1, 1, "uc = uc");
    c1 = s1;  func_c(c1, 1, "uc = us");
    c1 = i1;  func_c(c1, 1, "uc = ui");
    c1 = l1;  func_c(c1, 1, "uc = ul");

    s1 = c1;  func_s(s1, 1, "us = uc");
              func_s(s1, 1, "us = us");
    s1 = i1;  func_s(s1, 1, "us = ui");
    s1 = l1;  func_s(s1, 1, "us = ul");

    i1 = c1;  func_i(i1, 1, "ui = uc");
    i1 = s1;  func_i(i1, 1, "ui = us");
              func_i(i1, 1, "ui = ui");
    i1 = l1;  func_i(i1, 1, "ui = ul");

    l1 = c1;  func_l(l1, 1, "ul = uc");
    l1 = s1;  func_l(l1, 1, "ul = us");
    l1 = l1;  func_l(l1, 1, "ul = ui");
              func_l(l1, 1, "ul = ul");

    func_c(c1, 1, "func_c(uc)");
    func_c(s1, 1, "func_c(us)");
    func_c(i1, 1, "func_c(ui)");
    func_c(l1, 1, "func_c(ul)");

    func_s(c1, 1, "func_s(uc)");
    func_s(s1, 1, "func_s(us)");
    func_s(i1, 1, "func_s(ui)");
    func_s(l1, 1, "func_s(ul)");

    func_i(c1, 1, "func_i(uc)");
    func_i(s1, 1, "func_i(us)");
    func_i(i1, 1, "func_i(ui)");
    func_i(l1, 1, "func_i(ul)");

    func_l(c1, 1, "func_l(uc)");
    func_l(s1, 1, "func_l(us)");
    func_l(i1, 1, "func_l(ui)");
    func_l(l1, 1, "func_l(ul)");
}

void test_int_uint_assignment() {
    char c1;
    int i1;
    short s1;
    long l1;

    unsigned char c2;
    unsigned int i2;
    unsigned short s2;
    unsigned long l2;

    c2 = s2 = i2 = l2 = 1;

    // Test self assignments by piggybacking on function register assignment
    // Otherwise, coalescing registers may get in the way and the rule isn't tested.
    func_uc(c2, 1, "uc = uc");
    func_us(s2, 1, "us = us");
    func_ui(i2, 1, "ui = ui");
    func_ul(l2, 1, "ul = ul");

              func_uc(c2, 1, "uc = uc");
    c1 = s2;  func_uc(c2, 1, "uc = us");
    c1 = i2;  func_uc(c2, 1, "uc = ui");
    c1 = l2;  func_uc(c2, 1, "uc = ul");

    s1 = c2;  func_us(s2, 1, "us = uc");
              func_us(s2, 1, "us = us");
    s1 = i2;  func_us(s2, 1, "us = ui");
    s1 = l2;  func_us(s2, 1, "us = ul");

    i1 = c2;  func_ui(i2, 1, "ui = uc");
    i1 = s2;  func_ui(i2, 1, "ui = us");
              func_ui(i2, 1, "ui = ui");
    i1 = l2;  func_ui(i2, 1, "ui = ul");

    l1 = c2;  func_ul(l2, 1, "ul = uc");
    l1 = s2;  func_ul(l2, 1, "ul = us");
    l1 = l2;  func_ul(l2, 1, "ul = ui");
              func_ul(l2, 1, "ul = ul");

    func_uc(c2, 1, "func_uc(uc)");
    func_uc(s2, 1, "func_uc(us)");
    func_uc(i2, 1, "func_uc(ui)");
    func_uc(l2, 1, "func_uc(ul)");

    func_us(c2, 1, "func_us(uc)");
    func_us(s2, 1, "func_us(us)");
    func_us(i2, 1, "func_us(ui)");
    func_us(l2, 1, "func_us(ul)");

    func_ui(c2, 1, "func_ui(uc)");
    func_ui(s2, 1, "func_ui(us)");
    func_ui(i2, 1, "func_ui(ui)");
    func_ui(l2, 1, "func_ui(ul)");

    func_ul(c2, 1, "func_ul(uc)");
    func_ul(s2, 1, "func_ul(us)");
    func_ul(i2, 1, "func_ul(ui)");
    func_ul(l2, 1, "func_ul(ul)");
}

void test_uint_int_assignment() {
    unsigned char c1;
    unsigned int i1;
    unsigned short s1;
    unsigned long l1;

    char c2;
    int i2;
    short s2;
    long l2;

    c2 = s2 = i2 = l2 = 1;

    // Test self assignments by piggybacking on function register assignment
    // Otherwise, coalescing registers may get in the way and the rule isn't tested.
    func_c(c2, 1, "uc = uc");
    func_s(s2, 1, "us = us");
    func_i(i2, 1, "ui = ui");
    func_l(l2, 1, "ul = ul");

              func_c(c2, 1, "uc = uc");
    c1 = s2;  func_c(c2, 1, "uc = us");
    c1 = i2;  func_c(c2, 1, "uc = ui");
    c1 = l2;  func_c(c2, 1, "uc = ul");

    s1 = c2;  func_s(s2, 1, "us = uc");
              func_s(s2, 1, "us = us");
    s1 = i2;  func_s(s2, 1, "us = ui");
    s1 = l2;  func_s(s2, 1, "us = ul");

    i1 = c2;  func_i(i2, 1, "ui = uc");
    i1 = s2;  func_i(i2, 1, "ui = us");
              func_i(i2, 1, "ui = ui");
    i1 = l2;  func_i(i2, 1, "ui = ul");

    l1 = c2;  func_l(l2, 1, "ul = uc");
    l1 = s2;  func_l(l2, 1, "ul = us");
    l1 = l2;  func_l(l2, 1, "ul = ui");
              func_l(l2, 1, "ul = ul");

    func_c(c2, 1, "func_c(uc)");
    func_c(s2, 1, "func_c(us)");
    func_c(i2, 1, "func_c(ui)");
    func_c(l2, 1, "func_c(ul)");

    func_s(c2, 1, "func_s(uc)");
    func_s(s2, 1, "func_s(us)");
    func_s(i2, 1, "func_s(ui)");
    func_s(l2, 1, "func_s(ul)");

    func_i(c2, 1, "func_i(uc)");
    func_i(s2, 1, "func_i(us)");
    func_i(i2, 1, "func_i(ui)");
    func_i(l2, 1, "func_i(ul)");

    func_l(c2, 1, "func_l(uc)");
    func_l(s2, 1, "func_l(us)");
    func_l(i2, 1, "func_l(ui)");
    func_l(l2, 1, "func_l(ul)");
}

void test_uint_uint_assignment() {
    unsigned char c1, c2;
    unsigned int i1, i2;
    unsigned short s1, s2;
    unsigned long l1, l2;

    c1 = s1 = i1 = l1 = 1;

    // Test self assignments by piggybacking on function register assignment
    // Otherwise, coalescing registers may get in the way and the rule isn't tested.
    func_uc(c1, 1, "uc = uc");
    func_us(s1, 1, "us = us");
    func_ui(i1, 1, "ui = ui");
    func_ul(l1, 1, "ul = ul");

              func_uc(c1, 1, "uc = uc");
    c1 = s1;  func_uc(c1, 1, "uc = us");
    c1 = i1;  func_uc(c1, 1, "uc = ui");
    c1 = l1;  func_uc(c1, 1, "uc = ul");

    s1 = c1;  func_us(s1, 1, "us = uc");
              func_us(s1, 1, "us = us");
    s1 = i1;  func_us(s1, 1, "us = ui");
    s1 = l1;  func_us(s1, 1, "us = ul");

    i1 = c1;  func_ui(i1, 1, "ui = uc");
    i1 = s1;  func_ui(i1, 1, "ui = us");
              func_ui(i1, 1, "ui = ui");
    i1 = l1;  func_ui(i1, 1, "ui = ul");

    l1 = c1;  func_ul(l1, 1, "ul = uc");
    l1 = s1;  func_ul(l1, 1, "ul = us");
    l1 = l1;  func_ul(l1, 1, "ul = ui");
              func_ul(l1, 1, "ul = ul");

    func_uc(c1, 1, "func_uc(uc)");
    func_uc(s1, 1, "func_uc(us)");
    func_uc(i1, 1, "func_uc(ui)");
    func_uc(l1, 1, "func_uc(ul)");

    func_us(c1, 1, "func_us(uc)");
    func_us(s1, 1, "func_us(us)");
    func_us(i1, 1, "func_us(ui)");
    func_us(l1, 1, "func_us(ul)");

    func_ui(c1, 1, "func_ui(uc)");
    func_ui(s1, 1, "func_ui(us)");
    func_ui(i1, 1, "func_ui(ui)");
    func_ui(l1, 1, "func_ui(ul)");

    func_ul(c1, 1, "func_ul(uc)");
    func_ul(s1, 1, "func_ul(us)");
    func_ul(i1, 1, "func_ul(ui)");
    func_ul(l1, 1, "func_ul(ul)");
}

void test_sign_extend_globals() {
    long l;

    l = 1 + gc;
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_long_constant();
    test_expr();
    test_local_comma_var_declarations();
    test_global_comma_var_declarations();
    test_double_assign();
    test_assign_operations();
    test_assign_to_globals();
    test_integer_sizes();
    test_add_operation_sign();
    test_logical_or_operation_sign();
    test_sizeof_expr();
    test_conditional_jumps();
    test_integer_constant_sizes();
    test_hex_and_octal_constants();
    test_constant_suffixes();
    test_800000000_bug();
    test_pointer_casting_reads();
    test_int_int_assignment();
    test_int_uint_assignment();
    test_uint_int_assignment();
    test_uint_uint_assignment();
    test_sign_extend_globals();

    finalize();
}
