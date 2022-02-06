#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

char gc;
short gs;
int gi;
long gl;

unsigned char guc;
unsigned short gus;
unsigned int gui;
unsigned long gul;

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

void test_constant_and_or_shortcutting() {
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

void test_register_memory_and_or_shortcutting() {
    char c, *pc;
    short s, *ps;
    int i, *pi;
    long l, *pl;

    unsigned char uc;
    unsigned short us;
    unsigned int ui;
    unsigned long ul;

    c = s = i = l = uc = us = ui = ul = pc = ps = pi = pl = 1;
    gc = gs = gi = gl = guc = gus = gui = gul = 1;
    assert_int(1, c   && 1, "c   && 1");
    assert_int(1, s   && 1, "s   && 1");
    assert_int(1, i   && 1, "i   && 1");
    assert_int(1, l   && 1, "l   && 1");
    assert_int(1, uc  && 1, "uc  && 1");
    assert_int(1, us  && 1, "us  && 1");
    assert_int(1, ui  && 1, "ui  && 1");
    assert_int(1, ul  && 1, "ul  && 1");
    assert_int(1, gc  && 1, "gc  && 1");
    assert_int(1, gs  && 1, "gs  && 1");
    assert_int(1, gi  && 1, "gi  && 1");
    assert_int(1, gl  && 1, "gl  && 1");
    assert_int(1, guc && 1, "guc && 1");
    assert_int(1, gus && 1, "gus && 1");
    assert_int(1, gui && 1, "gui && 1");
    assert_int(1, gul && 1, "gul && 1");
    assert_int(1, pc  && 1, "pc  && 1");
    assert_int(1, ps  && 1, "ps  && 1");
    assert_int(1, pi  && 1, "pi  && 1");
    assert_int(1, pl  && 1, "pl  && 1");

    c = s = i = l = uc = us = ui = ul = pc = ps = pi = pl = 0;
    gc = gs = gi = gl = guc = gus = gui = gul = 0;
    assert_int(1, c   || 1, "c   || 1");
    assert_int(1, s   || 1, "s   || 1");
    assert_int(1, i   || 1, "i   || 1");
    assert_int(1, l   || 1, "l   || 1");
    assert_int(1, uc  || 1, "uc  || 1");
    assert_int(1, us  || 1, "us  || 1");
    assert_int(1, ui  || 1, "ui  || 1");
    assert_int(1, ul  || 1, "ul  || 1");
    assert_int(1, gc  || 1, "gc  || 1");
    assert_int(1, gs  || 1, "gs  || 1");
    assert_int(1, gi  || 1, "gi  || 1");
    assert_int(1, gl  || 1, "gl  || 1");
    assert_int(1, guc || 1, "guc || 1");
    assert_int(1, gus || 1, "gus || 1");
    assert_int(1, gui || 1, "gui || 1");
    assert_int(1, gul || 1, "gul || 1");
    assert_int(1, pc  || 1, "pc  || 1");
    assert_int(1, ps  || 1, "ps  || 1");
    assert_int(1, pi  || 1, "pi  || 1");
    assert_int(1, pl  || 1, "pl  || 1");
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

void foo() { gi = 1; }
void bar() { gi = 2; }

void foobar(int i) {
    return i ? foo() : bar();
}

int f2() {}

void test_ternary() {
    int i;

    assert_int(1, (1 ? 1 : 2),          "ternary 1");
    assert_int(2, (0 ? 1 : 2),          "ternary 2");
    assert_int(6,  0 + 2 * (1 ? 3 : 4), "ternary 3");
    assert_int(0, strcmp("foo", 1 ? "foo" : "bar"), "ternary 4");
    assert_int(0, strcmp("bar", 0 ? "foo" : "bar"), "ternary 5");

    // Arithmetic promotion
    float f;
    double d;

    i = 0; f = i ? 1.1 : 1; assert_float(1.0, f, "ternary promotion int/float false");
    i = 1; f = i ? 1.1 : 1; assert_float(1.1, f, "ternary promotion int/float true");

    i = 0; d = i ? 1.1 : 1.2f; assert_double(1.2, d, "ternary promotion float/double false");
    i = 1; d = i ? 1.1 : 1.2f; assert_double(1.1, d, "ternary promotion float/double true");

    gi = -1; foobar(0); assert_int(2, gi, "ternary void false");
    gi = -1; foobar(1); assert_int(1, gi, "ternary void true");

    int *pi1, *pi2, *pi3;
    pi1 = 1;
    pi2 = 2;
    i = 0; pi3 = i ? pi1 : pi2; assert_long(pi2, pi3, "ternary *pi false");
    i = 1; pi3 = i ? pi1 : pi2; assert_long(pi1, pi3, "ternary *pi true");

    i = 0; pi2 = i ? pi1 : (void *) 0; assert_long(0,   pi2, "ternary *pi/0 false");
    i = 1; pi2 = i ? pi1 : (void *) 0; assert_long(pi1, pi2, "ternary *pi/0 true");

    int i2 = 42;
    int i3 = 43;
    void *pv = &i2;
    pi1 = &i3;
    i = 0; pi2 = i ? pi1 : pv; assert_long(pv,  pi2, "pi/pv false");
    i = 1; pi2 = i ? pi1 : pv; assert_long(pi1, pi2, "pi/pv true");

    // Commas in expression in between ? and :
    assert_int(3, 0 ? 1, 2 : 3, "Ternary with , between ? and : 1");
    assert_int(2, 1 ? 1, 2 : 3, "Ternary with , between ? and : 2");

    // With functions and null pointer operands
    int (*x1)() = 0 ? f2 : 0;
    int (*x2)() = 0 ? 0 : f2;
    int (*x3)() = 0 ? f2 : (void *) 0;
    int (*x4)() = 0 ? (void *) 0 : f2;
    int (*x5)() = 0 ? &f2 : 0;
    int (*x6)() = 0 ? 0 : &f2;
    int (*x7)() = 0 ? &f2 : (void *) 0;
    int (*x8)() = 0 ? (void *) 0 : &f2;
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

int test_ternary_composite_type() {
    int (*i)[3]; // A pointer to an array with 3 elements

    i = malloc(sizeof((*i)[3]));
    (*i)[0] = 1;
    (*i)[1] = 2;
    (*i)[2] = 3;

    int (*j)[]; // A pointer to an array
    j = malloc(10);

    assert_int(12, sizeof(*(1 ? i : j)), "Composite ternary ptr to array 1");
    assert_int(12, sizeof(*(1 ? j : i)), "Composite ternary ptr to array 2");
}

int test_string_literal_used_as_scalar() {
    int i = "foo" ? 1 : 2;
    assert_int(1, i, "\"foo\" in ternary");

    int ia[1];
    int j = ia ? 1 : 2;

    assert_int(1, j, "int array in ternary");
    if ("foo") assert_int(1, 1, "\"foo\" in if"); else assert_int(0, 1, "\"foo\" in if");
}

// Test bug where a temporary struct was in a register instead of on the stack
int test_structs_in_ternary() {
    struct s { long i, j; } s[4] = {{0, 0}, {1, 2}, {3, 4}};
    s[3] = s[0].i == 0 ? s[1] : s[2];
    assert_int(1, s[3].i, "Structs in ternary 1");
    assert_int(2, s[3].j, "Structs in ternary 2");

    s[0].i = 1;
    s[3] = s[0].i == 0 ? s[1] : s[2];
    assert_int(3, s[3].i, "Structs in ternary 3");
    assert_int(4, s[3].j, "Structs in ternary 4");
}

int test_long_doubles_in_ternary() {
    long double ld[4] = {0.0, 1.1, 2.2};
    ld[3] = 1 ? ld[1] : ld[2];
    assert_long_double(1.1, ld[3], "Long double in ternary 1");
    ld[3] = 0 ? ld[1] : ld[2];
    assert_long_double(2.2, ld[3], "Long double in ternary 2");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_if_else();
    test_constant_and_or_shortcutting();
    test_register_memory_and_or_shortcutting();
    test_first_arg_to_or_and_and_must_be_rvalue();
    test_ternary();
    test_ternary_cast();
    test_ternary_composite_type();
    test_string_literal_used_as_scalar();
    test_structs_in_ternary();
    test_long_doubles_in_ternary();

    finalize();
}
