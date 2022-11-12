#include <stdio.h>
#include <stdlib.h>

void die(char *message) {
    puts(message);
    exit(1);
}

int main() {
    FILE *f = fopen("internals.h", "r");
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
