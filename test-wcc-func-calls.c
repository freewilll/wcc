#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "test-lib.h"

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

static char  return_char_from_char()  { char  v; v = 1; return v; }
static char  return_char_from_short() { short v; v = 1; return v; }
static char  return_char_from_int()   { int   v; v = 1; return v; }
static char  return_char_from_long()  { long  v; v = 1; return v; }
static short return_short_from_char()  { char  v; v = 1; return v; }
static short return_short_from_short() { short v; v = 1; return v; }
static short return_short_from_int()   { int   v; v = 1; return v; }
static short return_short_from_long()  { long  v; v = 1; return v; }
static int   return_int_from_char()  { char  v; v = 1; return v; }
static int   return_int_from_short() { short v; v = 1; return v; }
static int   return_int_from_int()   { int   v; v = 1; return v; }
static int   return_int_from_long()  { long  v; v = 1; return v; }
static long  return_long_from_char()  { char  v; v = 1; return v; }
static long  return_long_from_short() { short v; v = 1; return v; }
static long  return_long_from_int()   { int   v; v = 1; return v; }
static long  return_long_from_long()  { long  v; v = 1; return v; }

static void test_return_mixed_ints() {
    assert_int(1, return_char_from_char(),   "return char from char");
    assert_int(1, return_char_from_short(),  "return char from short");
    assert_int(1, return_char_from_int(),    "return char from int");
    assert_int(1, return_char_from_long(),   "return char from long");
    assert_int(1, return_short_from_char(),  "return short from char");
    assert_int(1, return_short_from_short(), "return short from short");
    assert_int(1, return_short_from_int(),   "return short from int");
    assert_int(1, return_short_from_long(),  "return short from long");
    assert_int(1, return_int_from_char(),    "return int from char");
    assert_int(1, return_int_from_short(),   "return int from short");
    assert_int(1, return_int_from_int(),     "return int from int");
    assert_int(1, return_int_from_long(),    "return int from long");
    assert_long(1, return_long_from_char(),  "return long from char");
    assert_long(1, return_long_from_short(), "return long from short");
    assert_long(1, return_long_from_int(),   "return long from int");
    assert_long(1, return_long_from_long(),  "return long from long");
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
    test_return_mixed_ints();
    test_split_function_declaration_and_definition();
    test_void_return();
    test_func_returns_are_lvalues();
    test_open_read_write_close();

    finalize();
}
