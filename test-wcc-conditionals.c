#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

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

    finalize();
}
