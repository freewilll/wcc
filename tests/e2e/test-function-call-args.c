#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

long double gld;

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

int f(int i, int j) {
    return i + 2 * j;
}

void test_direct_register_use() {
    int i;

    // Amount of args per call that can use an arg register (rdi, rsi, ...)
    i = f(1, 1);             assert_int(3,  i, "direct register use 1"); // 2
    i = f(f(1, 2), 1);       assert_int(7,  i, "direct register use 2"); // 1 2
    i = f(1, f(1, 2));       assert_int(11, i, "direct register use 3"); // 2 2
    i = f(1, f(1, f(1, 3))); assert_int(31, i, "direct register use 4"); // 2 2 2
    i = f(f(1, f(1, 3)), 1); assert_int(17, i, "direct register use 5"); // 1 2 2
    i = f(f(f(1, 3), 1), 1); assert_int(11, i, "direct register use 6"); // 1 1 2
}

void test_pushed_param_c(int a1, int a2, int a3, int a4, int a5, int a6, char c) {
    assert_int(1, c, "Sign extension of pushed char param");
}

void test_pushed_param_s(int a1, int a2, int a3, int a4, int a5, int a6, short s) {
    assert_int(1, s, "Sign extension of pushed char param");
}

void test_pushed_param_i(int a1, int a2, int a3, int a4, int a5, int a6, int i) {
    assert_int(1, i, "Sign extension of pushed char param");
}

void test_pushed_param_l(int a1, int a2, int a3, int a4, int a5, int a6, long l) {
    assert_int(1, l, "Sign extension of pushed char param");
}

void test_pushed_param_uc(int a1, int a2, int a3, int a4, int a5, int a6, unsigned char uc) {
    assert_int(1, uc, "Sign extension of pushed unsigned char param");
}

void test_pushed_param_us(int a1, int a2, int a3, int a4, int a5, int a6, unsigned short us) {
    assert_int(1, us, "Sign extension of pushed unsigned char param");
}

void test_pushed_param_ui(int a1, int a2, int a3, int a4, int a5, int a6, unsigned int ui) {
    assert_int(1, ui, "Sign extension of pushed unsigned char param");
}

void test_pushed_param_ul(int a1, int a2, int a3, int a4, int a5, int a6, unsigned long ul) {
    assert_int(1, ul, "Sign extension of pushed unsigned char param");
}

void test_sign_extension_pushed_params() {
    char c;
    short s;
    int i;
    long l;

    unsigned char uc;
    unsigned short us;
    unsigned int ui;
    unsigned long ul;

    c = s = i = l = uc = us = ui = ul = 1;
    test_pushed_param_c(0, 0, 0, 0, 0, 0, c);
    test_pushed_param_s(0, 0, 0, 0, 0, 0, s);
    test_pushed_param_i(0, 0, 0, 0, 0, 0, i);
    test_pushed_param_l(0, 0, 0, 0, 0, 0, l);
    test_pushed_param_uc(0, 0, 0, 0, 0, 0, uc);
    test_pushed_param_us(0, 0, 0, 0, 0, 0, us);
    test_pushed_param_ui(0, 0, 0, 0, 0, 0, ui);
    test_pushed_param_ul(0, 0, 0, 0, 0, 0, ul);
}

// These tests stack layout is correct from an ABI point of view by checking
// convoluted combinations of ints and double longs beyond the 6 single-register-arg limit
void test_long_double_stack_zero_offset() {
    char *buffer;
    long double ld = 1.3;
    gld = 1.4;

    buffer = malloc(100);

    sprintf(buffer, "%5.5Lf %5.5Lf", 1.1l, 1.2l);
    assert_int(0, strcmp(buffer, "1.10000 1.20000"), "Long double zero sprintf cst");

    sprintf(buffer, "%5.5Lf %5.5Lf", ld, gld);
    assert_int(0, strcmp(buffer, "1.30000 1.40000"), "Long double zero sprintf ld & gld");

    sprintf(buffer, "%5.5Lf %d %d %d %d %d %d", 1.1l, 1, 2, 3, 4, 5, 6);
    assert_int(0, strcmp(buffer, "1.10000 1 2 3 4 5 6"), "Long double zero sprintf 1");

    sprintf(buffer, "%5.5Lf %d %d %d %d %d %d %Lf", 1.1l, 1, 2, 3, 4, 5, 6, 1.2l);
    assert_int(0, strcmp(buffer, "1.10000 1 2 3 4 5 6 1.200000"), "Long double zero sprintf 2");

    sprintf(buffer, "%5.5Lf %d %d %d %d %d %d %d %Lf", 1.1l, 1, 2, 3, 4, 5, 6, 7, 1.2l);
    assert_int(0, strcmp(buffer, "1.10000 1 2 3 4 5 6 7 1.200000"), "Long double zero sprintf 3");

    sprintf(buffer, "%5.5Lf %d %d %d %d %d %d %d %Lf %d", 1.1l, 1, 2, 3, 4, 5, 6, 7, 1.2l, 1);
    assert_int(0, strcmp(buffer, "1.10000 1 2 3 4 5 6 7 1.200000 1"), "Long double zero sprintf 4");
}

