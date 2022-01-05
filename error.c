#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "wcc.h"

// ANSI color codes
#define LOCUS "\e[0;01m" // Bold
#define BRED "\e[1;31m"  // Bright red
#define BMAG "\e[1;35m"  // Bright magenta
#define RESET "\e[0m"    // Reset

static void print_filename_and_linenumber(int is_tty) {
    if (is_tty) fprintf(stderr, LOCUS);
    fprintf(stderr, "%s:%d: ", cur_filename, cur_line);
    if (is_tty) fprintf(stderr, RESET);
}

void panic_with_line_number(char *format, ...) {
    va_list ap;
    va_start(ap, format);

    int is_tty = isatty(2);
    print_filename_and_linenumber(is_tty);
    if (is_tty) fprintf(stderr, BRED);
    fprintf(stderr, "internal error: ");
    if (is_tty) fprintf(stderr, RESET);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}


// Report an error by itself (no filename or line number) and exit
void simple_error(char *format, ...) {
    va_list ap;
    va_start(ap, format);

    int is_tty = isatty(2);
    if (is_tty) fprintf(stderr, BRED);
    fprintf(stderr, "error: ");
    if (is_tty) fprintf(stderr, RESET);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

// Report an error with a filename and line number and exit
void _error(char *format, va_list ap) {
    if (compile_phase == CP_PREPROCESSING) get_cpp_filename_and_line();

    int is_tty = isatty(2);
    print_filename_and_linenumber(is_tty);
    if (is_tty) fprintf(stderr, BRED);
    fprintf(stderr, "error: ");
    if (is_tty) fprintf(stderr, RESET);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void error(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    _error(format, ap);
}

// Report a warning with a filename and line number
void warning(char *format, ...) {
    va_list ap;
    va_start(ap, format);

    if (opt_warnings_are_errors) _error(format, ap);

    if (compile_phase == CP_PREPROCESSING) get_cpp_filename_and_line();

    int is_tty = isatty(2);
    print_filename_and_linenumber(is_tty);
    if (is_tty) fprintf(stderr, BMAG);
    fprintf(stderr, "warning: ");
    if (is_tty) fprintf(stderr, RESET);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
