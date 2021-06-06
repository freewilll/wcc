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

    finalize();
}