void test_long_double_stack_eight_offset() {
    char *buffer;
    long double ld = 1.3;

    // Add something to the stack to ensure the tests pass on a stack with an extra
    // offset of 8 bytes.
    int i, *j;
    j = &i; // This forces i onto the stack, allocating 8 bytes.

    gld = 1.4;

    buffer = malloc(100);

    sprintf(buffer, "%5.5Lf %5.5Lf", 1.1l, 1.2l);
    assert_int(0, strcmp(buffer, "1.10000 1.20000"), "Long double eight sprintf cst");

    sprintf(buffer, "%5.5Lf %5.5Lf", ld, gld);
    assert_int(0, strcmp(buffer, "1.30000 1.40000"), "Long double eight sprintf ld & gld");

    sprintf(buffer, "%5.5Lf %d %d %d %d %d %d", 1.1l, 1, 2, 3, 4, 5, 6);
    assert_int(0, strcmp(buffer, "1.10000 1 2 3 4 5 6"), "Long double eight sprintf 1");

    sprintf(buffer, "%5.5Lf %d %d %d %d %d %d %Lf", 1.1l, 1, 2, 3, 4, 5, 6, 1.2l);
    assert_int(0, strcmp(buffer, "1.10000 1 2 3 4 5 6 1.200000"), "Long double eight sprintf 2");

    sprintf(buffer, "%5.5Lf %d %d %d %d %d %d %d %Lf", 1.1l, 1, 2, 3, 4, 5, 6, 7, 1.2l);
    assert_int(0, strcmp(buffer, "1.10000 1 2 3 4 5 6 7 1.200000"), "Long double eight sprintf 3");

    sprintf(buffer, "%5.5Lf %d %d %d %d %d %d %d %Lf %d", 1.1l, 1, 2, 3, 4, 5, 6, 7, 1.2l, 1);
    assert_int(0, strcmp(buffer, "1.10000 1 2 3 4 5 6 7 1.200000 1"), "Long double eight sprintf 4");
}

char *long_double_pushed_args_i_ld(int i, long double ld) {
    char *buffer = malloc(100);
    sprintf(buffer, "%d %5.5Lf", i, ld);
    return buffer;
}

char *long_double_pushed_args_i_ld_rep4(int i1, long double ld1, int i2, long double ld2, int i3, long double ld3, int i4, long double ld4) {
    char *buffer = malloc(100);
    sprintf(buffer, "%d %5.5Lf %d %5.5Lf %d %5.5Lf %d %5.5Lf", i1, ld1, i2, ld2, i3, ld3, i4, ld4);
    return buffer;
}

char *long_double_pushed_args_ld_i_rep4(long double ld1, int i1, long double ld2, int i2, long double ld3, int i3, long double ld4, int i4) {
    char *buffer = malloc(100);
    sprintf(buffer, "%5.5Lf %d %5.5Lf %d %5.5Lf %d %5.5Lf %d", ld1, i1, ld2, i2, ld3, i3, ld4, i4);
    return buffer;
}

char *long_double_pushed_args_6i_ld(int i1, int i2, int i3, int i4, int i5, int i6, long double ld) {
    char *buffer = malloc(100);
    sprintf(buffer, "%d %d %d %d %d %d %5.5Lf", i1, i2, i3, i4, i5, i6, ld);
    return buffer;
}

char *long_double_pushed_args_7i_ld(int i1, int i2, int i3, int i4, int i5, int i6, int i7, long double ld) {
    char *buffer = malloc(100);
    sprintf(buffer, "%d %d %d %d %d %d %d %5.5Lf", i1, i2, i3, i4, i5, i6, i7, ld);
    return buffer;
}

