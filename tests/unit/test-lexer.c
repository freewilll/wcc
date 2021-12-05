#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../wcc.h"


int failures;

void assert_int(int expected, int got, char *message) {
    if (expected != got) {
        printf("Expected %d, got %d in %s\n", expected, got, message);
        failures++;
    }
}

void assert_long_double(long double expected, long double got, char *message) {
    long double diff = expected - got;
    if (diff < 0) diff = -diff;
    if (diff > 0.000001) {
        printf("Expected %Lf, got %Lf in %s\n", expected, got, message);
        failures++;
    }
}
void assert_type(int type_type, int is_unsigned, char *message) {
    if (cur_lexer_type->type != type_type || cur_lexer_type->is_unsigned != is_unsigned) {
        printf("Type mismatch: expected %d/%d, got %d/%d in %s\n",
            type_type, is_unsigned, cur_lexer_type->type, cur_lexer_type->is_unsigned, message);
        failures++;
    }
}

void test_numeric_integer_literal(char *string, int type_type, int is_unsigned) {
    warn_integer_constant_too_large = 0;
    cur_filename = "test";
    cur_line = 0;
    init_lexer_from_string(string);
    assert_type(type_type, is_unsigned, string);
}

void test_decimal_constants() {
    // No suffix
    test_numeric_integer_literal("1",                      TYPE_INT,  0);
    test_numeric_integer_literal("2147483647",             TYPE_INT,  0);
    test_numeric_integer_literal("2147483648",             TYPE_LONG, 0);
    test_numeric_integer_literal("4294967295",             TYPE_LONG, 0);
    test_numeric_integer_literal("9223372036854775807",    TYPE_LONG, 0);
    test_numeric_integer_literal("9223372036854775808",    TYPE_LONG, 1); // with warning
    test_numeric_integer_literal("18446744073709551615",   TYPE_LONG, 1); // with warning

    // u
    test_numeric_integer_literal("1u",                     TYPE_INT,  1);
    test_numeric_integer_literal("2147483647u",            TYPE_INT,  1);
    test_numeric_integer_literal("2147483648u",            TYPE_INT,  1);
    test_numeric_integer_literal("4294967295u",            TYPE_INT,  1);
    test_numeric_integer_literal("9223372036854775807u",   TYPE_LONG, 1);
    test_numeric_integer_literal("9223372036854775808u",   TYPE_LONG, 1);
    test_numeric_integer_literal("18446744073709551615u",  TYPE_LONG, 1);

    // l
    test_numeric_integer_literal("1l",                     TYPE_LONG, 0);
    test_numeric_integer_literal("2147483647l",            TYPE_LONG, 0);
    test_numeric_integer_literal("2147483648l",            TYPE_LONG, 0);
    test_numeric_integer_literal("4294967295l",            TYPE_LONG, 0);
    test_numeric_integer_literal("9223372036854775807l",   TYPE_LONG, 0);
    test_numeric_integer_literal("9223372036854775808l",   TYPE_LONG, 1); // with warning
    test_numeric_integer_literal("18446744073709551615l",  TYPE_LONG, 1); // with warning

    // ul
    test_numeric_integer_literal("1ul",                    TYPE_LONG, 1);
    test_numeric_integer_literal("2147483647ul",           TYPE_LONG, 1);
    test_numeric_integer_literal("2147483648ul",           TYPE_LONG, 1);
    test_numeric_integer_literal("4294967295ul",           TYPE_LONG, 1);
    test_numeric_integer_literal("9223372036854775807ul",  TYPE_LONG, 1);
    test_numeric_integer_literal("9223372036854775808ul",  TYPE_LONG, 1); // with warning
    test_numeric_integer_literal("18446744073709551615ul", TYPE_LONG, 1); // with warning
}

