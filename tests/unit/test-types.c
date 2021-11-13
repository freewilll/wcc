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

void assert_it_type(TypeIterator *it, int offset, char *type_str, char *message) {
    assert_type_eq(it->type, lex_type(type_str), message);
    char buffer[100];
    sprintf(buffer, "%s offset", message);
    assert_int(offset, it->offset, buffer);
}

void assert_it_done(TypeIterator *it, int value) {
    assert_int(value, type_iterator_done(it), "type iterator done");
}

int test_compatible_types() {
    Type *type1, *type2;

    assert_int(0, types_are_compatible(new_type(TYPE_CHAR), new_type(TYPE_INT)), "char int");
    assert_int(1, types_are_compatible(new_type(TYPE_INT), new_type(TYPE_INT)), "int int");

    type1 = lex_type("int i");
    type2 = lex_type("unsigned int i");
    assert_int(1, types_are_compatible(type1, type2), "unsigned int and signed int");

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

    type1 = lex_type("void()");
    type2 = lex_type("void()");
    assert_int(1, types_are_compatible(type1, type2), "function return types match");

    type1 = lex_type("void()");
    type2 = lex_type("int()");
    assert_int(0, types_are_compatible(type1, type2), "function return types mismatch");

    type1 = lex_type("void(char *pc, ...)");
    type2 = lex_type("void(char *pc, ...)");
    assert_int(1, types_are_compatible(type1, type2), "function variadic match");

    type1 = lex_type("void(char *pc, ...)");
    type2 = lex_type("void(char *pc)");
    assert_int(0, types_are_compatible(type1, type2), "function variadic mismatch");

    type1 = lex_type("void(int)");
    type2 = lex_type("void(int)");
    assert_int(1, types_are_compatible(type1, type2), "function types match");

    type1 = lex_type("void(char)");
    type2 = lex_type("void(int)");
    assert_int(0, types_are_compatible(type1, type2), "function types mismatch");

    type1 = lex_type("void()");
    type2 = lex_type("void(char)");
    assert_int(0, types_are_compatible(type1, type2), "function types mismatch in K&R () vs non-K&R void(char)");

    type1 = lex_type("void()");
    type2 = lex_type("void(int)");
    assert_int(1, types_are_compatible(type1, type2), "function types match in K&R () vs non-K&R void(int)");

    type1 = lex_type("void(int a)"); type1->function->is_paramless = 1; // Fake K&R
    type2 = lex_type("void(int)");
    assert_int(1, types_are_compatible(type1, type2), "function types match in K&R void(int) vs non-K&R void(int)");

    type1 = lex_type("void(char a)"); type1->function->is_paramless = 1; // Fake K&R
    type2 = lex_type("void(int)");
    assert_int(0, types_are_compatible(type1, type2), "function types mismatch in K&R void(char) vs non-K&R void(int)");
}

