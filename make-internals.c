#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void die(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: make-internals internals1.h [internals2.h] [...]\n");
        exit(1);
    }

    printf("char *internals(void) {\n");
    printf("    return \"");

    char **internals_h = &argv[1];
    while (*internals_h) {
        FILE *f = fopen(*internals_h, "r");
        if (!f) die("Unable to open %s", *internals_h);

        char *buffer = malloc(1024 * 1024);
        int count = fread(buffer, 1024 * 1024, 1, f);
        if (count < 0) die("Reading internals.h");

        char *p = buffer;
        while (*p) {
            if (*p == '\n')
                printf("\\n");
            else
                putchar(*p);

            p++;
        }

        fclose(f);

        internals_h++;
    }

    printf("\";\n}\n");
}
