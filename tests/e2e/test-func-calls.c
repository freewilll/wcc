#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

int g;

int nfc(int i) { return i + 1; }

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

static char  return_c_from_c() { return (char)  -1; }
static char  return_c_from_s() { return (short) -1; }
static char  return_c_from_i() { return (int)   -1; }
static char  return_c_from_l() { return (long)  -1; }
static short return_s_from_c() { return (char)  -1; }
static short return_s_from_s() { return (short) -1; }
static short return_s_from_i() { return (int)   -1; }
static short return_s_from_l() { return (long)  -1; }
static int   return_i_from_c() { return (char)  -1; }
static int   return_i_from_s() { return (short) -1; }
static int   return_i_from_i() { return (int)   -1; }
static int   return_i_from_l() { return (long)  -1; }
static long  return_l_from_c() { return (char)  -1; }
static long  return_l_from_s() { return (short) -1; }
static long  return_l_from_i() { return (int)   -1; }
static long  return_l_from_l() { return (long)  -1; }

static unsigned char  return_uc_from_c() { return (char)  -1; }
static unsigned char  return_uc_from_s() { return (short) -1; }
static unsigned char  return_uc_from_i() { return (int)   -1; }
static unsigned char  return_uc_from_l() { return (long)  -1; }
static unsigned short return_us_from_c() { return (char)  -1; }
static unsigned short return_us_from_s() { return (short) -1; }
static unsigned short return_us_from_i() { return (int)   -1; }
static unsigned short return_us_from_l() { return (long)  -1; }
static unsigned int   return_ui_from_c() { return (char)  -1; }
static unsigned int   return_ui_from_s() { return (short) -1; }
static unsigned int   return_ui_from_i() { return (int)   -1; }
static unsigned int   return_ui_from_l() { return (long)  -1; }
static unsigned long  return_ul_from_c() { return (char)  -1; }
static unsigned long  return_ul_from_s() { return (short) -1; }
static unsigned long  return_ul_from_i() { return (int)   -1; }
static unsigned long  return_ul_from_l() { return (long)  -1; }

static unsigned char  return_uc_from_uc() { return (unsigned char)  -1; }
static unsigned char  return_uc_from_us() { return (unsigned short) -1; }
static unsigned char  return_uc_from_ui() { return (unsigned int)   -1; }
static unsigned char  return_uc_from_ul() { return (unsigned long)  -1; }
static unsigned short return_us_from_uc() { return (unsigned char)  -1; }
static unsigned short return_us_from_us() { return (unsigned short) -1; }
static unsigned short return_us_from_ui() { return (unsigned int)   -1; }
static unsigned short return_us_from_ul() { return (unsigned long)  -1; }
static unsigned int   return_ui_from_uc() { return (unsigned char)  -1; }
static unsigned int   return_ui_from_us() { return (unsigned short) -1; }
static unsigned int   return_ui_from_ui() { return (unsigned int)   -1; }
static unsigned int   return_ui_from_ul() { return (unsigned long)  -1; }
static unsigned long  return_ul_from_uc() { return (unsigned char)  -1; }
static unsigned long  return_ul_from_us() { return (unsigned short) -1; }
static unsigned long  return_ul_from_ui() { return (unsigned int)   -1; }
static unsigned long  return_ul_from_ul() { return (unsigned long)  -1; }

static char  return_c_from_uc() { return (unsigned char)  -1; }
static char  return_c_from_us() { return (unsigned short) -1; }
static char  return_c_from_ui() { return (unsigned int)   -1; }
static char  return_c_from_ul() { return (unsigned long)  -1; }
static short return_s_from_uc() { return (unsigned char)  -1; }
static short return_s_from_us() { return (unsigned short) -1; }
static short return_s_from_ui() { return (unsigned int)   -1; }
static short return_s_from_ul() { return (unsigned long)  -1; }
static int   return_i_from_uc() { return (unsigned char)  -1; }
static int   return_i_from_us() { return (unsigned short) -1; }
static int   return_i_from_ui() { return (unsigned int)   -1; }
static int   return_i_from_ul() { return (unsigned long)  -1; }
static long  return_l_from_uc() { return (unsigned char)  -1; }
static long  return_l_from_us() { return (unsigned short) -1; }
static long  return_l_from_ui() { return (unsigned int)   -1; }
static long  return_l_from_ul() { return (unsigned long)  -1; }

