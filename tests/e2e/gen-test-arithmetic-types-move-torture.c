#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

// Test conversions in all combinations of arithmetic types

char **types;
char **vars;
int *sizes;
int *is_signed;
int *is_float;
int *is_sse;
int *is_ptr;
char **format_str;
char **asserts;
char **sized_outcomes;

enum {
    INTS = 8,
    COUNT = 22,
};

// What is the result of casting src -> dst -> unsigned long
char *get_outcome(int dst, int src) {
    int src_size, dst_size;

    src_size = sizes[src];
    dst_size = sizes[dst];

    if (is_float[dst] && !is_float[src] && is_signed[src]) return "-1";
    if (is_float[dst] && !is_float[src] && !is_signed[src]) return sized_outcomes[src_size - 1];
    else if (is_float[dst] && is_float[src]) return "-1.1";
    else if (is_signed[src] && is_signed[dst]) return sized_outcomes[3];
    else if (!is_signed[src] && is_signed[dst] && src_size >= dst_size) return sized_outcomes[3];
    else if (is_signed[src] && !is_signed[dst] && src_size <= dst_size) return sized_outcomes[dst_size - 1];
    else if (dst_size <= src_size) return sized_outcomes[dst_size - 1];
    else return sized_outcomes[src_size - 1];
}

int skip(int dst, int src) {
    if (is_ptr[dst] && is_float[src]) return 1;
    if (is_ptr[src] && is_float[dst]) return 1;
    return 0;
}

