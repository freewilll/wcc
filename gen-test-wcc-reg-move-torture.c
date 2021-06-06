#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

// This started its life a test for pointer indirects with all kinds of mixed types
// before I realized that pointer indirects and moves have been split, and there are
// no longer any rules that indirect and sign/zero extend at the same time. Nevertheless,
// this is a good stress test for the move machinery.

char **types;
char **unsigned_types;
char **vars;
int *sizes;
int *is_signed;
int *is_ptrs;
unsigned long *sized_outcomes;

enum {
    COUNT = 16,
};

// What is the result of casting src -> dst -> unsigned long
unsigned long get_outcome(int dst, int src) {
    int src_size, dst_size;

    src_size = sizes[src];
    dst_size = sizes[dst];

    if (is_signed[src] && is_signed[dst]) return sized_outcomes[3];
    else if (!is_signed[src] && is_signed[dst] && src_size >= dst_size) return sized_outcomes[3];
    else if (is_signed[src] && !is_signed[dst] && src_size <= dst_size) return sized_outcomes[dst_size - 1];
    else if (dst_size <= src_size) return sized_outcomes[dst_size - 1];
    else return sized_outcomes[src_size - 1];
}

int main() {
    int i, j;
    unsigned long outcome;
    void *f;

    types = malloc(sizeof(char *) * COUNT * 2);
    unsigned_types = malloc(sizeof(char *) * COUNT * 2);
    vars = malloc(sizeof(char *) * COUNT * 2);
    sizes = malloc(sizeof(int) * COUNT * 2);
    is_signed = malloc(sizeof(int) * COUNT * 2);
    is_ptrs = malloc(sizeof(int) * COUNT * 2);

    types[0]  = "char";           vars[0]  = "c";          is_signed[0]  = 1; sizes[0]  = 1; is_ptrs[0]  = 0;
    types[1]  = "short";          vars[1]  = "s";          is_signed[1]  = 1; sizes[1]  = 2; is_ptrs[1]  = 0;
    types[2]  = "int";            vars[2]  = "i";          is_signed[2]  = 1; sizes[2]  = 3; is_ptrs[2]  = 0;
    types[3]  = "long";           vars[3]  = "l";          is_signed[3]  = 1; sizes[3]  = 4; is_ptrs[3]  = 0;
    types[4]  = "unsigned char";  vars[4]  = "uc";         is_signed[4]  = 0; sizes[4]  = 1; is_ptrs[4]  = 0;
    types[5]  = "unsigned short"; vars[5]  = "us";         is_signed[5]  = 0; sizes[5]  = 2; is_ptrs[5]  = 0;
    types[6]  = "unsigned int";   vars[6]  = "ui";         is_signed[6]  = 0; sizes[6]  = 3; is_ptrs[6]  = 0;
    types[7]  = "unsigned long";  vars[7]  = "ul";         is_signed[7]  = 0; sizes[7]  = 4; is_ptrs[7]  = 0;
    types[8]  = "char *";         vars[8]  = "pc";         is_signed[8]  = 1; sizes[8]  = 4; is_ptrs[8]  = 1;
    types[9]  = "short *";        vars[9]  = "ps";         is_signed[9]  = 1; sizes[9]  = 4; is_ptrs[9]  = 1;
    types[10] = "int *";          vars[10] = "pi";         is_signed[10] = 1; sizes[10] = 4; is_ptrs[10] = 1;
    types[11] = "long *";         vars[11] = "pl";         is_signed[11] = 1; sizes[11] = 4; is_ptrs[11] = 1;
    types[12] = "char *";         vars[12] = "puc";        is_signed[12] = 1; sizes[12] = 4; is_ptrs[12] = 1;
    types[13] = "short *";        vars[13] = "pus";        is_signed[13] = 1; sizes[13] = 4; is_ptrs[13] = 1;
    types[14] = "int *";          vars[14] = "pui";        is_signed[14] = 1; sizes[14] = 4; is_ptrs[14] = 1;
    types[15] = "long *";         vars[15] = "pul";        is_signed[15] = 1; sizes[15] = 4; is_ptrs[15] = 1;

    for (i = 0; i < 4; i++) asprintf(&unsigned_types[i], "unsigned %s", types[i]);
    for (i = 4; i < COUNT; i++) unsigned_types[i] = types[i];

    for (i = 0; i < COUNT; i++) {
        types[i + 16] = types[i];
        unsigned_types[i + 16] = unsigned_types[i];
        sizes[i + 16] = sizes[i];
        is_signed[i + 16] = is_signed[i];
        is_ptrs[i + 16] = is_ptrs[i];
        asprintf(&vars[i + 16], "g%s", vars[i]);
    }

    sized_outcomes = malloc(sizeof(unsigned long) * 4);
    sized_outcomes[0] = 0xff;
    sized_outcomes[1] = 0xffff;
    sized_outcomes[2] = 0xffffffff;
    sized_outcomes[3] = 0xffffffffffffffff;

    f = fopen("test-wcc-reg-move-torture.c", "w");

    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "\n");
    fprintf(f, "int failures;\n");
    fprintf(f, "int passes;\n");
    fprintf(f, "int verbose;\n");
    fprintf(f, "\n");

    // Global declarations
    for (i = 0; i < COUNT; i++) fprintf(f, "%s %s;\n", types[COUNT + i], vars[COUNT + i]);

    fprintf(f, "\n");

    for (i = 0; i < COUNT * 2; i++) // dst
        for (j = 0; j < COUNT * 2; j++) { // src
                fprintf(f, "// (unsigned long) (%s) %s\n", types[i], vars[j]);
                fprintf(f, "static void func_%d_%d() {\n", i, j);
                fprintf(f, "    %s %s;\n", types[j], vars[j]);
                outcome = get_outcome(i, j);
                fprintf(f, "    %s = -1;\n", vars[j]);
                fprintf(f, "    if ((unsigned long) (%s) %s != 0x%lxul) {\n", types[i], vars[j], outcome);
                fprintf(f, "        printf(\"Failed %5s = %5s: expected 0x%%lx, got 0x%%lx\\n\", 0x%lx, (unsigned long) (%s) %s);\n",
                    vars[i], vars[j], outcome, types[i], vars[j]);
                fprintf(f,"        failures++;\n");
                fprintf(f, "    }\n");
                fprintf(f, "}\n\n");
            }

    fprintf(f, "\n");
    fprintf(f, "int main() {\n");
    fprintf(f, "    unsigned long result;\n");

    // Local declarations
    for (i = 0; i < COUNT; i++) fprintf(f, "    %s %s;\n", types[i], vars[i]);

    fprintf(f, "\n");
    fprintf(f, "    failures = 0;\n");
    fprintf(f, "\n");
    fprintf(f, "\n");

    for (i = 0; i < COUNT * 2; i++) // dst
        for (j = 0; j < COUNT * 2; j++) // src
                fprintf(f, "    func_%d_%d();\n", i, j);

    fprintf(f, "\n");
    fprintf(f, "    if (failures) {\n");
    fprintf(f, "        printf(\"Failures: %%d\\n\", failures);\n");
    fprintf(f, "        exit(1);\n");
    fprintf(f, "    }\n");
    fprintf(f, "    else\n");
    fprintf(f, "        printf(\"Reg move torture tests passed\\n\");\n");
    fprintf(f, "}\n");

    fclose(f);
}