static void test_return_mixed_integers() {
    // Test conversions when returning integers

    assert_int (-1, return_c_from_c(), "return c from c");
    assert_int (-1, return_c_from_s(), "return c from s");
    assert_int (-1, return_c_from_i(), "return c from i");
    assert_int (-1, return_c_from_l(), "return c from l");
    assert_int (-1, return_s_from_c(), "return s from c");
    assert_int (-1, return_s_from_s(), "return s from s");
    assert_int (-1, return_s_from_i(), "return s from i");
    assert_int (-1, return_s_from_l(), "return s from l");
    assert_int (-1, return_i_from_c(), "return i from c");
    assert_int (-1, return_i_from_s(), "return i from s");
    assert_int (-1, return_i_from_i(), "return i from i");
    assert_int (-1, return_i_from_l(), "return i from l");
    assert_long(-1, return_l_from_c(), "return l from c");
    assert_long(-1, return_l_from_s(), "return l from s");
    assert_long(-1, return_l_from_i(), "return l from i");
    assert_long(-1, return_l_from_l(), "return l from l");

    assert_int (0xff,       return_uc_from_c(), "return uc from c");
    assert_int (0xff,       return_uc_from_s(), "return uc from s");
    assert_int (0xff,       return_uc_from_i(), "return uc from i");
    assert_int (0xff,       return_uc_from_l(), "return uc from l");
    assert_int (0xffff,     return_us_from_c(), "return us from c");
    assert_int (0xffff,     return_us_from_s(), "return us from s");
    assert_int (0xffff,     return_us_from_i(), "return us from i");
    assert_int (0xffff,     return_us_from_l(), "return us from l");
    assert_int (-1,         return_ui_from_c(), "return ui from c");
    assert_int (-1,         return_ui_from_s(), "return ui from s");
    assert_int (-1,         return_ui_from_i(), "return ui from i");
    assert_int (-1,         return_ui_from_l(), "return ui from l");
    assert_long(-1,         return_ul_from_c(), "return ul from c");
    assert_long(-1,         return_ul_from_s(), "return ul from s");
    assert_long(-1,         return_ul_from_i(), "return ul from i");
    assert_long(-1,         return_ul_from_l(), "return ul from l");

    assert_int (0xff,       return_uc_from_uc(), "return uc from uc");
    assert_int (0xff,       return_uc_from_us(), "return uc from us");
    assert_int (0xff,       return_uc_from_ui(), "return uc from ui");
    assert_int (0xff,       return_uc_from_ul(), "return uc from ul");
    assert_int (0xff,       return_us_from_uc(), "return us from uc");
    assert_int (0xffff,     return_us_from_us(), "return us from us");
    assert_int (0xffff,     return_us_from_ui(), "return us from ui");
    assert_int (0xffff,     return_us_from_ul(), "return us from ul");
    assert_int (0xff,       return_ui_from_uc(), "return ui from uc");
    assert_int (0xffff,     return_ui_from_us(), "return ui from us");
    assert_int (-1,         return_ui_from_ui(), "return ui from ui");
    assert_int (-1,         return_ui_from_ul(), "return ui from ul");
    assert_long(0xff,       return_ul_from_uc(), "return ul from uc");
    assert_long(0xffff,     return_ul_from_us(), "return ul from us");
    assert_long(0xffffffff, return_ul_from_ui(), "return ul from ui");
    assert_long(-1,         return_ul_from_ul(), "return ul from ul");

    assert_int (-1,         return_c_from_uc(), "return c from uc");
    assert_int (-1,         return_c_from_us(), "return c from us");
    assert_int (-1,         return_c_from_ui(), "return c from ui");
    assert_int (-1,         return_c_from_ul(), "return c from ul");
    assert_int (0xff,       return_s_from_uc(), "return s from uc");
    assert_int (-1,         return_s_from_us(), "return s from us");
    assert_int (-1,         return_s_from_ui(), "return s from ui");
    assert_int (-1,         return_s_from_ul(), "return s from ul");
    assert_int (0xff,       return_i_from_uc(), "return i from uc");
    assert_int (0xffff,     return_i_from_us(), "return i from us");
    assert_int (-1,         return_i_from_ui(), "return i from ui");
    assert_int (-1,         return_i_from_ul(), "return i from ul");
    assert_long(0xff,       return_l_from_uc(), "return l from uc");
    assert_long(0xffff,     return_l_from_us(), "return l from us");
    assert_long(0xffffffff, return_l_from_ui(), "return l from ui");
    assert_long(-1,         return_l_from_ul(), "return l from ul");
}

