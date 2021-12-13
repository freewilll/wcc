#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Report an internal error and exit
void panic(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "Internal error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
