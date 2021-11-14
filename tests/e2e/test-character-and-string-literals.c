#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

static void test_char_literal_lexer() {
    assert_int(0,  '\0', "Char literal escapes \\0");
    assert_int(39, '\'', "Char literal escapes \\'");
    assert_int(34, '\"', "Char literal escapes \\\"");
    assert_int(63, '\?', "Char literal escapes \\?");
    assert_int(92, '\\', "Char literal escapes \\\\");
    assert_int( 7, '\a', "Char literal escapes \\a");
    assert_int( 8, '\b', "Char literal escapes \\b");
    assert_int(12, '\f', "Char literal escapes \\f");
    assert_int(10, '\n', "Char literal escapes \\n");
    assert_int(13, '\r', "Char literal escapes \\r");
    assert_int( 9, '\t', "Char literal escapes \\t");
    assert_int(11, '\v', "Char literal escapes \\v");

    // Truncation of last 32 bits
    assert_int(97,         'a',      "Multiple chars in char literal 1");
    assert_int(24930,      'ab',     "Multiple chars in char literal 2");
    assert_int(6382179,    'abc',    "Multiple chars in char literal 3");
    assert_int(16706,      'AB',     "Multiple chars in char literal 4");
    assert_int(4276803,    'ABC',    "Multiple chars in char literal 5");
    assert_int(1094861636, 'ABCD',   "Multiple chars in char literal 6");
    assert_int(1111704645, 'ABCDE',  "Multiple chars in char literal 7");
    assert_int(1128547654, 'ABCDEF', "Multiple chars in char literal 8");
    assert_int(1145390663, 'ABCDEFG',"Multiple chars in char literal 9");

    // Hex escapes
    assert_int(0,    '\x0',        "Char literal hex 1");
    assert_int(1,    '\x1',        "Char literal hex 2");
    assert_int(0,    '\x00',       "Char literal hex 3");
    assert_int(1,    '\x01',       "Char literal hex 4");
    assert_int(127,  '\x7f',       "Char literal hex 5");
    assert_int(-128, '\x80',       "Char literal hex 6");
    assert_int(-127, '\x81',       "Char literal hex 7");
    assert_int(-1,   '\xff',       "Char literal hex 8");
    assert_int(-1,   '\x1ff',      "Char literal hex 9");
    assert_int(-1,   '\x1FF',      "Char literal hex 10");
    assert_int(-1,   '\xffff',     "Char literal hex 11");
    assert_int(-1,   '\xffffff',   "Char literal hex 12");
    assert_int(-1,   '\xffffffff', "Char literal hex 13");

    assert_long(-1, '\x7fffffffffffffff',   "Char literal hex 11");
    assert_long(0,  '\x8000000000000000',   "Char literal hex 12");
    assert_long(-1, '\xffffffffffffffff',   "Char literal hex 13");
    assert_long(0,  '\x1000000000000000',   "Char literal hex 14");
    assert_long(-1, '\xffffffff',           "Char literal hex 15");
    assert_long(-1, '\xffff',               "Char literal hex 16");

    assert_int(65535,    '\xff\xff',    "Multiple char literals 1");
    assert_int(65530,    '\xff\xffA',   "Multiple char literals 2");
    assert_int(65531,    '\xff\xffB',   "Multiple char literals 3");
    assert_int(65451,    '\xff\xffAB',  "Multiple char literals 4");
    assert_int(16777031, '\xff\xffG',   "Multiple char literals 5");
    assert_int(16777031, '\x1ff\x1ffG', "Multiple char literals 6");

    // Octal constants.
    assert_int(0,           '\0',           "Char literal octal 1");
    assert_int(0,           '\00',          "Char literal octal 2");
    assert_int(1,           '\01',          "Char literal octal 3");
    assert_int(0,           '\000',         "Char literal octal 4");
    assert_int(1,           '\001',         "Char literal octal 5");
    assert_int(7,           '\007',         "Char literal octal 6");
    assert_int(8,           '\010',         "Char literal octal 7");
    assert_int(64,          '\100',         "Char literal octal 8");
    assert_int(8,           '\010',         "Char literal octal 9");
    assert_int(2096,        '\0100',        "Char literal octal 10");
    assert_int(77872,       '\00100',       "Char literal octal 11");
    assert_int(536624,      '\01000',       "Char literal octal 12");
    assert_int(137375792,   '\010000',      "Char literal octal 13");
    assert_int(808464432,   '\0100000',     "Char literal octal 14");
    assert_int(64,          '\100',         "Char literal octal 15");
    assert_int(16432,       '\1000',        "Char literal octal 16");
    assert_int(4206640,     '\10000',       "Char literal octal 17");
    assert_int(65335,       '\7777',        "Char literal octal 18");
    assert_int(16725815,    '\77777',       "Char literal octal 19");
    assert_int(4281808695,  '\777777',      "Char literal octal 20");
    assert_int(926365495,   '\7777777',     "Char literal octal 21");
    assert_int(926365495,   '\77777777',    "Char literal octal 22");
    assert_int(65336,       '\7778',        "Char literal octal 23");
    assert_int(16193,       '\77A',         "Char literal octal 24");

    assert_int(16641,       'A\1',          "Char literal octal 25a");
    assert_int(16648,       'A\10',         "Char literal octal 25b");
    assert_int(16704,       'A\100',        "Char literal octal 25c");
    assert_int(16641,       'A\01',         "Char literal octal 25d");
    assert_int(16648,       'A\010',        "Char literal octal 25e");
    assert_int(4261936,     'A\0100',       "Char literal octal 25f");
    assert_int(1090596912,  'A\00100',      "Char literal octal 25g");
    assert_int(19935280,    'A\001000',     "Char literal octal 25h");
    assert_int(808464432,   'A\0010000',    "Char literal octal 25i");

    assert_int(16706,       '\101\102',          "Char literal octal 26a");
    assert_int(4276803,     '\101\102\103',      "Char literal octal 26b");
    assert_int(1094861636,  '\101\102\103\104',  "Char literal octal 26c");
    assert_int(2113,        '\10A',              "Char literal octal 27");
}

