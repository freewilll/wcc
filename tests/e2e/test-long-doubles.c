#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

int gi;

long double gld, *gpld;

struct s1 { int i; long double ld; };
struct s2 { long double ld; int i; };

void assert_ld_string(long double ld, char *expected, char *message) {
    char *buffer = malloc(100);

    sprintf(buffer, "%5.5Lf", ld);

    int matches = !strcmp(buffer, expected);
    if (!matches) printf("Expected \"%s\", got \"%s\"\n", expected, buffer);
    assert_int(1, matches, message);
}

void test_assignment() {
    long double ld1, ld2;

    char *buffer = malloc(100);

    ld1 = 1.1;
    ld2 = ld1;
    assert_ld_string(ld2, "1.10000", "Long double assignment local-local");

    gld = ld1;
    assert_ld_string(gld, "1.10000", "Long double assignment local-global");

    gld = 1.2;
    ld1 = gld;
    assert_ld_string(ld1, "1.20000", "Long double assignment global-local");

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
    assert_ld_string(-1.2L, "-1.20000", "Long double unary minus constant");
    assert_ld_string(-ld1,  "-1.10000", "Long double unary minus local");

    // Some elaborate combinations for addition
    assert_ld_string(ld1 + 1.2, "2.30000", "Long double addition local-constant");
    assert_ld_string(1.2 + ld1, "2.30000", "Long double addition constant-local");
    assert_ld_string(gld + 1.2, "3.30000", "Long double addition global-constant");
    assert_ld_string(1.2 + gld, "3.30000", "Long double addition constant-global");
    assert_ld_string(ld1 + ld2, "4.30000", "Long double addition local-local");
    assert_ld_string(ld1 + gld, "3.20000", "Long double addition local-global");
    assert_ld_string(gld + ld1, "3.20000", "Long double addition global-local");
    assert_ld_string(gld + gld, "4.20000", "Long double addition global-global");

    // A local and global are equivalent in terms of backend so it suffices to only test with locals

    // Subtraction
    assert_ld_string(ld1 - 1.2, "-0.10000", "Long double subtraction local-constant");
    assert_ld_string(1.2 - ld1, "0.10000",  "Long double subtraction constant-local");
    assert_ld_string(ld1 - ld2, "-2.10000", "Long double subtraction local-local");

    // Multiplication
    assert_ld_string(ld1 * 1.2, "1.32000", "Long double multiplication local-constant");
    assert_ld_string(1.2 * ld1, "1.32000", "Long double multiplication constant-local");
    assert_ld_string(ld1 * ld2, "3.52000", "Long double multiplication local-local");

    // Division
    ld1 = 10.0L;
    ld2 = 20.0L;
    assert_ld_string(ld1 / 5.0, "2.00000", "Long double division local-constant");
    assert_ld_string(5.0 / ld1, "0.50000",  "Long double division constant-local");
    assert_ld_string(ld1 / ld2, "0.50000", "Long double division local-local");

    // Combinations
    ld1 = 10.1L;
    ld2 = 0.50L;
    ld3 = 3.0L;
    assert_ld_string(-ld1 * (ld2 + ld3) * (ld1 + ld2) * (ld2 - ld1) / ld3, "1199.07200", "Long double combination");
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

void test_pointers() {
    long double ld, *pld;
    char *buffer = malloc(100);

    ld = 1.1;

    // Make a nan, for the hell of it
    *((long *) &ld) = -1;
    *((long *) &ld + 1) = -1;

    assert_ld_string(ld, " -nan", "-nan");

    ld = 1.1;
    gld = 2.1;

    // Local pointer to local
    pld = &ld;
    assert_ld_string(*pld,     "1.10000", "Local -> local long double deref");
    assert_ld_string(*pld + 1, "2.10000", "Local -> local long double deref + 1");

    // Local pointer to global
    pld = &gld;
    assert_ld_string(*pld, "2.10000", "Local -> global long double deref");

    // Global pointer to local
    gpld = &ld;
    assert_ld_string(*gpld, "1.10000", "Global -> local long double deref");

    // Global pointer to global
    gpld = &gld;
    assert_ld_string(*gpld, "2.10000", "Global -> global long double deref");

    // Assignments to dereferenced pointers from a constant
    pld  = &ld; *pld  = 3.1; assert_ld_string(*pld,     "3.10000", "*pld");
                *pld  = 6.1; assert_ld_string(*pld + 1, "7.10000", "*pld + 1");
    gpld = &ld; *gpld = 4.1; assert_ld_string(*gpld,    "4.10000", "*gpld");

    // Assignments to dereferenced pointers from a local/global
    pld = &ld;
    gpld = &gld;
    *pld  = ld;  assert_ld_string(*pld,  "4.10000", "*pld = pld; *pld");
    *gpld = gld; assert_ld_string(*gpld, "2.10000", "*gpld = gld; *gpld");

    assert_ld_string(*pld + 1,  "5.10000", "*pld = pld; *pld + 1");
    assert_ld_string(*gpld + 1, "3.10000", "*gpld = gld; *gpld + 1");

    long double ld2, *pld2;
    pld2 = &ld2;
    *pld2 = *pld;  assert_ld_string(*pld2, "4.10000", "*pld2 = *pld");
    *pld2 = *gpld; assert_ld_string(*pld2, "2.10000", "*pld2 = *gpld");

    // Malloc tests
    pld  = malloc(sizeof(long double)); *pld  = 5.1L; assert_ld_string(*pld,  "5.10000", "*pld from malloc");
    gpld = malloc(sizeof(long double)); *gpld = 6.1L; assert_ld_string(*gpld, "6.10000", "*gpld from malloc");

    long double *pld3 = malloc(sizeof(long double));
    *pld = 1.1;
    *pld2 = 1.2;
    *pld3 = *pld + *pld2; assert_ld_string(*pld3,  "2.30000", "*ld3 = *pld + *pld2");
    *pld3 = *pld - *pld2; assert_ld_string(*pld3, "-0.10000", "*ld3 = *pld - *pld2");

    // Double dereference
    long double **ppld;
    ld = 1.1L;
    pld = &ld;
    ppld = &pld;
    assert_ld_string(*pld  , "1.10000", "*pld");
    assert_ld_string(**ppld, "1.10000", "**ppld");

    // Assignment of constant to memory
    gpld = 0;
    gpld = 0ul;

    // Assignment to a pointer in a register
    ld = 11.1;
    pld = &ld;
    ppld = malloc(sizeof(*pld));
    *ppld = &ld;
    assert_ld_string(**ppld,  "11.10000", "*ppld = &ld");
}

void test_pointer_arithmetic() {
    long double *pld = malloc(2 * sizeof(long double));
    long double *pld1 = pld + 1;

    pld[0] = 1.1;
    pld[1] = 2.1;
    int i = 1;
    assert_ld_string(pld[0],      "1.10000", "pld[0]");
    assert_ld_string(pld[1],      "2.10000", "pld[1]");
    assert_ld_string(pld[i],      "2.10000", "pld[i]");
    assert_ld_string(*(pld + 1),  "2.10000", "*(pld + 1)");
    assert_ld_string(*(1 + pld),  "2.10000", "*(1 + pld)");
    assert_ld_string(*(pld1 - 1), "1.10000", "*(pld1 - 1)");

    assert_int(1, pld1 - pld, "pld1 - pld");
}

void test_pointer_casting() {
    struct s1* s1 = malloc(sizeof(struct s1));
    s1->ld = 1.1;

    // Assertions can't be done since the wcc lexer produces other results than gcc.
    // However, this test suffices to ensure that the code can be compiled.
    *((long *) &s1->ld);
    *((long *) &s1->ld + 1);
}

void test_structs() {
    struct s1 *s1 = malloc(sizeof(struct s1));
    struct s2 *s2 = malloc(sizeof(struct s2));

    s1->ld = 1.1; s2->ld = s1->ld; assert_ld_string(s2->ld, "1.10000", "s2->ld");
    s2->ld = 2.1; s1->ld = s2->ld; assert_ld_string(s1->ld, "2.10000", "s1->ld");
}

void test_constant_operations() {
    char *buffer = malloc(100);

    assert_ld_string(1.1L + 1.2L, "2.30000", "Long double constant +");
    assert_ld_string(1.2L - 1.1L, "0.10000", "Long double constant -");
    assert_ld_string(1.2L * 1.1L, "1.32000", "Long double constant *");
    assert_ld_string(1.0L / 2.0L, "0.50000", "Long double constant /"); // Tests a bug in the / 1.0 optimization
    assert_ld_string(1.2L / 2.0L, "0.60000", "Long double constant /");

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

    ++ld; assert_ld_string(ld, "2.00000", "Long double prefix ++");
    --ld; assert_ld_string(ld, "1.00000", "Long double prefix --");
    ld++; assert_ld_string(ld, "2.00000", "Long double postfix ++");
    ld--; assert_ld_string(ld, "1.00000", "Long double postfix --");

    gld = 1.0;
    ++gld; assert_ld_string(gld, "2.00000", "Global long double prefix ++");
    --gld; assert_ld_string(gld, "1.00000", "Global long double prefix --");
    gld++; assert_ld_string(gld, "2.00000", "Global long double postfix ++");
    gld--; assert_ld_string(gld, "1.00000", "Global long double postfix --");
}

long double return_ld_from_int_constant () {
    return 1;
}

void test_int_long_double_type_changes() {
    long double ld;
    char c;
    short s;
    int i;
    long l;

    unsigned char uc;
    unsigned short us;
    unsigned int ui;
    unsigned long ul;

    char *buffer = malloc(100);

    assert_ld_string((1.1L + 1   ), "2.10000", "1.1 + 1");
    assert_ld_string((1    + 1.1L), "2.10000", "1 + 1.1");
    assert_ld_string((1.1L - 1   ), "0.10000", "1.1 - 1");
    assert_ld_string((2    - 1.1L), "0.90000", "1 - 1.1");
    assert_ld_string((1.1L * 2   ), "2.20000", "1.1 * 2");
    assert_ld_string((2    * 1.1L), "2.20000", "2 * 1.1");
    assert_ld_string((1.1L / 2   ), "0.55000", "1.1 / 2");
    assert_ld_string((3    / 1.5L), "2.00000", "3 / 1.5");

    i = 1; assert_ld_string((1.1L + i   ), "2.10000", "1.1 + i   ");
    i = 1; assert_ld_string((i    + 1.1L), "2.10000", "i    + 1.1");
    i = 1; assert_ld_string((1.1L - i   ), "0.10000", "1.1 - i   ");
    i = 2; assert_ld_string((i    - 1.1L), "0.90000", "i    - 1.1");
    i = 2; assert_ld_string((1.1L * i   ), "2.20000", "1.1 * i   ");
    i = 2; assert_ld_string((i    * 1.1L), "2.20000", "i    * 1.1");
    i = 2; assert_ld_string((1.1L / i   ), "0.55000", "1.1 / i   ");
    i = 3; assert_ld_string((i    / 1.5L), "2.00000", "i    / 1.5");

    ld = 1.1L; assert_ld_string((ld + 1   ), "2.10000", "ld + 1   ");
    ld = 1.1L; assert_ld_string((1    + ld), "2.10000", "1    + ld");
    ld = 1.1L; assert_ld_string((ld - 1   ), "0.10000", "ld - 1   ");
    ld = 1.1L; assert_ld_string((2    - ld), "0.90000", "2    - ld");
    ld = 1.1L; assert_ld_string((ld * 2   ), "2.20000", "ld * 2   ");
    ld = 1.1L; assert_ld_string((2    * ld), "2.20000", "2    * ld");
    ld = 1.1L; assert_ld_string((ld / 2   ), "0.55000", "ld / 2   ");
    ld = 1.5L; assert_ld_string((3    / ld), "2.00000", "3    / ld");

    ld = 1.1L; i = 1; assert_ld_string((ld + i   ), "2.10000", "ld + i   ");
    ld = 1.1L; i = 1; assert_ld_string((i    + ld), "2.10000", "i    + ld");
    ld = 1.1L; i = 1; assert_ld_string((ld - i   ), "0.10000", "ld - i   ");
    ld = 1.1L; i = 2; assert_ld_string((i    - ld), "0.90000", "i    - ld");
    ld = 1.1L; i = 2; assert_ld_string((ld * i   ), "2.20000", "ld * i   ");
    ld = 1.1L; i = 2; assert_ld_string((i    * ld), "2.20000", "i    * ld");
    ld = 1.1L; i = 2; assert_ld_string((ld / i   ), "0.55000", "ld / i   ");
    ld = 1.5L; i = 3; assert_ld_string((i    / ld), "2.00000", "i    / ld");

    // Signed int conversions
    ld = 1.1L; c = 1; assert_ld_string((ld + c), "2.10000", "ld + c");
    ld = 1.1L; s = 1; assert_ld_string((ld + s), "2.10000", "ld + s");
    ld = 1.1L; i = 1; assert_ld_string((ld + i), "2.10000", "ld + i");
    ld = 1.1L; l = 1; assert_ld_string((ld + l), "2.10000", "ld + l");

    ld = 1.1L; c = -1; assert_ld_string((ld + c), "0.10000", "ld + c");
    ld = 1.1L; s = -1; assert_ld_string((ld + s), "0.10000", "ld + s");
    ld = 1.1L; i = -1; assert_ld_string((ld + i), "0.10000", "ld + i");
    ld = 1.1L; l = -1; assert_ld_string((ld + l), "0.10000", "ld + l");

    // Unsigned int conversions
    ld = 1.1L; uc = -1; assert_ld_string((ld + uc), "256.10000",                  "ld + uc");
    ld = 1.1L; us = -1; assert_ld_string((ld + us), "65536.10000",                "ld + us");
    ld = 1.1L; ui = -1; assert_ld_string((ld + ui), "4294967296.10000",           "ld + ui");
    ld = 1.1L; ul = -1; assert_ld_string((ld + ul), "18446744073709551616.00000", "ld + ul");

    // Local signed int conversion
    int lsi, *plsi;
    plsi = &lsi; // Force lsi onto the stack
    lsi = 1;
    assert_ld_string((ld + lsi),  "2.10000", "ld + ul");

    // Global signed int conversion
    gi = 1;
    assert_ld_string((ld + gi), "2.10000", "ld + gi");

    // Relational operator conversion
    ld =  1.1; assert_int(0, ld < 0, " 1.1 < 0");
    ld = -1.1; assert_int(1, ld < 0, "-1.1 < 0)");

    i = 0;
    ld =  1.1; assert_int(0, ld < i, " 1.1 < i");
    ld = -1.1; assert_int(1, ld < i, "-1.1 < i)");

    gi = 0;
    ld =  1.1; assert_int(0, ld < gi, " 1.1 < gi");
    ld = -1.1; assert_int(1, ld < gi, "-1.1 < gi)");

    // Conversion of int -> ld in assignment
    ld = 1;  assert_ld_string(ld, "1.00000",  "assigment of 1");
    ld = -1; assert_ld_string(ld, "-1.00000", "assigment of -1");

    i = 1;  ld = i; assert_ld_string(ld, "1.00000" , "assigment of i=1");
    i = -1; ld = i; assert_ld_string(ld, "-1.00000", "assigment of i=-1");

    gld = 1;  ld = gld; assert_ld_string(ld, "1.00000" , "assigment of gld=1");
    gld = -1; ld = gld; assert_ld_string(ld, "-1.00000", "assigment of gld=-1");

    assert_ld_string(return_ld_from_int_constant(), "1.00000", "return (long double) 1");

    // Casting from int -> long double
    i = gi = -1;
    assert_ld_string((long double) i,  "-1.00000", "(long double) -1");
    assert_ld_string((long double) i,  "-1.00000", "(long double) i");
    assert_ld_string((long double) gi, "-1.00000", "(long double) gi");

    ul = -1;
    assert_ld_string((long double) ul, "18446744073709551615.00000", "(long double) (unsigned long) -1");
}

void assert_long_double_int_conversion(long double ld, char c, short s, int i, long l, unsigned char uc, unsigned short us, unsigned int ui, unsigned long ul, char *message) {
    char *buffer = malloc(100);

    wasprintf(&buffer, "%s c",  message); assert_int(1, (char)           ld == c,  buffer);
    wasprintf(&buffer, "%s s",  message); assert_int(1, (short)          ld == s,  buffer);
    wasprintf(&buffer, "%s i",  message); assert_int(1, (int)            ld == i,  buffer);
    wasprintf(&buffer, "%s l",  message); assert_int(1, (long)           ld == l,  buffer);
    wasprintf(&buffer, "%s uc", message); assert_int(1, (unsigned char)  ld == uc, buffer);
    wasprintf(&buffer, "%s us", message); assert_int(1, (unsigned short) ld == us, buffer);
    wasprintf(&buffer, "%s ui", message); assert_int(1, (unsigned int)   ld == ui, buffer);
    wasprintf(&buffer, "%s ul", message); assert_int(1, (unsigned long)  ld == ul, buffer);
}

int return_int_from_ld_constant() {
    return -42.1L;
}

void test_long_double_type_int_changes() {
    long double ld;

    char c;
    short s;
    int i;
    long l;

    unsigned char uc;
    unsigned short us;
    unsigned int ui;
    unsigned long ul;

    // Moves/casts
    assert_long_double_int_conversion(-100.1L,                        -100,         -100,         -100,                 -100,          156,        65436,         -100, 18446744073709551516UL, "-100.1L                ");
    assert_long_double_int_conversion(100.1L,                          100,          100,          100,                  100,          100,          100,          100,                  100UL, "100.1L                 ");
    assert_long_double_int_conversion(-1000.1L,                         24,        -1000,        -1000,                -1000,           24,        64536,        -1000, 18446744073709550616UL, "-1000.1L               ");
    assert_long_double_int_conversion(1000.1L,                         -24,         1000,         1000,                 1000,          232,         1000,         1000,                 1000UL, "1000.1L                ");
    assert_long_double_int_conversion(-10000.1L,                       -16,       -10000,       -10000,               -10000,          240,        55536,       -10000, 18446744073709541616UL, "-10000.1L              ");
    assert_long_double_int_conversion(10000.1L,                         16,        10000,        10000,                10000,           16,        10000,        10000,                10000UL, "10000.1L               ");
    assert_long_double_int_conversion(-100000.1L,                        0,       -32768,      -100000,              -100000,            0,        31072,      -100000, 18446744073709451616UL, "-100000.1L             ");
    assert_long_double_int_conversion(100000.1L,                         0,       -32768,       100000,               100000,            0,        34464,       100000,               100000UL, "100000.1L              ");
    assert_long_double_int_conversion(-1000000000.1L,                    0,       -32768,  -1000000000,          -1000000000,            0,        13824,  -1000000000, 18446744072709551616UL, "-1000000000.1L         ");
    assert_long_double_int_conversion(1000000000.1L,                     0,       -32768,   1000000000,           1000000000,            0,        51712,   1000000000,           1000000000UL, "1000000000.1L          ");
    assert_long_double_int_conversion(-10000000000.1L,                   0,       -32768,  -2147483648,         -10000000000,            0,            0,  -1410065408, 18446744063709551616UL, "-10000000000.1L        ");
    assert_long_double_int_conversion(10000000000.1L,                    0,       -32768,  -2147483648,          10000000000,            0,            0,   1410065408,          10000000000UL, "10000000000.1L         ");
    assert_long_double_int_conversion(9223372036854775807.1L,            0,       -32768,  -2147483648,  9223372036854775807,            0,            0,           -1,  9223372036854775807UL, "9223372036854775807.1L ");
    assert_long_double_int_conversion(9223372036854775808.1L,            0,       -32768,  -2147483648, -9223372036854775808UL,          0,            0,            0,  9223372036854775808UL, "9223372036854775808.1L ");
    assert_long_double_int_conversion(9900000000000000000.1L,            0,       -32768,  -2147483648, -9223372036854775808UL,          0,            0,            0,  9900000000000000000UL, "9900000000000000000.1L ");

    // Assignment of a constant
    c  = -1.1L;  assert_int(-1, c, "c = -1.1L");
    s  = -2.1L;  assert_int(-2, s, "s = -2.1L");
    i  = -3.1L;  assert_int(-3, i, "i = -3.1L");
    l  = -4.1L;  assert_int(-4, l, "l = -4.1L");
    uc = 100.1L; assert_int(100, uc, "uc = 100.1L");
    us = 200.1L; assert_int(200, us, "us = 200.1L");
    ui = 300.1L; assert_int(300, ui, "ui = 300.1L");
    ul = 400.1L; assert_int(400, ul, "ul = 400.1L");

    // Assignment of a variable
    ld = -1.1L;  c  = ld; assert_int(-1, c, "c = -1.1L via variable");
    ld = -2.1L;  s  = ld; assert_int(-2, s, "s = -2.1L via variable");
    ld = -3.1L;  i  = ld; assert_int(-3, i, "i = -3.1L via variable");
    ld = -4.1L;  l  = ld; assert_int(-4, l, "l = -4.1L via variable");
    ld = 100.1L; uc = ld; assert_int(100, uc, "uc = 100.1L via variable");
    ld = 200.1L; us = ld; assert_int(200, us, "us = 200.1L via variable");
    ld = 300.1L; ui = ld; assert_int(300, ui, "ui = 300.1L via variable");
    ld = 400.1L; ul = ld; assert_int(400, ul, "ul = 400.1L via variable");

    // Function return
    assert_int(-42, return_int_from_ld_constant(), "Return int from long double constant");
}


void function_with_int_parameter(int i) {
    assert_int(1, i, "Long double to int conversion in function call");
}

void function_with_long_double_parameter(long double ld) {
    assert_ld_string(ld, "1.00000", "Int to Long double conversion in function call");
}

void test_function_call_argument_conversions() {
    function_with_int_parameter(1.0l);
    function_with_long_double_parameter(1);
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_assignment();
    test_arithmetic();
    test_comparison_assignment();
    test_comparison_conditional_jump();
    test_jz_jnz();
    test_pointers();
    test_pointer_arithmetic();
    test_pointer_casting();
    test_structs();
    test_constant_operations();
    test_inc_dec();
    test_int_long_double_type_changes();
    test_long_double_type_int_changes();
    test_function_call_argument_conversions();

    finalize();
}
