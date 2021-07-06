#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

#ifdef FLOATS
long double gld;

void test_assignment() {
    long double ld1, ld2;

    char *buffer = malloc(100);

    ld1 = 1.1;
    ld2 = ld1;
    sprintf(buffer, "%5.5Lf", ld2);
    assert_int(0, strcmp(buffer, "1.10000"), "Long double assignment local-local");

    gld = ld1;
    sprintf(buffer, "%5.5Lf", gld);
    assert_int(0, strcmp(buffer, "1.10000"), "Long double assignment local-global");

    gld = 1.2;
    ld1 = gld;
    sprintf(buffer, "%5.5Lf", ld1);
    assert_int(0, strcmp(buffer, "1.20000"), "Long double assignment global-local");

    gld = 1.3;
    ld1 = ld2 = gld;
    sprintf(buffer, "%5.5Lf %5.5Lf", ld1, ld2);
    assert_int(0, strcmp(buffer, "1.30000 1.30000"), "Long double assignment global-local-local");
}

void test_arithmetic() {
    long double ld1, ld2, ld3;

    char *buffer = malloc(100);

    ld1 = 1.1;
    ld2 = 3.2;
    gld = 2.1;

    // Unary -
    sprintf(buffer, "%5.5Lf", -1.2L); assert_int(0, strcmp(buffer, "-1.20000"), "Long double unary minus constant");
    sprintf(buffer, "%5.5Lf", -ld1);  assert_int(0, strcmp(buffer, "-1.10000"), "Long double unary minus local");

    // Some elaborate combinations for addition
    sprintf(buffer, "%5.5Lf", ld1 + 1.2); assert_int(0, strcmp(buffer, "2.30000"), "Long double addition local-constant");
    sprintf(buffer, "%5.5Lf", 1.2 + ld1); assert_int(0, strcmp(buffer, "2.30000"), "Long double addition constant-local");
    sprintf(buffer, "%5.5Lf", gld + 1.2); assert_int(0, strcmp(buffer, "3.30000"), "Long double addition global-constant");
    sprintf(buffer, "%5.5Lf", 1.2 + gld); assert_int(0, strcmp(buffer, "3.30000"), "Long double addition constant-global");
    sprintf(buffer, "%5.5Lf", ld1 + ld2); assert_int(0, strcmp(buffer, "4.30000"), "Long double addition local-local");
    sprintf(buffer, "%5.5Lf", ld1 + gld); assert_int(0, strcmp(buffer, "3.20000"), "Long double addition local-global");
    sprintf(buffer, "%5.5Lf", gld + ld1); assert_int(0, strcmp(buffer, "3.20000"), "Long double addition global-local");
    sprintf(buffer, "%5.5Lf", gld + gld); assert_int(0, strcmp(buffer, "4.20000"), "Long double addition global-global");

    // A local and global are equivalent in terms of backend so it suffices to only test with locals

    // Subtraction
    sprintf(buffer, "%5.5Lf", ld1 - 1.2); assert_int(0, strcmp(buffer, "-0.10000"), "Long double subtraction local-constant");
    sprintf(buffer, "%5.5Lf", 1.2 - ld1); assert_int(0, strcmp(buffer, "0.10000"),  "Long double subtraction constant-local");
    sprintf(buffer, "%5.5Lf", ld1 - ld2); assert_int(0, strcmp(buffer, "-2.10000"), "Long double subtraction local-local");

    // Multiplication
    sprintf(buffer, "%5.5Lf", ld1 * 1.2); assert_int(0, strcmp(buffer, "1.32000"), "Long double multiplication local-constant");
    sprintf(buffer, "%5.5Lf", 1.2 * ld1); assert_int(0, strcmp(buffer, "1.32000"), "Long double multiplication constant-local");
    sprintf(buffer, "%5.5Lf", ld1 * ld2); assert_int(0, strcmp(buffer, "3.52000"), "Long double multiplication local-local");

    // Division
    ld1 = 10.0L;
    ld2 = 20.0L;
    sprintf(buffer, "%5.5Lf", ld1 / 5.0); assert_int(0, strcmp(buffer, "2.00000"), "Long double division local-constant");
    sprintf(buffer, "%5.5Lf", 5.0 / ld1); assert_int(0, strcmp(buffer, "0.50000"),  "Long double division constant-local");
    sprintf(buffer, "%5.5Lf", ld1 / ld2); assert_int(0, strcmp(buffer, "0.50000"), "Long double division local-local");

    // Combinations
    ld1 = 10.1L;
    ld2 = 0.50L;
    ld3 = 3.0L;
    sprintf(buffer, "%Lf", -ld1 * (ld2 + ld3) * (ld1 + ld2) * (ld2 - ld1) / ld3);
    assert_int(0, strcmp(buffer, "1199.072000"),  "Long double combination");
}