static void test_string_literal_lexer() {
    assert_string("foobar", "foo" "bar", "Adjacent string literals 1");
    assert_string("foobar", "foo"
                            "bar",       "Adjacent string literals 2");
    assert_string("foobarbaz", "foo"
                            "bar" "baz", "Adjacent string literals 2");

    char *foon = "foo\n";
    assert_int(4, strlen(foon), "String literal escapes \\n 1");
    assert_int('f',  foon[0],  "String literal escapes \\n 2");
    assert_int('o',  foon[1],  "String literal escapes \\n 3");
    assert_int('o',  foon[2],  "String literal escapes \\n 4");
    assert_int(10,   foon[3],  "String literal escapes \\n 5");
    assert_int(0,    foon[4],  "String literal escapes \\n 6");

    assert_int(0,  "\0"[0], "String literal escapes \0'");
    assert_int(39, "\'"[0], "String literal escapes \\'");
    assert_int(34, "\""[0], "String literal escapes \\\"");
    assert_int(63, "\?"[0], "String literal escapes \\?");
    assert_int(92, "\\"[0], "String literal escapes \\\\");
    assert_int( 7, "\a"[0], "String literal escapes \\a");
    assert_int( 8, "\b"[0], "String literal escapes \\b");
    assert_int(12, "\f"[0], "String literal escapes \\f");
    assert_int(10, "\n"[0], "String literal escapes \\n");
    assert_int(13, "\r"[0], "String literal escapes \\r");
    assert_int( 9, "\t"[0], "String literal escapes \\t");
    assert_int(11, "\v"[0], "String literal escapes \\v");

    // Hex escapes
    assert_string("fooA",       "foo\x41",          "String literal hex 1");
    assert_string("fooB",       "foo\x4142",        "String literal hex 2");
    assert_string("fooAB",      "foo\x41\x42",      "String literal hex 3");
    assert_string("fooAB\a",    "foo\x41\x42\x07",  "String literal hex 4");
    assert_string("fooAG",      "foo\x41G",         "String literal hex 5");
    assert_int(9,    "\x9"[0],                      "String literal hex 6");
    assert_int(31,    "\x41F"[0],                   "String literal hex 7");
    assert_int(120,   "\x12345678"[0],              "String literal hex 8");
    assert_int(-128,  "\x80"[0],                    "String literal hex 9");
    assert_int(-112,  "\x1234567890"[0],            "String literal hex 10");
    assert_int(-1,    "\x123456789ff"[0],           "String literal hex 11");
    assert_int(-1,    "\x123456789ffg"[0],          "String literal hex 12");
    assert_int('g',    "\x123456789ffg"[1],         "String literal hex 13");
    assert_int(1,     "\x1"[0],                     "String literal hex 14");
    assert_int(-1,    "\xFF"[0],                    "String literal hex 15");

    assert_int('A',    "A\777B"[0],                 "String literal octal 1a");
    assert_int(-1,     "A\777B"[1],                 "String literal octal 1b");
    assert_int('B',    "A\777B"[2],                 "String literal octal 1c");
    assert_int('A',    "\101\102"[0],               "String literal octal 2a");
    assert_int('B',    "\101\102"[1],               "String literal octal 2b");
    assert_int(8,     "\10A"[0],                    "String literal octal 3a");
    assert_int('A',   "\10A"[1],                    "String literal octal 3b");

    // Test escaping of a string literal in the assembly output by using a variable
    char *s = "\'\"\?\\\a\b\f\n\r\t\v\x80\xff";
    assert_int(13, strlen(s), "String escaping in codegen 1");
    assert_int(39,   s[0],    "String escaping in codegen 1 0");
    assert_int(34,   s[1],    "String escaping in codegen 1 1");
    assert_int(63,   s[2],    "String escaping in codegen 1 2");
    assert_int(92,   s[3],    "String escaping in codegen 1 3");
    assert_int(7,    s[4],    "String escaping in codegen 1 4");
    assert_int(8,    s[5],    "String escaping in codegen 1 5");
    assert_int(12,   s[6],    "String escaping in codegen 1 6");
    assert_int(10,   s[7],    "String escaping in codegen 1 7");
    assert_int(13,   s[8],    "String escaping in codegen 1 8");
    assert_int(9,    s[9],    "String escaping in codegen 1 9");
    assert_int(11,   s[10],   "String escaping in codegen 1 10");
    assert_int(-128, s[11],   "String escaping in codegen 1 11");
    assert_int(-1,   s[12],   "String escaping in codegen 1 12");
    assert_int(0,    s[13],   "String escaping in codegen 1 13");

    // Test \0 in strings
    s = "\0a";
    assert_int(0,  s[0], "String escaping in codegen 2 0");
    assert_int(97, s[1], "String escaping in codegen 2 1");
    assert_int(0,  s[2], "String escaping in codegen 2 2");

    // Test \0 in strings
    s = "a\0";
    assert_int(97, s[0], "String escaping in codegen 3 0");
    assert_int(0,  s[1], "String escaping in codegen 3 1");
    assert_int(0,  s[2], "String escaping in codegen 3 2");

    s = "a\0b";
    assert_int(97, s[0], "String escaping in codegen 4 0");
    assert_int(0,  s[1], "String escaping in codegen 4 1");
    assert_int(98, s[2], "String escaping in codegen 4 2");
    assert_int(0,  s[3], "String escaping in codegen 4 3");
}

