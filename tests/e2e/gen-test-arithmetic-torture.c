#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

// Test arithmetic for all kinds of combinations

char **vars;
int *is_int;
char **str_types;
int *types;
int *storage;
char **str_storage;
char **str_values;
long double *ld_values;
char **asserts;

enum {
    COUNT = 12,
};

enum {
    CONSTANT = 1,
    LOCAL = 2,
    GLOBAL = 3,
};

enum {
    TYPE_INT = 0,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LD,
};

void output_value(void *f, int index) {
    if (storage[index] == CONSTANT)
        fprintf(f, "%s", str_values[index]);
    else
        fprintf(f, "%s", vars[index]);
}

int main() {
    void *f;

    types = malloc(sizeof(char *) * COUNT);
    vars = malloc(sizeof(char *) * COUNT);
    is_int = malloc(sizeof(int) * COUNT);
    str_types = malloc(sizeof(char *) * COUNT);
    storage = malloc(sizeof(int) * COUNT);
    str_storage = malloc(sizeof(char *) * 4);
    str_values = malloc(sizeof(char *) * COUNT);
    ld_values = malloc(sizeof(long double) * COUNT);
    types = malloc(sizeof(int) * COUNT);
    asserts = malloc(sizeof(char *) * 4);

    str_types[ 0] = "int";         vars[ 0] = "i";   types[ 0] = TYPE_INT;    storage[ 0] = LOCAL;    str_values[ 0] = "1";     ld_values[ 0] = 1;
    str_types[ 1] = "float";       vars[ 1] = "f";   types[ 1] = TYPE_FLOAT;  storage[ 1] = LOCAL;    str_values[ 1] = "2.1f";  ld_values[ 1] = 2.1;
    str_types[ 2] = "double";      vars[ 2] = "d";   types[ 2] = TYPE_DOUBLE; storage[ 2] = LOCAL;    str_values[ 2] = "3.1";   ld_values[ 2] = 3.1;
    str_types[ 3] = "long double"; vars[ 3] = "ld";  types[ 3] = TYPE_LD;     storage[ 3] = LOCAL;    str_values[ 3] = "4.1l";  ld_values[ 3] = 4.1;
    str_types[ 4] = "int";         vars[ 4] = "gi";  types[ 4] = TYPE_INT;    storage[ 4] = GLOBAL;   str_values[ 4] = "5";     ld_values[ 4] = 5;
    str_types[ 5] = "float";       vars[ 5] = "gf";  types[ 5] = TYPE_FLOAT;  storage[ 5] = GLOBAL;   str_values[ 5] = "6.1f";  ld_values[ 5] = 6.1;
    str_types[ 6] = "double";      vars[ 6] = "gd";  types[ 6] = TYPE_DOUBLE; storage[ 6] = GLOBAL;   str_values[ 6] = "7.1";   ld_values[ 6] = 7.1;
    str_types[ 7] = "long double"; vars[ 7] = "gld"; types[ 7] = TYPE_LD;     storage[ 7] = GLOBAL;   str_values[ 7] = "8.1l";  ld_values[ 7] = 8.1;
    str_types[ 8] = "int";         vars[ 8] = 0;     types[ 8] = TYPE_INT;    storage[ 8] = CONSTANT; str_values[ 8] = "9";     ld_values[ 8] = 9;
    str_types[ 9] = "float";       vars[ 9] = 0;     types[ 9] = TYPE_FLOAT;  storage[ 9] = CONSTANT; str_values[ 9] = "10.1f"; ld_values[ 9] = 10.1;
    str_types[10] = "double";      vars[10] = 0;     types[10] = TYPE_DOUBLE; storage[10] = CONSTANT; str_values[10] = "11.1";  ld_values[10] = 11.1;
    str_types[11] = "long double"; vars[11] = 0;     types[11] = TYPE_LD;     storage[11] = CONSTANT; str_values[11] = "12.1l"; ld_values[11] = 12.1;

    asserts[ 0] = "assert_int";
    asserts[ 1] = "assert_float";
    asserts[ 2] = "assert_double";
    asserts[ 3] = "assert_long_double";

    str_storage[CONSTANT] = "constant";
    str_storage[LOCAL] = "local";
    str_storage[GLOBAL] = "global";

    f = fopen("test-arithmetic-torture.c", "w");

    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "\n");
    fprintf(f, "#include \"../test-lib.h\"\n");
    fprintf(f, "\n");
    fprintf(f, "int failures;\n");
    fprintf(f, "int passes;\n");
    fprintf(f, "int verbose;\n");
    fprintf(f, "\n");

    // Global declarations
    for (int i = 0; i < COUNT; i++)
        if (storage[i] == GLOBAL)
            fprintf(f, "%s %s;\n", str_types[i], vars[i]);

    fprintf(f, "\n");

    fprintf(f, "int main() {\n");

    // Local declarations & assignments
    for (int i = 0; i < COUNT; i++)
        if (storage[i] == LOCAL)
            fprintf(f, "    %s %s = %s;\n", str_types[i], vars[i], str_values[i]);

    // Global assignments
    for (int i = 0; i < COUNT; i++)
        if (storage[i] == GLOBAL) fprintf(f, "    %s = %s;\n", vars[i], str_values[i]);

    fprintf(f, "\n");
    fprintf(f, "    failures = 0;\n");
    fprintf(f, "\n");

    for (int src = 0; src < COUNT; src++) // src
        for (int dst = 0; dst < COUNT; dst++) { // dst
            if (storage[dst] == CONSTANT) continue;

            int dst_type = types[dst];
            int src_type = types[src];
            int common_type = dst_type > src_type ? dst_type : src_type;
            fprintf(f, "    %s(%s + %s, ", asserts[common_type], str_values[src], str_values[dst]);
            output_value(f, src);
            fprintf(f, " + ");
            output_value(f, dst);
            fprintf(f, ", \"%s %s + %s %s\");\n",str_storage[storage[src]], str_types[src], str_storage[storage[dst]], str_types[dst]);
        }

    fprintf(f, "\n");
    fprintf(f, "    if (failures) {\n");
    fprintf(f, "        printf(\"Failures: %%d\\n\", failures);\n");
    fprintf(f, "        exit(1);\n");
    fprintf(f, "    }\n");
    fprintf(f, "    else\n");
    fprintf(f, "        printf(\"Arithmetic torture tests passed\\n\");\n");
    fprintf(f, "}\n");

    fclose(f);
}