void test_type_iterator() {
    TypeIterator *it;
    char *type_str;
    Type *type;

    // Test shallow iteration
    type = lex_type("int");
    it = type_iterator(type);
                                 assert_it_done(it, 0); assert_it_type(it, 0, "int", "Shallow iteration int");
    it = type_iterator_next(it); assert_it_done(it, 1);

    type_str = "struct s { char c; short s; }";
    type = lex_type(type_str);
    it = type_iterator(type);    assert_it_done(it, 0); assert_it_type(it, 0, type_str, "Shallow iteration struct s { char c; short s; } char");
    it = type_iterator_next(it); assert_it_done(it, 0); assert_it_type(it, 2, type_str, "Shallow iteration struct s { char c; short s; } short");
    it = type_iterator_next(it); assert_it_done(it, 1);

    type_str = "char c[2]";
    type = lex_type(type_str);
    it = type_iterator(type);
                                 assert_it_done(it, 0); assert_it_type(it, 0, type_str, "Shallow iteration char c[2][0]");
    it = type_iterator_next(it); assert_it_done(it, 0); assert_it_type(it, 1, type_str, "Shallow iteration char c[2][1]");
    it = type_iterator_next(it); assert_it_done(it, 1);

    type_str = "struct { struct { int i; long l; } s; char c[2]; short s; }";
    type = lex_type(type_str);
    it = type_iterator(type);
                                 assert_it_done(it, 0); assert_it_type(it, 0,  type_str, "Shallow iteration nested struct 1");
    it = type_iterator_next(it); assert_it_done(it, 0); assert_it_type(it, 16, type_str, "Shallow iteration nested struct 2");
    it = type_iterator_next(it); assert_it_done(it, 0); assert_it_type(it, 18, type_str, "Shallow iteration nested struct 3");
    it = type_iterator_next(it); assert_it_done(it, 1);

    // Test deep iteration
    type = lex_type("int");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "int", "Deep interation int");
    it = type_iterator_dig(it); it = type_iterator_next(it); assert_it_done(it, 1);

    type = lex_type("struct s { char c; short s; }");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "char",  "Deep interation struct s { char c; short s; } 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 2, "short", "Deep interation struct s { char c; short s; } 2");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("char c[2]");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "char", "Deep interation int char[2] 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 1, "char", "Deep interation int char[2] 2");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("struct s { char c; short s; }[2]");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "char",  "Deep interation struct s { char c; short s; }[2] 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 2, "short", "Deep interation struct s { char c; short s; }[2] 2");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 4, "char",  "Deep interation struct s { char c; short s; }[2] 3");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 6, "short", "Deep interation struct s { char c; short s; }[2] 4");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("struct s { int i:2, j:2, :0, l:2; }"); // With zero sized bit field
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "int", "Deep interation struct s { int i:2, j:2, k:0, l:2; } 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 0, "int", "Deep interation struct s { int i:2, j:2, k:0, l:2; } 2");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 4, "int", "Deep interation struct s { int i:2, j:2, k:0, l:2; } 3");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("struct s { int i:2, j:2, :0, :0, l:2; }"); // With two zero sized bit fields
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "int", "Deep interation struct s { int i:2, j:2, k:0, l:2; } 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 0, "int", "Deep interation struct s { int i:2, j:2, k:0, l:2; } 2");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 4, "int", "Deep interation struct s { int i:2, j:2, k:0, l:2; } 3");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("struct s { char c[2]; short s; }");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "char",  "Deep interation struct s { char c[2]; short s; } 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 1, "char",  "Deep interation struct s { char c[2]; short s; } 2");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 2, "short", "Deep interation struct s { char c[2]; short s; } 3");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("struct s { struct { int i; long l; } s; char c; }");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0,  "int",  "struct s { struct { int i; long l; } s; char c; } 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 8,  "long", "struct s { struct { int i; long l; } s; char c; } 2");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 16, "char", "struct s { struct { int i; long l; } s; char c; } 3");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("struct s { struct { int i; long l; } s; char c[2]; }");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0,  "int",  "struct s { struct { int i; long l; } s; char c[2]; } 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 8,  "long", "struct s { struct { int i; long l; } s; char c[2]; } 2");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 16, "char", "struct s { struct { int i; long l; } s; char c[2]; } 3");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 17, "char", "struct s { struct { int i; long l; } s; char c[2]; } 4");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("struct s { struct { int i; long l[2]; } s; char c[2]; }");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0,  "int",  "struct s { struct { int i; long l[2]; } s; char c[2]; } 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 8,  "long", "struct s { struct { int i; long l[2]; } s; char c[2]; } 2");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 16, "long", "struct s { struct { int i; long l[2]; } s; char c[2]; } 3");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 24, "char", "struct s { struct { int i; long l[2]; } s; char c[2]; } 4");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 25, "char", "struct s { struct { int i; long l[2]; } s; char c[2]; } 5");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("struct s { struct {char c; short s; } s[2]; int i[1]; }");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "char",  "struct s { struct {char c; short s; } s[2]; int i[1]; } 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 2, "short", "struct s { struct {char c; short s; } s[2]; int i[1]; } 2");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 4, "char",  "struct s { struct {char c; short s; } s[2]; int i[1]; } 3");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 6, "short", "struct s { struct {char c; short s; } s[2]; int i[1]; } 4");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 8, "int",   "struct s { struct {char c; short s; } s[2]; int i[1]; } 5");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("union u { char c; short s; }");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "char", "Deep interation union u { char c; short s; }");
    it = type_iterator_next(it);                             assert_it_done(it, 1);

    type = lex_type("union u { char c; short s; }[2]");
    it = type_iterator(type);
    it = type_iterator_dig(it);                              assert_it_done(it, 0); assert_it_type(it, 0, "char", "Deep interation union u { char c; short s; }[2] 1");
    it = type_iterator_next(it); it = type_iterator_dig(it); assert_it_done(it, 0); assert_it_type(it, 2, "char", "Deep interation union u { char c; short s; }[2] 2");
    it = type_iterator_next(it);                             assert_it_done(it, 1);
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_compatible_types();
    test_type_iterator();

    finalize();
}