int test_wide_char_char_literal_lexer() {
    assert_int(255,      L'\xff',         "wchar_t char 1");
    assert_int(4095,     L'\xfff',        "wchar_t char 2");
    assert_int(65535,    L'\xffff',       "wchar_t char 3");
    assert_int(1048575,  L'\xfffff',      "wchar_t char 4");
    assert_int(16777215, L'\xffffff',     "wchar_t char 5");
    assert_int(-1,       L'\xffffffff',   "wchar_t char 6");
    assert_int(-1,       L'\xfffffffff',  "wchar_t char 7");
    assert_int(255,      L'\xff',         "wchar_t char 8");
    assert_int(255,      L'\xff\xff',     "wchar_t char 9");
    assert_int(255,      L'\xff\xff',     "wchar_t char 10");
    assert_int(255,      L'\xff\xff\xff', "wchar_t char 111");
}

int test_wide_char_string_literal_lexer() {
    wchar_t *wc = L"ab";
    assert_int(4, sizeof(wc[0]), "wchar_t string literal char size");
    assert_int(97, wc[0], "wchar_t string literal s[0]");
    assert_int(98, wc[1], "wchar_t string literal s[1]");
    assert_int(0,  wc[2], "wchar_t string literal s[2]");
    assert_int(2, wcslen(wc), "wchar_t string literal wcslen");

    wchar_t *wc2 = L"ab" L"cd";
    assert_int(97,  wc2[0], "wchar_t string literal 2 s[0]");
    assert_int(98,  wc2[1], "wchar_t string literal 2 s[1]");
    assert_int(99,  wc2[2], "wchar_t string literal 2 s[2]");
    assert_int(100, wc2[3], "wchar_t string literal 2 s[3]");
    assert_int(0,   wc2[4], "wchar_t string literal 2 s[4]");
    assert_int(4, wcslen(wc2), "wchar_t string literal 2 wcslen");

    // If any string literal are wide, they all are wide
    assert_int(8,  sizeof( "foo"  "bars"), "Sizeof split strings ..");
    assert_int(32, sizeof( "foo" L"bars"), "Sizeof split strings .L");
    assert_int(32, sizeof(L"foo"  "bars"), "Sizeof split strings L.");
    assert_int(32, sizeof(L"foo" L"bars"), "Sizeof split strings LL");
}

int main(int argc, char **argv) {

    parse_args(argc, argv, &verbose);

    test_char_literal_lexer();
    test_string_literal_lexer();
    test_wide_char_char_literal_lexer();
    test_wide_char_string_literal_lexer();

    finalize();
}
