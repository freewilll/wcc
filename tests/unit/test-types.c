#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "../../wcc.h"
#include "../test-lib.h"

int verbose;
int passes;
int failures;

Type *lex_type(char *type_str) {
    char *filename =  make_temp_filename("/tmp/XXXXXX.c");
    FILE *f = fopen(filename, "w");
    fprintf(f, "%s\n", type_str);
    fprintf(f, "\n");
    fclose(f);
    init_lexer_from_filename(filename);
    init_parser();
    init_scopes();

    Type *type = parse_type_name();

    if (cur_token != TOK_EOF)
        printf("%-32s Did not get EOF parsing type\n", type_str);

    return type;
}

void assert_type_eq(Type *type1, Type *type2, char *message) {
    int compatible = types_are_compatible(type1, type2);
    char buffer[100];
    sprintf(buffer, "%s offset", message);
    assert_int(1, compatible, buffer);

    if (!compatible) {
        printf("Type differences:\n");
        printf("  Expected: "); print_type_in_english(type1);
        printf("  Got:      "); print_type_in_english(type2);
    }
}

int test_compatible_types() {
    Type *type1, *type2;

    assert_int(0, types_are_compatible(new_type(TYPE_CHAR), new_type(TYPE_INT)), "char int");
    assert_int(1, types_are_compatible(new_type(TYPE_INT), new_type(TYPE_INT)), "int int");

    type1 = lex_type("char i");
    type2 = lex_type("unsigned char i");
    assert_int(0, types_are_compatible(type1, type2), "unsigned char and signed char");

    type1 = lex_type("short i");
    type2 = lex_type("unsigned short i");
    assert_int(0, types_are_compatible(type1, type2), "unsigned short and signed short");

    type1 = lex_type("int i");
    type2 = lex_type("unsigned int i");
    assert_int(0, types_are_compatible(type1, type2), "unsigned int and signed int");

    type1 = lex_type("long i");
    type2 = lex_type("unsigned long i");
    assert_int(0, types_are_compatible(type1, type2), "unsigned long and signed long");

    type1 = new_type(TYPE_INT); type1->is_const = 1;
    type2 = new_type(TYPE_INT);
    assert_int(0, types_are_compatible(type1, type2), "const vs non const 1");

    type1 = new_type(TYPE_INT);
    type2 = new_type(TYPE_INT); type2->is_const = 1;
    assert_int(0, types_are_compatible(type1, type2), "const vs non const 2");

    type1 = new_type(TYPE_INT); type1->is_const = 1;
    type2 = new_type(TYPE_INT); type2->is_const = 1;
    assert_int(1, types_are_compatible(type1, type2), "const vs const");

    type1 = make_pointer(new_type(TYPE_INT));
    type2 = make_pointer(new_type(TYPE_INT));
    assert_int(1, types_are_compatible(type1, type2), "pointers to ints");

    type1 = make_pointer(new_type(TYPE_CHAR));
    type2 = make_pointer(new_type(TYPE_INT));
    assert_int(0, types_are_compatible(type1, type2), "mismatching pointer targets");

    type1 = make_array(new_type(TYPE_INT), 10);
    type2 = make_array(new_type(TYPE_INT), 10);
    assert_int(1, types_are_compatible(type1, type2), "int[10] arrays");

    type1 = make_array(new_type(TYPE_INT), 5);
    type2 = make_array(new_type(TYPE_INT), 10);
    assert_int(0, types_are_compatible(type1, type2), "mismatched array size");

    type1 = make_array(new_type(TYPE_INT), 0);
    type2 = make_array(new_type(TYPE_INT), 10);
    assert_int(1, types_are_compatible(type1, type2), "one array size is zero");
    assert_int(1, types_are_compatible(type2, type1), "one array size is zero");

    type1 = make_array(new_type(TYPE_CHAR), 10);
    type2 = make_array(new_type(TYPE_INT), 10);
    assert_int(0, types_are_compatible(type1, type2), "mismatched array element type");

    type1 = lex_type("int **ppi");
    type2 = lex_type("int *pai[]");
    assert_int(1, types_are_compatible(type1, type2), "pointer and array with same target");

    type1 = lex_type("struct { int i; }");
    type2 = lex_type("struct { int i; }");
    assert_int(1, types_are_compatible(type1, type2), "matching struct tags");

    type1 = lex_type("struct s1 { int i; }");
    type2 = lex_type("struct s2 { int i; }");
    assert_int(0, types_are_compatible(type1, type2), "mismatching struct tags");

    type1 = lex_type("struct { int i; }");
    type2 = lex_type("struct s2 { int i; }");
    assert_int(0, types_are_compatible(type1, type2), "mismatching struct tags");

    type1 = lex_type("enum { I, J }");
    type2 = lex_type("enum { I, J }");
    assert_int(1, types_are_compatible(type1, type2), "matching enum tags");

    type1 = lex_type("enum s1 { I, J }");
    type2 = lex_type("enum s2 { I, J }");
    assert_int(0, types_are_compatible(type1, type2), "mismatching enum tags");

    type1 = lex_type("enum { I, J }");
    type2 = lex_type("enum s2 { I, J }");
    assert_int(0, types_are_compatible(type1, type2), "mismatching enum tags");

    type1 = lex_type("enum { I, J }");
    type2 = lex_type("int");
    assert_int(1, types_are_compatible(type1, type2), "one is enum, the other an int 1");
    assert_int(1, types_are_compatible(type2, type1), "one is enum, the other an int 2");

    type1 = lex_type("struct s { int i; }");
    type2 = lex_type("struct s");
    assert_int(1, types_are_compatible(type1, type2), "incomplete + complete struct");
    assert_int(1, types_are_compatible(type2, type1), "incomplete + complete struct");
    assert_int(1, types_are_compatible(type1, type1), "incomplete + incomplete struct");

    type1 = lex_type("struct { int i, j; }");
    type2 = lex_type("struct { int i, j; }");
    assert_int(1, types_are_compatible(type1, type2), "struct match");

    type1 = lex_type("struct { int i; }");
    type2 = lex_type("struct { int i, j; }");
    assert_int(0, types_are_compatible(type1, type2), "struct member count mismatch");

    type1 = lex_type("struct { int i; }");
    type2 = lex_type("struct { int j; }");
    assert_int(0, types_are_compatible(type1, type2), "struct member name mismatch");

    type1 = lex_type("struct { int i:1; }");
    type2 = lex_type("struct { int i:2; }");
    assert_int(0, types_are_compatible(type1, type2), "struct member bit field size mismatch");

    type1 = lex_type("struct { char i; }");
    type2 = lex_type("struct { int i; }");
    assert_int(0, types_are_compatible(type1, type2), "struct member type mismatch");

    type1 = lex_type("struct { struct { int i; } s; }");
    type2 = lex_type("struct { struct { int i; } s; }");
    assert_int(1, types_are_compatible(type1, type2), "struct member sub struct match");

    type1 = lex_type("struct { struct { int i; } s; }");
    type2 = lex_type("struct { struct { int j; } s; }");
    assert_int(0, types_are_compatible(type1, type2), "struct member sub struct mismatch");

    type1 = lex_type("union { int i, a, j, b; }");
    type2 = lex_type("union { int i, a, j, b; }");
    assert_int(1, types_are_compatible(type1, type2), "union match");

    type1 = lex_type("union { int i, a, j, b; }");
    type2 = lex_type("union { int a, b, i, j; }");
    assert_int(1, types_are_compatible(type1, type2), "union out of order match");

    type1 = lex_type("union { int i; }");
    type2 = lex_type("union { int i, j; }");
    assert_int(0, types_are_compatible(type1, type2), "union member count mismatch");

    type1 = lex_type("union { int i, a, j, b; }");
    type2 = lex_type("union { int x, a, j, b; }");
    assert_int(0, types_are_compatible(type1, type2), "union member name mismatch");

    type1 = lex_type("union { int i, a, j, b; }");
    type2 = lex_type("union { char i, a, j, b; }");
    assert_int(0, types_are_compatible(type1, type2), "union member type mismatch");

    type1 = lex_type("union { int :0, a, j, b; }");
    type2 = lex_type("union { char i, a, j, b; }");
    assert_int(0, types_are_compatible(type1, type2), "union member type mismatch");

    type1 = lex_type("void f()");
    type2 = lex_type("void f()");
    assert_int(1, types_are_compatible(type1, type2), "function return types match");

    type1 = lex_type("void f()");
    type2 = lex_type("int f()");
    assert_int(0, types_are_compatible(type1, type2), "function return types mismatch");

    type1 = lex_type("void f(char *pc, ...)");
    type2 = lex_type("void f(char *pc, ...)");
    assert_int(1, types_are_compatible(type1, type2), "function variadic match");

    type1 = lex_type("void f(char *pc, ...)");
    type2 = lex_type("void f(char *pc)");
    assert_int(0, types_are_compatible(type1, type2), "function variadic mismatch");

    type1 = lex_type("void f(int)");
    type2 = lex_type("void f(int)");
    assert_int(1, types_are_compatible(type1, type2), "function types match");

    type1 = lex_type("void f(char)");
    type2 = lex_type("void f(int)");
    assert_int(0, types_are_compatible(type1, type2), "function types mismatch");

    type1 = lex_type("void f()");
    type2 = lex_type("void f(char)");
    assert_int(0, types_are_compatible(type1, type2), "function types mismatch in K&R () vs non-K&R void(char)");

    type1 = lex_type("void f()");
    type2 = lex_type("void f(int)");
    assert_int(1, types_are_compatible(type1, type2), "function types match in K&R () vs non-K&R void(int)");

    type1 = lex_type("void f(int a)"); type1->function->is_paramless = 1; // Fake K&R
    type2 = lex_type("void f(int)");
    assert_int(1, types_are_compatible(type1, type2), "function types match in K&R void(int) vs non-K&R void(int)");

    type1 = lex_type("void f(char a)"); type1->function->is_paramless = 1; // Fake K&R
    type2 = lex_type("void f(int)");
    assert_int(0, types_are_compatible(type1, type2), "function types mismatch in K&R void(char) vs non-K&R void(int)");
}

void flattened_eq(char *src, char *dst) {
    assert_type_eq(lex_type(src), lex_type(dst), src);
}

void test_anonymous_struct_flattening() {
    Type *type;

    flattened_eq("struct {           struct { int i, j; };           }",  "struct { int i, j; }");
    flattened_eq("struct { int i;    struct { int j, k; };           }",  "struct { int i, j, k; }");
    flattened_eq("struct { int i, j; struct { int k, l; };           }",  "struct { int i, j, k, l; }");
    flattened_eq("struct {           struct { int i, j; }; int k;    }",  "struct { int i, j, k; }");
    flattened_eq("struct { int i;    struct { int j, k; }; int l;    }",  "struct { int i, j, k, l; }");
    flattened_eq("struct { int i;    struct { int j, k; }; int l, m; } ", "struct { int i, j, k, l, m; }");

    flattened_eq("struct { int i;    struct { int j; struct { int k; }; }; int l, m; } ", "struct { int i, j, k, l, m; }");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv);

    init_memory_management_for_translation_unit();

    test_compatible_types();
    test_anonymous_struct_flattening();

    finalize();
}