char *long_double_pushed_args_6i_2pc_1i(int i1, int i2, int i3, int i4, int i5, char *pc1, char *pc2, int i6) {
    char *buffer = malloc(100);
    sprintf(buffer, "%d %d %d %d %d %s %s %d", i1, i2, i3, i4, i5, pc1, pc2, i6);
    return buffer;
}

char *long_double_pushed_args_6ld_6i(
        long double ld1, long double ld2, long double ld3, long double ld4, long double ld5, long double ld6,
        int i1, int i2, int i3, int i4, int i5, int i6) {

    char *buffer = malloc(100);
    sprintf(buffer, "%5.5Lf %5.5Lf %5.5Lf %5.5Lf %5.5Lf %5.5Lf %d %d %d %d %d %d", ld1, ld2, ld3, ld4, ld5, ld6, i1, i2, i3, i4, i5, i6);
    return buffer;
}

void test_long_double_pushed_params() {
    char *buffer;

    buffer = long_double_pushed_args_i_ld(1, 1.1L);
    assert_int(0, strcmp(buffer, "1 1.10000"), "Long double call args i, ld");

    buffer = long_double_pushed_args_i_ld_rep4(1, 1.1L, 2, 2.1L, 3, 3.1L, 4, 4.1L);
    assert_int(0, strcmp(buffer, "1 1.10000 2 2.10000 3 3.10000 4 4.10000"), "Long double call args i, ld rep4");

    buffer = long_double_pushed_args_ld_i_rep4(1.1L, 1, 2.1L, 2, 3.1L, 3, 4.1L, 4);
    assert_int(0, strcmp(buffer, "1.10000 1 2.10000 2 3.10000 3 4.10000 4"), "Long double call args ld, i rep4");

    buffer = long_double_pushed_args_6i_ld(1, 2, 3, 4, 5, 6, 1.1L);
    assert_int(0, strcmp(buffer, "1 2 3 4 5 6 1.10000"), "Long double call args 6i, ld");

    buffer = long_double_pushed_args_7i_ld(1, 2, 3, 4, 5, 6, 7, 1.1L);
    assert_int(0, strcmp(buffer, "1 2 3 4 5 6 7 1.10000"), "Long double call args 7i, ld");

    buffer = long_double_pushed_args_6i_2pc_1i(1, 2, 3, 4, 5, "foo", "bar", 6);
    assert_int(0, strcmp(buffer, "1 2 3 4 5 foo bar 6"), "Long double call args 6i 2pc 1i");

    buffer = long_double_pushed_args_6ld_6i(1.1L, 2.1L, 3.1L, 4.1L, 5.1L, 6.1L, 1, 2, 3, 4, 5, 6);
    assert_int(0, strcmp(buffer, "1.10000 2.10000 3.10000 4.10000 5.10000 6.10000 1 2 3 4 5 6"), "Long double call args 6ld 6i");
}

long double cst1() {
    return 1.1L;
}

long double cst2() {
    return 1.2L;
}

long double local() {
    long double ld = 1.3;
    return ld;
}

long double global() {
    gld = 1.4;
    return gld;
}

void test_long_double_function_call_return_value() {
    long double ld1;
    char *buffer;

    ld1 = cst1();
    buffer = malloc(100);
    sprintf(buffer, "%5.5Lf", ld1);
    assert_int(0, strcmp(buffer, "1.10000"), "Long double assignment from function call");

    gld = cst1();
    buffer = malloc(100);
    sprintf(buffer, "%5.5Lf", gld);
    assert_int(0, strcmp(buffer, "1.10000"), "Long double assignment from function call");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    fca0();
    fca1(1);
    fca2(1, 2);
    fca3(1, 2, 3);
    fca4(1, 2, 3, 4);
    fca5(1, 2, 3, 4, 5);
    fca6(1, 2, 3, 4, 5, 6);
    fca7(1, 2, 3, 4, 5, 6, 7);
    fca8(1, 2, 3, 4, 5, 6, 7, 8);

    test_direct_register_use();
    test_sign_extension_pushed_params();
    test_long_double_stack_zero_offset();
    test_long_double_stack_eight_offset();
    test_long_double_pushed_params();
    test_long_double_function_call_return_value();

    finalize();
}
