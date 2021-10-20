#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "../../wcc.h"
#include "../test-lib.h"

int verbose;
int passes;
int failures;

static void assert_english_type_str(char *expected, char *actual, char *message) {
    if (strcmp(expected, actual)) {
        printf("%-32s ", message);
        failures++;
        printf("failed, expected %s, got %s\n", expected, actual);
    }
    else {
        passes++;
        if (verbose) printf("%-32s ok\n", message);
    }
}

void assert_english_type(Type *type, char *expected_english, char *message) {
    char *english = sprint_type_in_english(type);
    assert_english_type_str(expected_english, english, message);
}

static void assert_type_eq(Type *expected, Type *got, Type *src1, Type *src2) {
    if (expected->type != got->type || expected->is_unsigned != got->is_unsigned) {
        printf("Types mismatch. From ");
        print_type(stdout, src1);
        printf(" and ");
        print_type(stdout, src2);
        printf(", expected ");
        print_type(stdout, expected);
        printf(", but got ");
        print_type(stdout, got);
        printf("\n");

        failures++;
    }
}

static Type *t(int type_type, int is_unsigned) {
    Type *type;

    type = new_type(type_type);
    type->is_unsigned = is_unsigned;

    return type;
}

void test_integer_operation_type(Type *dst, Type *src1, Type *src2) {
    Value *src1_value, *src2_value;
    src1_value = new_value();
    src1_value->type = src1;
    src2_value = new_value();
    src2_value->type = src2;

    assert_type_eq(dst, operation_type(src1_value, src2_value, 0), src1, src2);
    assert_type_eq(dst, operation_type(src2_value, src1_value, 0), src2, src1);
}

void test_integer_types_operations() {
    int u, t1, t2, r;

    // Types match
    for (u = 0; u++; u < 2) {
        test_integer_operation_type(t(TYPE_INT,  u), t(TYPE_CHAR,  u), t(TYPE_CHAR,  u));
        test_integer_operation_type(t(TYPE_INT,  u), t(TYPE_SHORT, u), t(TYPE_SHORT, u));
        test_integer_operation_type(t(TYPE_INT,  u), t(TYPE_INT,   u), t(TYPE_INT,   u));
        test_integer_operation_type(t(TYPE_LONG, u), t(TYPE_LONG,  u), t(TYPE_LONG,  u));
    }

    // Same sign, different precision
    for (u = 0; u++; u < 2)
        for (t1 = TYPE_CHAR; t1 <= TYPE_LONG; t1++)
            for (t2 = TYPE_CHAR; t2 <= TYPE_LONG; t2++) {
                r = (t1 == TYPE_LONG || t2 == TYPE_LONG) ? TYPE_LONG : TYPE_INT;
                test_integer_operation_type(t(r,  u), t(t1,  u), t(t2,  u));
            }

    // Different sign, different precision
    test_integer_operation_type(t(TYPE_INT,  0), t(TYPE_CHAR,  1), t(TYPE_INT,  0));
    test_integer_operation_type(t(TYPE_INT,  1), t(TYPE_CHAR,  0), t(TYPE_INT,  1));
    test_integer_operation_type(t(TYPE_INT,  0), t(TYPE_SHORT, 1), t(TYPE_INT,  0));
    test_integer_operation_type(t(TYPE_INT,  1), t(TYPE_SHORT, 0), t(TYPE_INT,  1));
    test_integer_operation_type(t(TYPE_LONG, 0), t(TYPE_CHAR,  1), t(TYPE_LONG, 0));
    test_integer_operation_type(t(TYPE_LONG, 1), t(TYPE_CHAR,  0), t(TYPE_LONG, 1));
    test_integer_operation_type(t(TYPE_LONG, 0), t(TYPE_SHORT, 1), t(TYPE_LONG, 0));
    test_integer_operation_type(t(TYPE_LONG, 1), t(TYPE_SHORT, 0), t(TYPE_LONG, 1));
    test_integer_operation_type(t(TYPE_LONG, 0), t(TYPE_INT,   1), t(TYPE_LONG, 0));
    test_integer_operation_type(t(TYPE_LONG, 1), t(TYPE_INT,   0), t(TYPE_LONG, 1));
}

Type *run_lexer(char *type_str) {
    char *filename =  make_temp_filename("/tmp/XXXXXX.c");
    f = fopen(filename, "w");
    fprintf(f, "%s\n", type_str);
    fprintf(f, "\n");
    fclose(f);
    init_lexer(filename);
    init_parser();
    init_scopes();

    Type *result = parse_type_name();

    if (cur_token != TOK_EOF)
        printf("%-32s Did not get EOF\n", type_str);

    return result;
}

Type *test_type_parser(char *type_str, char *expected_english) {
    Type *type = run_lexer(type_str);

    assert_english_type(type, expected_english, type_str);

    return type;
}