void test_comparison_assignment() {
    long double ld1, ld2;

    ld1 = 1.0;
    ld2 = 2.0;
    assert_int(1, ld1 < ld2, "1.0 < 2.0");
    assert_int(0, ld1 > ld2, "1.0 > 2.0");
    assert_int(1, ld1 <= ld2, "1.0 <= 2.0");
    assert_int(0, ld1 >= ld2, "1.0 >= 2.0");

    assert_int(1, ld1 < 3.0, "1.0 < 3.0 cst");
    assert_int(1, 0.5 < ld1, "0.0 cst < 1.0");
    assert_int(2, ld1 < ld2 ? 2 : 3, "1.0 < 2.0 ternary true");
    assert_int(3, ld2 < ld1 ? 2 : 3, "1.0 < 2.0 ternary false");

    ld2 = 1.0;
    assert_int(1, ld1 == ld2, "1.0 == 1.0");
    assert_int(0, ld1 != ld2, "1.0 != 1.0");

    assert_int(1, ld1 <= ld2, "1.0 <= 2.0");
    assert_int(1, ld1 >= ld2, "1.0 >= 2.0");
    assert_int(1, ld2 <= ld1, "1.0 <= 1.0");
    assert_int(1, ld2 >= ld1, "1.0 >= 1.0");

    ld2 = 2.0;
    assert_int(0, ld1 == ld2, "1.0 == 2.0");
    assert_int(1, ld1 != ld2, "1.0 != 2.0");

    assert_int(1, ld1 <= ld2, "1.0 <= 2.0");
    assert_int(0, ld1 >= ld2, "1.0 >= 2.0");
    assert_int(0, ld2 <= ld1, "2.0 <= 1.0");
    assert_int(1, ld2 >= ld1, "2.0 >= 1.0");

    // Some crazy nan tests
    long double nan1, nan2;
    *((long *) &nan1) = -1;
    *((long *) &nan1 + 1) = -1;
    *((long *) &nan2) = -1;
    *((long *) &nan2 + 1) = -1;

    assert_int(0, nan1 == nan2, "nan-nan equality");
    assert_int(1, nan1 != nan2, "nan-nan inequality");

    // If one of the operands is a nan, then the comparison must return false
    assert_int(0, nan1 <  nan2, "nan1 < nan2");
    assert_int(0, nan1 >  nan2, "nan1 > nan2");
    assert_int(0, nan1 <= nan2, "nan1 <= nan2");
    assert_int(0, nan1 >= nan2, "nan1 >= nan2");
    assert_int(0, 0.0  <  nan1, "0.0 < nan1");
    assert_int(0, 0.0  >  nan1, "0.0 > nan1");
    assert_int(0, 0.0  >= nan1, "0.0 >= nan1");
    assert_int(0, 0.0  <= nan1, "0.0 <= nan1");
    assert_int(0, nan1 <  0.0,  "nan1 < 0.0");
    assert_int(0, nan1 >  0.0,  "nan1 > 0.0");
    assert_int(0, nan1 >= 0.0,  "nan1 >=0.0");
    assert_int(0, nan1 <= 0.0,  "nan1 <=0.0");
}