void test_void_return() {
    g = 0;
    vrf();
    assert_int(g, 1, "void return");
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

int (*null_global_func_ptr)(int);

int plus();
int (*global_func_ptr)(int) = plus;
int (*global_func_ptr2)(int) = &plus;

int          plus(int x)  { return x + 1; }
unsigned int uplus(int x)  { return x + 1; }
int          minus(int x) { return x - 1; }
float        fplus(int i) { return i + 1.1; }
double       dplus(int i) { return i + 1.1; }
long double  ldplus(int i) { return i + 1.1; }

void vplus(int *i) { (*i)++; }
void *pvplus(int *i) { (*i)++; }

int do_operation(int callback(int), int value) {
    if (callback)
        return callback(value);
    else
        return 0;
}

int do_operation_with_ptr(int (*callback)(int), int value) {
    if (callback)
        return callback(value);
    else
        return 0;
}

struct struct_with_assert_int {
    void (*assert_int)(int, int, char *);
};

void inc_x(int *x) {
    (*x)++;
}

void test_function_pointers() {
    assert_int(2, global_func_ptr(1),  "Global function pointer init 1");
    assert_int(2, global_func_ptr2(1), "Global function pointer init 2");
    global_func_ptr = minus;
    assert_int(1, global_func_ptr(2), "Global function pointer assign 1");
    global_func_ptr = &plus;
    assert_int(2, global_func_ptr(1), "Global function pointer assign 2");

    int (*func_ptr)(int) = plus;
    assert_int(2, func_ptr(1), "Function pointer init");
    func_ptr = minus;
    assert_int(1, func_ptr(2), "Function pointer assign 1");
    func_ptr = &plus;
    assert_int(2, func_ptr(1), "Function pointer assign 2");

    assert_int(2, (*func_ptr)(1), "Function pointer call through pointer");
    assert_int(2, (func_ptr)(1), "Function pointer call through (ptr)");
    assert_int(2, (plus)(1), "Function pointer call through (func)");

    assert_int(2, do_operation(plus,       1), "Passing function to a function accepting function 1");
    assert_int(1, do_operation(minus,      2), "Passing function to a function accepting function 2");
    assert_int(1, do_operation(&minus,     2), "Passing ptr to function to a function accepting function 3");

    assert_int(2, do_operation_with_ptr(plus,       1), "Passing function to a function accepting ptr to function 1");
    assert_int(1, do_operation_with_ptr(minus,      2), "Passing function to a function accepting ptr to function 2");
    assert_int(1, do_operation_with_ptr(&minus,     2), "Passing ptr to function to a function accepting ptr to function 3");

    assert_int(2, do_operation_with_ptr(plus,       1), "Passing function to a function accepting ptr to function 1");
    assert_int(1, do_operation_with_ptr(minus,      2), "Passing function to a function accepting ptr to function 2");
    assert_int(1, do_operation_with_ptr(&minus,     2), "Passing function to a function accepting ptr to function 3");

    assert_int(0, do_operation_with_ptr(0,          0), "Passing zero function to a function 1");
    assert_int(0, do_operation_with_ptr((void *) 0, 0), "Passing zero function to a function 2");

    assert_int(1, null_global_func_ptr == 0, "Null global pointer 1");
    assert_int(1, global_func_ptr != 0,      "Null global pointer 2");
    global_func_ptr = 0;
    assert_int(1, global_func_ptr == 0,      "Null global pointer 3");

    int (*func_ptr2)(int) = 0;
    assert_int(1, func_ptr2 == 0, "Null pointer 1");
    func_ptr2 = plus;
    assert_int(1, func_ptr2 != 0, "Null pointer 2");
    func_ptr2 = 0;
    assert_int(1, func_ptr2 == 0, "Null pointer 3");

    // Copies
    func_ptr2 = func_ptr;
    assert_int(2, (func_ptr)(1), "Function pointer copy 1");

    func_ptr2 = global_func_ptr;
    assert_int(2, (func_ptr)(1), "Function pointer copy 2");

    global_func_ptr = func_ptr;
    assert_int(2, (global_func_ptr)(1), "Function pointer copy 3");

    global_func_ptr2 = global_func_ptr;
    assert_int(2, (global_func_ptr2)(1), "Function pointer copy 4");

    // Function call through a struct member
    struct s {
        int (*func_ptr)(int);
    };

    struct s sv;
    sv.func_ptr = plus;
    assert_int(2, sv.func_ptr(1), "Func pointer in a struct 1");

    struct s *ps = &sv;

    func_ptr2 = ps->func_ptr;
    assert_int(2, func_ptr2(1), "Func pointer in a struct 2");

    assert_int(2, ps->func_ptr(1), "Func pointer in a struct 3");

    // Test declaration with multiple types
    int (*func_ptr3)(int), another_int;

    // Some bizarre assignments for completeness
    char  c = func_ptr; unsigned char  uc = func_ptr;
    int   i = func_ptr; unsigned int   ui = func_ptr;
    short s = func_ptr; unsigned short us = func_ptr;
    long  l = func_ptr; unsigned long  ul = func_ptr;
    char *pc = func_ptr;
    short *psh = func_ptr;
    int *pi = func_ptr;
    long *pl = func_ptr;
    void *pv = func_ptr;

    // With different return values
    unsigned int (*pfu)(int) = uplus;
    assert_int(2, pfu(1), "Func pointer returning unsigned int");

    i = 1;
    void (*pfv)(int *) = vplus;
    pfv(&i);
    assert_int(2, i, "Func pointer returning void that increments int *");

    i = 1;
    void (*pfpv)(int *) = pvplus;
    pfpv(&i);
    assert_int(2, i, "Func pointer returning void * that increments int *");

    float (*pff)(int) = fplus;
    assert_float(2.1, pff(1), "Func pointer returning float");

    double (*pfd)(int) = dplus;
    assert_double(2.1, pfd(1), "Func pointer returning double");

    long double (*pfld)(int) = ldplus;
    assert_long_double(2.1, pfld(1), "Func pointer returning long double");

    // Test assigning a pointer to a function to a pointer to a pointer to a function
    struct struct_with_assert_int *swai = malloc(sizeof(struct struct_with_assert_int));
    swai->assert_int = assert_int;
    swai->assert_int(1,1, "s->func = func");
    swai->assert_int = &assert_int;
    swai->assert_int(1,1, "s->func = &func");

    // A ternary without &
    i = 1;
    int (*a)(int);

    i = 0; a = i ? plus : minus; assert_int(2, a(3), "Termary assignment without & 1");
    i = 1; a = i ? plus : minus; assert_int(4, a(3), "Termary assignment without & 2");

    // Declaration of a function inside a function
    void inc_x();
    i = 1;
    inc_x(&i);
    assert_int(2, i, "Declaration of a function inside a function");

}

void test_sizeof_function_pointer() {
    int (*func_ptr)(int) = plus;

    assert_int(1, sizeof(*func_ptr), "sizeof(*func_ptr)");
    assert_int(8, sizeof(func_ptr),  "sizeof(func_ptr)");
    assert_int(1, sizeof(plus),      "sizeof(plus)");
    assert_int(8, sizeof(&plus),     "sizeof(&plus)");
    assert_int(1, sizeof(*&plus),    "sizeof(*&plus)");
    assert_int(1, sizeof(**&plus),   "sizeof(**&plus)");
    assert_int(1, sizeof(***&plus),  "sizeof(***&plus)");
}

void *vpplus(int *i) { (*i)++; }
void *cpplus(int *i) { (*i)++; }

int (*gfp)(int);

int test_function_pointer_comparisons() {
    int (*fp1)(int);
    int (*fp2)(int);

    // glob-glob
    assert_int(0, vpplus == cpplus, "glob-glob == 1");
    assert_int(1, vpplus == vpplus, "glob-glob == 2");

    // reg-reg
    fp1 = vpplus;
    fp2 = vpplus; assert_int(1, fp1 == fp2, "reg-reg == 1");
    fp2 = cpplus; assert_int(0, fp1 == fp2, "reg-reg == 2");

    // reg-ptr in glob
    fp1 = vpplus;
    gfp = vpplus; assert_int(1, fp1 == gfp, "reg-ptr in glob 1");
    gfp = cpplus; assert_int(0, fp1 == gfp, "reg-ptr in glob 1");

    // reg-&glob
    fp1 = vpplus;
    if (fp1 == &vpplus) printf("equal\n"); else printf("not equal\n");
    assert_int(1, fp1 == &vpplus, "reg-&glob");
    assert_int(0, fp1 == &cpplus, "reg-&glob");

    // reg-glob
    fp1 = vpplus;
    assert_int(1, fp1 == vpplus, "reg-glob");
    assert_int(0, fp1 == cpplus, "reg-glob");

    // glob-reg
    fp1 = vpplus;
    assert_int(1, vpplus == fp1, "glob-reg");
    assert_int(0, cpplus == fp1, "glob-reg");
}

implicit_int_foo() { return 1; }
*implicit_pint_foo() { int *pi = malloc(sizeof(int)); *pi = 1; return pi; }

int test_implicit_ints_in_globals() {
    assert_int(1, implicit_int_foo(), "implicit_int_foo()");
    assert_int(1, *implicit_pint_foo(), "*implicit_pint_foo()");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    assert_int(1, nfc(0),                      "nested function calls 1");
    assert_int(2, nfc(1),                      "nested function calls 2");
    assert_int(3, nfc(nfc(1)),                 "nested function calls 3");
    assert_int(4, nfc(nfc(nfc(1))),            "nested function calls 4");
    assert_int(5, nfc(nfc(1) + nfc(1)),        "nested function calls 5");
    assert_int(6, nfc(nfc(1) + nfc(1))+nfc(0), "nested function calls 6");

    test_function_call_with_global();
    test_return_mixed_integers();
    test_split_function_declaration_and_definition();
    test_void_return();
    test_func_returns_are_lvalues();
    test_open_read_write_close();
    test_function_pointers();
    test_sizeof_function_pointer();
    test_function_pointer_comparisons();
    test_implicit_ints_in_globals();

    finalize();
}