int test_type_parsing() {
    // Basic types
    test_type_parser("char x",                    "char");
    test_type_parser("short x",                   "short");
    test_type_parser("int x",                     "int");
    test_type_parser("long x",                    "long");
    test_type_parser("long long x",               "long");
    test_type_parser("float x",                   "float");
    test_type_parser("double x",                  "double");
    test_type_parser("long double x",             "long double");
    test_type_parser("unsigned int x",            "unsigned int");
    test_type_parser("signed int x",              "int");

    // Qualifiers
    test_type_parser("const int x",               "const int");
    test_type_parser("const int x",               "const int");
    test_type_parser("const const int x",         "const int");
    test_type_parser("volatile int x",            "volatile int");
    test_type_parser("volatile volatile int x",   "volatile int");
    test_type_parser("const volatile int x",      "const volatile int");
    test_type_parser("int * const x",             "const pointer to int");
    test_type_parser("int * const const x",       "const pointer to int");
    test_type_parser("int * volatile x",          "volatile pointer to int");
    test_type_parser("int * volatile volatile x", "volatile pointer to int");
    test_type_parser("int * const volatile x",    "const volatile pointer to int");
    test_type_parser("int * const * volatile x",  "volatile pointer to const pointer to int");
    test_type_parser("const int * const x",       "const pointer to const int");

    // Combinations of pointers, arrays and function calls
    test_type_parser("int *x",                    "pointer to int");
    test_type_parser("int x[]",                   "array of int");
    test_type_parser("int x[1]",                  "array[1] of int");
    test_type_parser("int x()",                   "function() returning int");
    test_type_parser("int **x",                   "pointer to pointer to int");
    test_type_parser("int (*x)[]",                "pointer to array of int");
    test_type_parser("int (*x)[1]",               "pointer to array[1] of int");
    test_type_parser("int (*x)()",                "pointer to function() returning int");
    test_type_parser("int *x[1]",                 "array[1] of pointer to int");
    test_type_parser("int x[1][2]",               "array[1] of array[2] of int");
    test_type_parser("int *x()",                  "function() returning pointer to int");
    test_type_parser("int ***x",                  "pointer to pointer to pointer to int");
    test_type_parser("int (**x)[1]",              "pointer to pointer to array[1] of int");
    test_type_parser("int (**x)()",               "pointer to pointer to function() returning int");
    test_type_parser("int *(*x)[1]",              "pointer to array[1] of pointer to int");
    test_type_parser("int (*x)[1][2]",            "pointer to array[1] of array[2] of int");
    test_type_parser("int *(*x)()",               "pointer to function() returning pointer to int");
    test_type_parser("int **x[1]",                "array[1] of pointer to pointer to int");
    test_type_parser("int (*x[1])[2]",            "array[1] of pointer to array[2] of int");
    test_type_parser("int (*x[1])()",             "array[1] of pointer to function() returning int");
    test_type_parser("int *x[1][2]",              "array[1] of array[2] of pointer to int");
    test_type_parser("int x[1][2][3]",            "array[1] of array[2] of array[3] of int");
    test_type_parser("int **x()",                 "function() returning pointer to pointer to int");
    test_type_parser("int (*x())[1]",             "function() returning pointer to array[1] of int");
    test_type_parser("int (*x())()",              "function() returning pointer to function() returning int");
    test_type_parser("int *(*(**x[][8])())[]",    "array of array[8] of pointer to pointer to function() returning pointer to array of pointer to int");
    test_type_parser("int (*(*x[])())()",         "array of pointer to function() returning pointer to function() returning int");

    // Structs
    test_type_parser("struct x",                        "struct x {}");
    test_type_parser("struct x {}",                     "struct x {}");
    test_type_parser("struct {int x;}",                 "struct {x as int}");
    test_type_parser("struct x {int y;}",               "struct x {y as int}");
    test_type_parser("struct x {int y; const int z;}",  "struct x {y as int, z as const int}");
    test_type_parser("struct x {int *y;}",              "struct x {y as pointer to int}");
    test_type_parser("struct x {int x[1];}",            "struct x {x as array[1] of int}");
    test_type_parser("struct x {int *x[1];}",           "struct x {x as array[1] of pointer to int}");
    test_type_parser("struct x {int (*x)[1];}",         "struct x {x as pointer to array[1] of int}");
    test_type_parser("struct x {int (*x)();}",          "struct x {x as pointer to function() returning int}");

    // Unions
    test_type_parser("union x",                        "union x {}");
    test_type_parser("union {int x;}",                 "union {x as int}");

    // Function parameters
    test_type_parser("void x(void)",                   "function(void) returning void");
    test_type_parser("void x(int)",                    "function(int) returning void");
    test_type_parser("void x(int i)",                  "function(int) returning void");
    test_type_parser("void x(int i, ...)",             "function(int, ...) returning void");
    test_type_parser("void x(int i, int *i, int i[])", "function(int, pointer to int, array of int) returning void");

    // Enums
    test_type_parser("enum {I}", "enum {}");
}

int test_composite_type() {
    Type *type1, *type2, *res;

    type1 = make_array(new_type(TYPE_INT), 10);
    type2 = make_array(new_type(TYPE_INT), 0);
    res = composite_type(type1, type2);
    assert_int(10, res->array_size, "Composite type of int[10] and int[]");
    res = composite_type(type2, type1);
    assert_int(10, res->array_size, "Composite type of int[] and int[10]");

    type1 = make_pointer(make_array(new_type(TYPE_INT), 10));
    type2 = make_pointer(make_array(new_type(TYPE_INT), 0));
    res = composite_type(type1, type2);
    assert_int(10, res->target->array_size, "Composite type of int(*)[10] and int(*)[]");
    res = composite_type(type2, type1);
    assert_int(10, res->target->array_size, "Composite type of int(*)[] and int(*)[10]");

    type1 = run_lexer("void ()");
    type2 = run_lexer("void (int)");
    res = composite_type(type1, type2);
    assert_english_type(res, "function(int) returning void", "Composite type of void() and void(int");
    res = composite_type(type2, type1);
    assert_english_type(res, "function(int) returning void", "Composite type of void() and void(int)");

    type1 = run_lexer("void (int[])");
    type2 = run_lexer("void (int[10])");
    res = composite_type(type1, type2);
    assert_english_type(res, "function(array[10] of int) returning void", "Composite type of void(int[]) and void(int[10])");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_integer_types_operations();
    test_type_parsing();
    test_composite_type();

    finalize();
}
