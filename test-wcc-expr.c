#include "test-lib.h"

int check_stack_alignment();

int verbose;
int passes;
int failures;

int gcvi, gcvj;


// Test 64 bit constants are assigned properly
void test_long_constant() {
    long l;

    l = 0x1000000001010101;
    assert_int(1, l && 0x1000000000000000, "64 bit constants");
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

    finalize();
}
