#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test-lib.h"

int failures;
int passes;
int verbose;

long gl;

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
    // assert_int(1, -1U           > 0, "Constant suffix U 1a"); // TODO unsigned constant expressions
    // assert_int(1, -2147483648U  > 0, "Constant suffix U 1b"); // TODO unsigned constant expressions
    // assert_int(1, -4294967296U  > 0, "Constant suffix U 1c"); // TODO unsigned constant expressions
    // assert_int(1, -0x1U         > 0, "Constant suffix U 2a"); // TODO unsigned constant expressions
    // assert_int(1, -0x80000000U  > 0, "Constant suffix U 2b"); // TODO unsigned constant expressions
    // assert_int(1, -0x100000000U > 0, "Constant suffix U 2c"); // TODO unsigned constant expressions
    // assert_int(1, -01U         > 0,  "Constant suffix U 3a"); // TODO unsigned constant expressions

    // LL
    assert_int(8, sizeof(1LL),   "Constant suffix LL 1a");
    assert_int(8, sizeof(0x1LL), "Constant suffix LL 2a");

    // U with combinations
    // assert_int(1, -1UL > 0,  "Constant suffix UL"); // TODO unsigned constant expressions
    // assert_int(1, -1LU > 0,  "Constant suffix LU"); // TODO unsigned constant expressions
    // assert_int(1, -1ULL > 0, "Constant suffix ULL"); // TODO unsigned constant expressions
    // assert_int(1, -1LLU > 0, "Constant suffix LLU"); // TODO unsigned constant expressions

    // // LL with combinations
    assert_int(8, sizeof(1LLU),   "Constant suffix LLU 1a");
    assert_int(8, sizeof(1ULL),   "Constant suffix ULL 1a");

    // // Some lower case sanity checks
    assert_int(8, sizeof(1l),   "Constant suffix lowercase l");
    // assert_int(1, -1u > 0,      "Constant suffix lowercase u"); // TODO unsigned constant expressions
    assert_int(8, sizeof(1ll),  "Constant suffix lowercase ll");
    // assert_int(1, -1UL > 0,     "Constant suffix lowercase ul"); // TODO unsigned constant expressions
    assert_int(8, sizeof(1llu), "Constant suffix lowercase llu");
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

void test_uint_uint_comparison() {
    unsigned int i, j;

    i = -1;
    j = 0;

    assert_int(0, i < j, "i < j");
}

void func_uc(unsigned char uc, long value, char *message) { assert_long((long) uc, value, message); }
void func_us(unsigned short us, long value, char *message) { assert_long((long) us, value, message); }
void func_ui(unsigned int ui, long value, char *message) { assert_long((long) ui, value, message); }
void func_ul(unsigned long ul, long value, char *message) { assert_long((long) ul, value, message); }

void test_uint_uint_assignment() {
    unsigned char uc1, uc2;
    unsigned int ui1, ui2;
    unsigned short us1, us2;
    unsigned long ul1, ul2;

    uc1 = us1 = ui1 = ul1 = 1;

    // Test self assignments by piggybacking on function register assignment
    // Otherwise, coalescing registers may get in the way and the rule isn't tested.
    // func_uc(uc1, 1, "uc = uc");
    func_us(us1, 1, "us = us");
    func_ui(ui1, 1, "ui = ui");
    func_ul(ul1, 1, "ul = ul");

                func_uc(uc1, 1, "uc = uc");
    uc1 = us1;  func_uc(uc1, 1, "uc = us");
    uc1 = ui1;  func_uc(uc1, 1, "uc = ui");
    uc1 = ul1;  func_uc(uc1, 1, "uc = ul");

    us1 = uc1;  func_us(us1, 1, "us = uc");
                func_us(us1, 1, "us = us");
    us1 = ui1;  func_us(us1, 1, "us = ui");
    us1 = ul1;  func_us(us1, 1, "us = ul");

    ui1 = uc1;  func_ui(ui1, 1, "ui = uc");
    ui1 = us1;  func_ui(ui1, 1, "ui = us");
                func_ui(ui1, 1, "ui = ui");
    ui1 = ul1;  func_ui(ui1, 1, "ui = ul");

    ul1 = uc1;  func_ul(ul1, 1, "ul = uc");
    ul1 = us1;  func_ul(ul1, 1, "ul = us");
    ul1 = ul1;  func_ul(ul1, 1, "ul = ui");
                func_ul(ul1, 1, "ul = ul");

    func_uc(uc1, 1, "func_uc(uc)");
    func_uc(us1, 1, "func_uc(us)");
    func_uc(ui1, 1, "func_uc(ui)");
    func_uc(ul1, 1, "func_uc(ul)");

    func_us(uc1, 1, "func_us(uc)");
    func_us(us1, 1, "func_us(us)");
    func_us(ui1, 1, "func_us(ui)");
    func_us(ul1, 1, "func_us(ul)");

    func_ui(uc1, 1, "func_ui(uc)");
    func_ui(us1, 1, "func_ui(us)");
    func_ui(ui1, 1, "func_ui(ui)");
    func_ui(ul1, 1, "func_ui(ul)");

    func_ul(uc1, 1, "func_ul(uc)");
    func_ul(us1, 1, "func_ul(us)");
    func_ul(ui1, 1, "func_ul(ui)");
    func_ul(ul1, 1, "func_ul(ul)");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_integer_constant_sizes();
    test_hex_and_octal_constants();
    test_constant_suffixes();
    test_hex_and_octal_constants();
    test_pointer_casting_reads();
    test_uint_uint_comparison();
    test_uint_uint_assignment();

    finalize();
}