int main() {
    void *f;

    types = malloc(sizeof(char *) * COUNT * 2);
    vars = malloc(sizeof(char *) * COUNT * 2);
    sizes = malloc(sizeof(int) * COUNT * 2);
    is_signed = malloc(sizeof(int) * COUNT * 2);
    is_float = malloc(sizeof(int) * COUNT * 2);
    is_sse = malloc(sizeof(int) * COUNT * 2);
    is_ptr = malloc(sizeof(int) * COUNT * 2);
    format_str = malloc(sizeof(char *) * COUNT * 2);
    asserts = malloc(sizeof(char *) * COUNT * 2);

    types[0]  = "char";           vars[0]  = "c";  is_float[0]  = 0; is_sse[0]  = 0;  format_str[0]  = "%d";  is_signed[0]  = 1; sizes[0]  = 1;   asserts[0]  = "assert_long";
    types[1]  = "short";          vars[1]  = "s";  is_float[1]  = 0; is_sse[1]  = 0;  format_str[1]  = "%d";  is_signed[1]  = 1; sizes[1]  = 2;   asserts[1]  = "assert_long";
    types[2]  = "int";            vars[2]  = "i";  is_float[2]  = 0; is_sse[2]  = 0;  format_str[2]  = "%d";  is_signed[2]  = 1; sizes[2]  = 3;   asserts[2]  = "assert_long";
    types[3]  = "long";           vars[3]  = "l";  is_float[3]  = 0; is_sse[3]  = 0;  format_str[3]  = "%ld"; is_signed[3]  = 1; sizes[3]  = 4;   asserts[3]  = "assert_long";
    types[4]  = "unsigned char";  vars[4]  = "uc"; is_float[4]  = 0; is_sse[4]  = 0;  format_str[4]  = "%d";  is_signed[4]  = 0; sizes[4]  = 1;   asserts[4]  = "assert_long";
    types[5]  = "unsigned short"; vars[5]  = "us"; is_float[5]  = 0; is_sse[5]  = 0;  format_str[5]  = "%d";  is_signed[5]  = 0; sizes[5]  = 2;   asserts[5]  = "assert_long";
    types[6]  = "unsigned int";   vars[6]  = "ui"; is_float[6]  = 0; is_sse[6]  = 0;  format_str[6]  = "%d";  is_signed[6]  = 0; sizes[6]  = 3;   asserts[6]  = "assert_long";
    types[7]  = "unsigned long";  vars[7]  = "ul"; is_float[7]  = 0; is_sse[7]  = 0;  format_str[7]  = "%ld"; is_signed[7]  = 0; sizes[7]  = 4;   asserts[7]  = "assert_long";
    types[8]  = "float";          vars[8]  = "f";  is_float[8]  = 1; is_sse[8]  = 1;  format_str[8]  = "%f";  is_signed[8]  = 1; sizes[8]  = -1;  asserts[8]  = "assert_float";
    types[9]  = "double";         vars[9]  = "d";  is_float[9]  = 1; is_sse[9]  = 1;  format_str[9]  = "%f";  is_signed[9]  = 1; sizes[9]  = -1;  asserts[9]  = "assert_double";
    types[10] = "long double";    vars[10] = "ld"; is_float[10] = 1; is_sse[10] = 0;  format_str[10] = "%Lf"; is_signed[10] = 1; sizes[10] = -1;  asserts[10] = "assert_long_double";

    types[11] = "char *";           vars[11] = "pc";  is_float[11]  = 0; is_sse[11]  = 0;  format_str[11]  = "%p"; is_signed[11]  = 0; sizes[11]  = 4;  asserts[11]  = "assert_long";
    types[12] = "short *";          vars[12] = "ps";  is_float[12]  = 0; is_sse[12]  = 0;  format_str[12]  = "%p"; is_signed[12]  = 0; sizes[12]  = 4;  asserts[12]  = "assert_long";
    types[13] = "int *";            vars[13] = "pi";  is_float[13]  = 0; is_sse[13]  = 0;  format_str[13]  = "%p"; is_signed[13]  = 0; sizes[13]  = 4;  asserts[13]  = "assert_long";
    types[14] = "long *";           vars[14] = "pl";  is_float[14]  = 0; is_sse[14]  = 0;  format_str[14]  = "%p"; is_signed[14]  = 0; sizes[14]  = 4;  asserts[14]  = "assert_long";
    types[15] = "unsigned char *";  vars[15] = "puc"; is_float[15]  = 0; is_sse[15]  = 0;  format_str[15]  = "%p"; is_signed[15]  = 0; sizes[15]  = 4;  asserts[15]  = "assert_long";
    types[16] = "unsigned short *"; vars[16] = "pus"; is_float[16]  = 0; is_sse[16]  = 0;  format_str[16]  = "%p"; is_signed[16]  = 0; sizes[16]  = 4;  asserts[16]  = "assert_long";
    types[17] = "unsigned int *";   vars[17] = "pui"; is_float[17]  = 0; is_sse[17]  = 0;  format_str[17]  = "%p"; is_signed[17]  = 0; sizes[17]  = 4;  asserts[17]  = "assert_long";
    types[18] = "unsigned long *";  vars[18] = "pul"; is_float[18]  = 0; is_sse[18]  = 0;  format_str[18]  = "%p"; is_signed[18]  = 0; sizes[18]  = 4;  asserts[18]  = "assert_long";
    types[19] = "float *";          vars[19] = "pf";  is_float[19]  = 0; is_sse[19]  = 0;  format_str[19]  = "%p"; is_signed[19]  = 0; sizes[19]  = 4;  asserts[19]  = "assert_long";
    types[20] = "double *";         vars[20] = "pd";  is_float[20]  = 0; is_sse[20]  = 0;  format_str[20]  = "%p"; is_signed[20]  = 0; sizes[20]  = 4;  asserts[20]  = "assert_long";
    types[21] = "long double *";    vars[21] = "pld"; is_float[21]  = 0; is_sse[21]  = 0;  format_str[21]  = "%p"; is_signed[21]  = 0; sizes[21]  = 4;  asserts[21]  = "assert_long";

    for (int i = 0; i < COUNT; i++) is_ptr[i] = i >= 11;

    for (int i = 0; i < COUNT; i++) {
        types[i + COUNT] = types[i];
        sizes[i + COUNT] = sizes[i];
        is_signed[i + COUNT] = is_signed[i];
        is_float[i + COUNT] = is_float[i];
        is_sse[i + COUNT] = is_sse[i];
        is_ptr[i + COUNT] = is_ptr[i];
        format_str[i + COUNT] = format_str[i];
        asserts[i + COUNT] = asserts[i];
        asprintf(&vars[i + COUNT], "g%s", vars[i]);
    }

    sized_outcomes = malloc(sizeof(unsigned long) * 4);
    sized_outcomes[0] = "0xff";
    sized_outcomes[1] = "0xffff";
    sized_outcomes[2] = "0xffffffff";
    sized_outcomes[3] = "0xffffffffffffffff";

    f = fopen("test-arithmetic-types-move-torture.c", "w");

    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "\n");
    fprintf(f, "#include \"../test-lib.h\"\n");
    fprintf(f, "\n");
    fprintf(f, "int failures;\n");
    fprintf(f, "int passes;\n");
    fprintf(f, "int verbose;\n");
    fprintf(f, "\n");
    fprintf(f, "#ifdef FLOATS\n");
    fprintf(f, "\n");

    // Global declarations
    for (int i = 0; i < COUNT; i++) fprintf(f, "%s %s;\n", types[COUNT + i], vars[COUNT + i]);

    fprintf(f, "\n");

    for (int src = 0; src < COUNT * 2; src++) // src
        for (int dst = 0; dst < COUNT * 2; dst++) { // dst
            if (skip(dst, src)) continue;
            fprintf(f, "static void func_%s_to_%s() {\n", vars[src], vars[dst]);
            if (is_float[src])
                fprintf(f, "    %s %s = -1.1;\n", types[src], vars[src]);
            else
                fprintf(f, "    %s %s = -1;\n", types[src], vars[src]);

            char *outcome = get_outcome(dst, src);
            fprintf(f, "    %s(%s, (%s) %s, \"Conversion %s (%s) -> %s (%s)\");\n", asserts[dst], outcome, types[dst], vars[src], types[src], vars[src], types[dst], vars[dst]);
            fprintf(f, "}\n\n");
        }

    fprintf(f, "#endif\n");
    fprintf(f, "\n");
    fprintf(f, "int main() {\n");
    fprintf(f, "    unsigned long result;\n");

    fprintf(f, "\n");
    fprintf(f, "    failures = 0;\n");
    fprintf(f, "\n");
    fprintf(f, "\n");

    fprintf(f, "    #ifdef FLOATS\n");
    for (int src = 0; src < COUNT * 2; src++) // src
        for (int dst = 0; dst < COUNT * 2; dst++) { // dst
            if (skip(dst, src)) continue;
            fprintf(f, "    func_%s_to_%s();\n", vars[src], vars[dst]);
        }
    fprintf(f, "    #endif\n");

    fprintf(f, "\n");
    fprintf(f, "    if (failures) {\n");
    fprintf(f, "        printf(\"Failures: %%d\\n\", failures);\n");
    fprintf(f, "        exit(1);\n");
    fprintf(f, "    }\n");
    fprintf(f, "    else\n");
    fprintf(f, "        printf(\"Arithmetic types move torture tests passed\\n\");\n");
    fprintf(f, "}\n");

    fclose(f);
}