void test_hex_constants() {
    // No suffix
    test_numeric_integer_literal("0x1",                 TYPE_INT,  0);
    test_numeric_integer_literal("0x7fffffff",          TYPE_INT,  0);
    test_numeric_integer_literal("0x80000000",          TYPE_INT,  1);
    test_numeric_integer_literal("0xffffffff",          TYPE_INT,  1);
    test_numeric_integer_literal("0x7fffffffffffffff",  TYPE_LONG, 0);
    test_numeric_integer_literal("0x8000000000000000",  TYPE_LONG, 1);
    test_numeric_integer_literal("0xffffffffffffffff",  TYPE_LONG, 1);

    // u
    test_numeric_integer_literal("0x1u",                 TYPE_INT,  1);
    test_numeric_integer_literal("0x7fffffffu",          TYPE_INT,  1);
    test_numeric_integer_literal("0x80000000u",          TYPE_INT,  1);
    test_numeric_integer_literal("0xffffffffu",          TYPE_INT,  1);
    test_numeric_integer_literal("0x7fffffffffffffffu",  TYPE_LONG, 1);
    test_numeric_integer_literal("0x8000000000000000u",  TYPE_LONG, 1);
    test_numeric_integer_literal("0xffffffffffffffffu",  TYPE_LONG, 1);

    // l
    test_numeric_integer_literal("0x1l",                 TYPE_LONG, 0);
    test_numeric_integer_literal("0x7fffffffl",          TYPE_LONG, 0);
    test_numeric_integer_literal("0x80000000l",          TYPE_LONG, 0);
    test_numeric_integer_literal("0xffffffffl",          TYPE_LONG, 0);
    test_numeric_integer_literal("0x7fffffffffffffffl",  TYPE_LONG, 0);
    test_numeric_integer_literal("0x8000000000000000l",  TYPE_LONG, 1);
    test_numeric_integer_literal("0xffffffffffffffffl",  TYPE_LONG, 1);

    // ul
    test_numeric_integer_literal("0x1ul",                TYPE_LONG, 1);
    test_numeric_integer_literal("0x7ffffffful",         TYPE_LONG, 1);
    test_numeric_integer_literal("0x80000000ul",         TYPE_LONG, 1);
    test_numeric_integer_literal("0xfffffffful",         TYPE_LONG, 1);
    test_numeric_integer_literal("0x7ffffffffffffffful", TYPE_LONG, 1);
    test_numeric_integer_literal("0x8000000000000000ul", TYPE_LONG, 1);
    test_numeric_integer_literal("0xfffffffffffffffful", TYPE_LONG, 1);
}

void test_numeric_floating_point_literal(char *string, long double expected, int type) {
    cur_filename = "test";
    cur_line = 0;
    init_lexer_from_string(string);

    char *buffer = malloc(100);
    sprintf(buffer, "Floating point literal %s", string);
    assert_long_double(expected, cur_long_double, buffer);
    assert_int(TOK_FLOATING_POINT_NUMBER, cur_token, "Token is TOK_FLOATING_POINT_NUMBER");
    assert_int(cur_lexer_type->type, type, "Type matches");
}

void test_floating_point_constants() {
    test_numeric_floating_point_literal("3.14159265359", 3.14159265359, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1.", 1.0, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1.0", 1.0, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1.01", 1.01, TYPE_DOUBLE);
    test_numeric_floating_point_literal(".1", 0.1, TYPE_DOUBLE);
    test_numeric_floating_point_literal("0.1", 0.1, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1e0", 1e0, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1e4", 1e4, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1E4", 1e4, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1e+4", 1e4, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1e-4", 1e-4, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1.e4", 1e4, TYPE_DOUBLE);
    test_numeric_floating_point_literal("1.f", 1.0, TYPE_FLOAT);
    test_numeric_floating_point_literal("1.F", 1.0, TYPE_FLOAT);
    test_numeric_floating_point_literal("1.0f", 1.0, TYPE_FLOAT);
    test_numeric_floating_point_literal("1e4f", 1e4, TYPE_FLOAT);
    test_numeric_floating_point_literal("1.e4f", 1e4, TYPE_FLOAT);
    test_numeric_floating_point_literal("1.l", 1.0, TYPE_LONG_DOUBLE);
    test_numeric_floating_point_literal("1.0l", 1.0, TYPE_LONG_DOUBLE);
    test_numeric_floating_point_literal("1.0L", 1.0, TYPE_LONG_DOUBLE);
    test_numeric_floating_point_literal("1e4l", 1e4, TYPE_LONG_DOUBLE);
    test_numeric_floating_point_literal("1.e4l", 1e4, TYPE_LONG_DOUBLE);

    test_numeric_floating_point_literal("9223372036854775808.1L", 9223372036854775808.1L, TYPE_LONG_DOUBLE);
    test_numeric_floating_point_literal("10000000000000000000.1L", 10000000000000000000.1L, TYPE_LONG_DOUBLE);
    test_numeric_floating_point_literal("18446744073709551615.1L", 18446744073709551615.1L, TYPE_LONG_DOUBLE);
    test_numeric_floating_point_literal("18446744073709551616.1L", 18446744073709551616.1L, TYPE_LONG_DOUBLE);
    test_numeric_floating_point_literal("100000000000000000000.1L", 100000000000000000000.1L, TYPE_LONG_DOUBLE);
}

int main() {
    failures = 0;

    test_decimal_constants();
    test_hex_constants();
    test_floating_point_constants();

    if (failures) {
        printf("%d tests failed\n", failures);
        exit(1);
    }
    printf("All tests succeeded\n");
}
