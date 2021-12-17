#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../test-lib.h"

int check_stack_alignment();

int verbose;
int passes;
int failures;

static char *write_temp_file(char *content) {
    char *filename = strdup("/tmp/test-errors-XXXXXX.c");
    int fd = mkstemps(filename, 2);
    if (fd == -1) {
        perror("in mkstemps");
        exit(1);
    }

    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        perror("in fdopen");
        exit(1);
    }

    fprintf(fp, "%s", content);
    fclose(fp);

    return filename;
}

static void check_output(char *code, char *expected, char *message) {
    char *filename = write_temp_file(code);

    char command[64];
    sprintf(command, "../../wcc %s 2>&1", filename);

    FILE *f = popen(command, "r");
    if (f == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }

    int found = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strstr(line, expected) != NULL) found = 1;
    }

    pclose(f);

    if (found) {
        passes++;
        if (verbose) printf("%-20s ok\n", message);
    }
    else {
        failures++;
        printf("%-60s failed, did not find: '%s'\n", message, expected);
        printf("Got: %s\n", line);
    }
}

static void check_main_output(char *code, char *expected, char *message) {
    char main_code[1024];
    sprintf(main_code, "int main() { %s }\n", code);
    check_output(main_code, expected, message);
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    check_main_output(
        "const int i;"
        "i = 1;",
        "Cannot assign to read-only variable",
        "const int assignment");

    check_output(
        "#include <stdlib.h>\n"
        "int main() {"
        "    int const *pi = malloc(sizeof(int));"
        "    *pi = 1;"
        "}",
        "Cannot assign to read-only variable",
        "assignment to pointer to const int");

    check_main_output(
        "struct { const int i; } s;"
        "s.i = 1;",
        "Cannot assign to read-only variable",
        "assignment to const struct member");

    check_main_output(
        "struct { const int i; } s1, s2;"
        "s1 = s2;",
        "Cannot assign to read-only variable",
        "assignment to struct with const member");

    check_main_output(
        "struct { struct { const int i;} s; } s1, s2;"
        "s1 = s2;",
        "Cannot assign to read-only variable",
        "assignment to struct with const member in sub struct");

    check_main_output(
        "struct s { int i; };"
        "const struct s s;"
        "s.i = 1;",
        "Cannot assign to read-only variable",
        "Assigmnent to const struct through . operator");

    check_main_output(
        "struct s { int i; };"
        "const struct s s;"
        "struct s *const ps = &s;"
        "ps->i = 1;",
        "Cannot assign to read-only variable",
        "Assigmnent to const struct through -> operator");

    check_main_output(
        "const int i;"
        "i++;",
        "Cannot assign to read-only variable",
        "const int++");

    check_main_output(
        "const int i;"
        "i--;",
        "Cannot assign to read-only variable",
        "const int--");

    check_main_output(
        "const int i;"
        "++i;",
        "Cannot assign to read-only variable",
        "const ++int");

    check_main_output(
        "const int i;"
        "--i;",
        "Cannot assign to read-only variable",
        "const --int");

    check_main_output(
        "const int i;"
        "i += 1;",
        "Cannot assign to read-only variable",
        "Compound assignment");

    check_output(
        "#include <stdlib.h>\n"
        "int main() {"
        "    struct s *s;"
        "    s = malloc(sizeof(s));"
        "    s->i = 1;"
        "}",
        "Dereferencing a pointer to incomplete struct or union",
        "Dereferencing a pointer to incomplete struct or union");

    check_main_output(
        "sizeof(struct s)",
        "Cannot take sizeof an incomplete type",
        "Cannot take sizeof an incomplete type");

    check_main_output(
        "struct s { int i:5; } s;"
        "sizeof(s.i)",
        "Cannot take sizeof a bit field",
        "Cannot take sizeof a bit field");

    check_main_output(
        "struct s { struct v v; };",
        "Struct/union members cannot have an incomplete type",
        "Struct/union members cannot have an incomplete type");

    check_output(
        "void foo() { return 1; }\n"
        "int main() {}",
        "Return with a value in a function returning void",
        "Return with a value in a function returning void");

    check_output(
        "void foo(void) {}\n"
        "int main() { foo(1) }",
        "Too many arguments for function call",
        "Too many arguments for function call for foo(void)");

    check_output(
        "void foo(int i) {}\n"
        "int main() { foo(1, 2) }",
        "Too many arguments for function call",
        "Too many arguments for function call");

    check_main_output(
        "struct { int i; } s1;"
        "struct { int j; } s2;"
        "1 ? s1 : s2;",
        "Invalid operands to ternary operator",
        "Invalid operands to ternary operator for incompatible structs");

    check_main_output(
        "typedef int A[2][3];"
        "const A a;"
        "int* pi = a[0]; // Error: a[0] has type const int*",
        "Incompatible types in assignment",
        "Moving const from array to array elements");

    check_output(
        "int foo()[];",
        "Functions cannot return arrays",
        "Functions cannot return arrays");

    check_output(
        "int foo() {}\n int foo() {}",
        "Redefinition of foo",
        "Redefinition of function");

    check_output(
        "int foo();\n int foo(char);",
        "Incompatible function types",
        "Incompatible function types () vs (char)");

    check_output(
        "int foo();\n int foo(float);",
        "Incompatible function types",
        "Incompatible function types () vs (float)");

    check_output(
        "auto int i;",
        "auto not allowed in global scope",
        "auto not allowed in global scope");

    check_output(
        "register int i;",
        "register not allowed in global scope",
        "register not allowed in global scope");

    check_output(
        "void foo(auto i)",
        "Invalid storage for function parameter",
        "Invalid storage for function parameter auto");

    check_output(
        "void foo(static i)",
        "Invalid storage for function parameter",
        "Invalid storage for function parameter static");

    check_output(
        "void foo(extern i)",
        "Invalid storage for function parameter",
        "Invalid storage for function parameter extern");

    check_output(
        "static int a;"
        "int a;",
        "Mismatching linkage in redeclared identifier a",
        "Mismatching linkage static/non-static");

    check_output(
        "int a;"
        "static int a;",
        "Mismatching linkage in redeclared identifier a",
        "Mismatching linkage non-static/static");

    check_output(
        "static int a();"
        "int a();",
        "Mismatching linkage in redeclared identifier a",
        "Mismatching linkage static/non-static");

    check_output(
        "int a();"
        "static int a();",
        "Mismatching linkage in redeclared identifier a",
        "Mismatching linkage non-static/static");

    check_main_output(
        "switch(1.1);",
        "The controlling expression of a switch statement is not an integral type",
        "Switch with double");

    check_main_output(
        "struct {int i;} s;"
        "switch(s);",
        "The controlling expression of a switch statement is not an integral type",
        "Switch with struct");

    check_main_output(
        "    switch(1) {"
        "       case 1: break;"
        "       case 1: break;"
        "    }",
        "Duplicate switch case value",
        "Switch with two same values");

    check_main_output(
        "    switch(1) {"
        "       case 2: break;"
        "       case 0x100000002: break;"
        "    }",
        "Duplicate switch case value",
        "Switch with two same values, truncated from longs to ints");

    check_output(
        "void main(int) {}",
        "Missing identifier for parameter in function definition",
        "Missing identifier for parameter in function definition");

    check_output(
        "void main() {"
        "    int i, j;"
        "    va_start(i, j);"
        "}",
        "Expected va_list type as first argument to va_start",
        "Expected va_list type as first argument to va_start");

    check_output(
        "#include <stdarg.h>\n"
        "void main() {"
        "    va_list ap;"
        "    int i;"
        "    va_start(ap, i);"
        "}",
        "Expected function parameter as second argument to va_start",
        "Expected function parameter as second argument to va_start 1");

    check_output(
        "#include <stdarg.h>\n"
        "void main() {"
        "    va_list ap;"
        "    va_start(ap, 0);"
        "}",
        "Expected function parameter as second argument to va_start",
        "Expected function parameter as second argument to va_start 2");

    check_output(
        "#include <stdarg.h>\n"
        "void main(int i, int j, ...) {"
        "    va_list ap;"
        "    va_start(ap, i);"
        "}",
        "Second argument to va_start isn't the rightmost function parameter",
        "Second argument to va_start isn't the rightmost function parameter");

    check_output(
        "#include <stdarg.h>\n"
        "void main(int i, ...) {"
        "    va_list ap;"
        "    va_start(ap, i);"
        "    va_arg(ap, int[]);"
        "}",
        "Cannot use an array in va_arg",
        "Cannot use an array in va_arg");

    check_output(
        "void main() {"
        "    char i[1.1]"
        "}",
        "Expected an integer constant expression",
        "Expected an integer constant expression");

    check_output(
        "struct s {i:3;} s;"
        "int *ps = &s.i;",
        "Cannot take an address of a bit-field",
        "Cannot take an address of a bit-field");

    check_main_output(
        "struct s;"
        "struct s sa[1];",
        "Array has incomplete element type",
        "Array has incomplete element type");

    check_output(
        "struct s;"
        "struct s s = {1};",
        "Attempt to use an incomplete struct or union in an initializer",
        "Attempt to use an incomplete struct or union in an initializer");

    check_main_output(
        "struct s;"
        "struct s s = {1};",
        "Attempt to use an incomplete struct or union in an initializer",
        "Attempt to use an incomplete struct or union in an initializer");

    check_main_output(
        "struct s {int i; int i;};",
        "Duplicate struct/union member",
        "Duplicate struct/union member direct");

    check_main_output(
        "struct s {int i; struct { int i;}; };",
        "Duplicate struct/union member",
        "Duplicate struct/union member indirect");

    check_main_output(
        "case 1:;",
        "Case label not within a switch statement",
        "Case label not within a switch statement");

    check_main_output(
        "default:;",
        "Default label not within a switch statement",
        "Default label not within a switch statement");

    check_main_output(
        "switch(0) { default:; default:;}",
        "Duplicate default label",
        "Duplicate default label");

    finalize();
}
