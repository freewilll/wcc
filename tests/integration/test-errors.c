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
    sprintf(command, "../../wcc %s\n", filename);

    FILE *f = popen(command, "r");
    if (f == NULL) {
        printf("Failed to run command\n" );
        exit(1);
    }

    int found = 0;
    char line[1035];
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

    check_main_output(
        "int const *pi = malloc(sizeof(int));"
        "*pi = 1;",
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

    check_main_output(
        "struct s *s;"
        "s = malloc(sizeof(s));"
        "s->i = 1;",
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


    finalize();
}