void test_comparison_conditional_jump() {
    long double ld1, ld2;

    ld1 = 1.0;
    ld2 = 2.0;
    if (ld1 < ld2)  assert_int(1, 1, "1.0 < 2.0 true case");  else assert_int(1, 0, "1.0 < 2.0 false case");
    if (ld1 > ld2)  assert_int(1, 0, "1.0 < 2.0 true case");  else assert_int(1, 1, "1.0 < 2.0 false case");
    if (ld1 <= ld2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (ld1 >= ld2) assert_int(1, 0, "1.0 <= 2.0 true case"); else assert_int(1, 1, "1.0 <= 2.0 false case");

    ld2 = 1.0;
    if (ld1 <= ld2) assert_int(1, 1, "1.0 <= 1.0 true case"); else assert_int(1, 0, "1.0 <= 1.0 false case");
    if (ld1 >= ld2) assert_int(1, 1, "1.0 <= 1.0 true case"); else assert_int(1, 0, "1.0 <= 1.0 false case");

    ld2 = 2.0;
    if (ld1 >= ld2) assert_int(1, 0, "1.0 >= 2.0 true case"); else assert_int(1, 1, "1.0 >= 2.0 false case");
    if (ld1 <= ld2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (ld1 <= ld2) assert_int(1, 1, "1.0 <= 2.0 true case"); else assert_int(1, 0, "1.0 <= 2.0 false case");
    if (ld1 >= ld2) assert_int(1, 0, "1.0 >= 2.0 true case"); else assert_int(1, 1, "1.0 >= 2.0 false case");

    // Some crazy nan tests
    long double nan1, nan2;
    *((long *) &nan1) = -1;
    *((long *) &nan1 + 1) = -1;
    *((long *) &nan2) = -1;
    *((long *) &nan2 + 1) = -1;

    if (nan1 ==  nan2) assert_int(0, 1, "nan comparison nan1 == nan2 true case"); else assert_int(1, 1, "nan comparison nan1 == nan2 false case");
    if (nan1 !=  nan2) assert_int(1, 1, "nan comparison nan1 != nan2 true case"); else assert_int(0, 1, "nan comparison nan1 != nan2 false case");

    // If one of the operands is a nan, then the comparison must return false
    if (nan1 <  nan2) assert_int(0, 1, "nan comparison nan1 <  nan2 true case"); else assert_int(1, 1, "nan comparison nan1 <  nan2 false case");
    if (nan1 >  nan2) assert_int(0, 1, "nan comparison nan1 >  nan2 true case"); else assert_int(1, 1, "nan comparison nan1 >  nan2 false case");
    if (nan1 <= nan2) assert_int(0, 1, "nan comparison nan1 <= nan2 true case"); else assert_int(1, 1, "nan comparison nan1 <= nan2 false case");
    if (nan1 >= nan2) assert_int(0, 1, "nan comparison nan1 >= nan2 true case"); else assert_int(1, 1, "nan comparison nan1 >= nan2 false case");
    if (0.0  <  nan1) assert_int(0, 1, "nan comparison 0.0  <  nan1 true case"); else assert_int(1, 1, "nan comparison 0.0  <  nan1 false case");
    if (0.0  >  nan1) assert_int(0, 1, "nan comparison 0.0  >  nan1 true case"); else assert_int(1, 1, "nan comparison 0.0  >  nan1 false case");
    if (0.0  >= nan1) assert_int(0, 1, "nan comparison 0.0  >= nan1 true case"); else assert_int(1, 1, "nan comparison 0.0  >= nan1 false case");
    if (0.0  <= nan1) assert_int(0, 1, "nan comparison 0.0  <= nan1 true case"); else assert_int(1, 1, "nan comparison 0.0  <= nan1 false case");
    if (nan1 <  0.0 ) assert_int(0, 1, "nan comparison nan1 <  0.0  true case"); else assert_int(1, 1, "nan comparison nan1 <  0.0  false case");
    if (nan1 >  0.0 ) assert_int(0, 1, "nan comparison nan1 >  0.0  true case"); else assert_int(1, 1, "nan comparison nan1 >  0.0  false case");
    if (nan1 >= 0.0 ) assert_int(0, 1, "nan comparison nan1 >= 0.0  true case"); else assert_int(1, 1, "nan comparison nan1 >= 0.0  false case");
    if (nan1 <= 0.0 ) assert_int(0, 1, "nan comparison nan1 <= 0.0  true case"); else assert_int(1, 1, "nan comparison nan1 <= 0.0  false case");
}

void test_jz_jnz() {
    long double zero = 0.0L;
    long double one = 1.0L;

    long double nan;
    *((long *) &nan) = -1;
    *((long *) &nan + 1) = -1;

    int i;
    i = 1; assert_int(0, zero && i++, "long double zero && 1 result"); assert_int(1, i, "long double zero && 1 i");
    i = 1; assert_int(1, zero || i++, "long double zero || 0 result"); assert_int(2, i, "long double zero || 0 i");
    i = 1; assert_int(1, one  && i++, "long double one  && 1 result"); assert_int(2, i, "long double one  && 1 i");
    i = 1; assert_int(1, one  || i++, "long double one  || 0 result"); assert_int(1, i, "long double one  || 0 i");
    i = 1; assert_int(1, nan  && i++, "long double nan  && 1 result"); assert_int(2, i, "long double nan  && 1 i");
    i = 1; assert_int(1, nan  || i++, "long double nan  || 0 result"); assert_int(1, i, "long double nan  || 0 i");
}

// Some unfinished pointer work on long doubles
void test_pointers() {
    long double ld, *pld;
    char *buffer = malloc(100);

    ld = 1.0;
    pld = &ld;

    // Make a nan, for the hell of it
    *((long *) &ld) = -1;
    *((long *) &ld + 1) = -1;
}

void test_constant_operations() {
    char *buffer = malloc(100);

    sprintf(buffer, "%5.5Lf", 1.1L + 1.2L); assert_int(0, strcmp(buffer, "2.30000"), "Long double constant +");
    sprintf(buffer, "%5.5Lf", 1.2L - 1.1L); assert_int(0, strcmp(buffer, "0.10000"), "Long double constant -");
    sprintf(buffer, "%5.5Lf", 1.2L * 1.1L); assert_int(0, strcmp(buffer, "1.32000"), "Long double constant *");
    sprintf(buffer, "%5.5Lf", 1.0L / 2.0L); assert_int(0, strcmp(buffer, "0.50000"), "Long double constant /"); // Tests a bug in the / 1.0 optimization
    sprintf(buffer, "%5.5Lf", 1.2L / 2.0L); assert_int(0, strcmp(buffer, "0.60000"), "Long double constant /");

    assert_int(1, 1.0L == 1.0L, "1.0L == 1.0L");
    assert_int(0, 1.0L != 1.0L, "1.0L != 1.0L");
    assert_int(1, 1.0L <  2.0L, "1.0L <  2.0L");
    assert_int(0, 2.0L <  1.0L, "2.0L <  1.0L");
    assert_int(0, 1.0L >  2.0L, "1.0L >  2.0L");
    assert_int(1, 2.0L >  1.0L, "2.0L >  1.0L");
    assert_int(1, 1.0L <= 2.0L, "1.0L <= 2.0L");
    assert_int(0, 2.0L <= 1.0L, "2.0L <= 1.0L");
    assert_int(0, 1.0L >= 2.0L, "1.0L >= 2.0L");
    assert_int(1, 2.0L >= 1.0L, "2.0L >= 1.0L");
    assert_int(1, 1.0L <= 1.0L, "1.0L <= 1.0L");
    assert_int(1, 1.0L <= 1.0L, "1.0L <= 1.0L");
    assert_int(1, 1.0L >= 1.0L, "1.0L >= 1.0L");
    assert_int(1, 1.0L >= 1.0L, "1.0L >= 1.0L");
}

void test_inc_dec() {
    long double ld = 1.0;
    char *buffer = malloc(100);

    ++ld; sprintf(buffer, "%5.5Lf", ld); assert_int(0, strcmp(buffer, "2.00000"), "Long double prefix ++");
    --ld; sprintf(buffer, "%5.5Lf", ld); assert_int(0, strcmp(buffer, "1.00000"), "Long double prefix --");
    ld++; sprintf(buffer, "%5.5Lf", ld); assert_int(0, strcmp(buffer, "2.00000"), "Long double postfix ++");
    ld--; sprintf(buffer, "%5.5Lf", ld); assert_int(0, strcmp(buffer, "1.00000"), "Long double postfix --");
}

#endif

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    #ifdef FLOATS
    test_assignment();
    test_arithmetic();
    test_comparison_assignment();
    test_comparison_conditional_jump();
    test_jz_jnz();
    test_pointers();
    test_constant_operations();
    test_inc_dec();
    #endif

    finalize();
}
