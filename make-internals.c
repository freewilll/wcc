#include <stdio.h>
#include <stdlib.h>

void die(char *message) {
    puts(message);
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: make-internals PATH-TO-internals.h\n");
        exit(1);
    }

    char *internals_h = argv[1];

    FILE *f = fopen(internals_h, "r");
    if (!f) die("Opening internals.h");

    char *buffer = malloc(1024 * 1024);
    int count = fread(buffer, 1024 * 1024, 1, f);
    if (count < 0) die("Reading internals.h");

    printf("char *internals(void) {\n");
    printf("    return \"");

    char *p = buffer;
    while (*p) {
        if (*p == '\n')
            printf("\\n");
        else
            putchar(*p);

        p++;
    }

    printf("\";\n}\n");

    fclose(f);
}
