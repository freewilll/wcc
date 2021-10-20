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
    f = fopen(filename, "w");
    fprintf(f, "%s\n", type_str);
    fprintf(f, "\n");
    fclose(f);
    init_lexer(filename);
    init_parser();
    init_scopes();

    return parse_type_name();
}

int test_compatible_types() {
    Type *type1, *type2;

    type1 = new_type(TYPE_INT); type1->is_const = 1;
    type2 = new_type(TYPE_INT);
    assert_int(0, types_are_compabible(type1, type2), "const vs non const 1");

    type1 = new_type(TYPE_INT);
    type2 = new_type(TYPE_INT); type2->is_const = 1;
    assert_int(0, types_are_compabible(type1, type2), "const vs non const 2");

    type1 = new_type(TYPE_INT); type1->is_const = 1;
    type2 = new_type(TYPE_INT); type2->is_const = 1;
    assert_int(1, types_are_compabible(type1, type2), "const vs const");

    type1 = make_pointer(new_type(TYPE_INT));
    type2 = make_pointer(new_type(TYPE_INT));
    assert_int(1, types_are_compabible(type1, type2), "pointers to ints");

    type1 = make_pointer(new_type(TYPE_CHAR));
    type2 = make_pointer(new_type(TYPE_INT));
    assert_int(0, types_are_compabible(type1, type2), "mismatching pointer targets");

    type1 = make_array(new_type(TYPE_INT), 10);
    type2 = make_array(new_type(TYPE_INT), 10);
    assert_int(1, types_are_compabible(type1, type2), "int[10] arrays");

    type1 = make_array(new_type(TYPE_INT), 5);
    type2 = make_array(new_type(TYPE_INT), 10);
    assert_int(0, types_are_compabible(type1, type2), "mismatched array size");

    type1 = make_array(new_type(TYPE_CHAR), 10);
    type2 = make_array(new_type(TYPE_INT), 10);
    assert_int(0, types_are_compabible(type1, type2), "mismatched array element type");

    type1 = lex_type("struct { int i; }");
    type2 = lex_type("struct { int i; }");
    assert_int(1, types_are_compabible(type1, type2), "matching struct tags");

    type1 = lex_type("struct s1 { int i; }");
    type2 = lex_type("struct s2 { int i; }");
    assert_int(0, types_are_compabible(type1, type2), "mismatching struct tags");

    type1 = lex_type("struct { int i; }");
    type2 = lex_type("struct s2 { int i; }");
    assert_int(0, types_are_compabible(type1, type2), "mismatching struct tags");

    type1 = lex_type("enum { I, J }");
    type2 = lex_type("enum { I, J }");
    assert_int(1, types_are_compabible(type1, type2), "matching enum tags");

    type1 = lex_type("enum s1 { I, J }");
    type2 = lex_type("enum s2 { I, J }");
    assert_int(0, types_are_compabible(type1, type2), "mismatching enum tags");

    type1 = lex_type("enum { I, J }");
    type2 = lex_type("enum s2 { I, J }");
    assert_int(0, types_are_compabible(type1, type2), "mismatching enum tags");

    assert_int(1, types_are_compabible(new_type(TYPE_INT), new_type(TYPE_INT)), "int int");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_compatible_types();

    finalize();
}
