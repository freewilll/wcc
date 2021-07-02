#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

#ifdef FLOATS
long double gld;

void test_assignment() {
    long double ld1, ld2;
    char *buffer;

    ld1 = 1.1;

    ld2 = ld1;
    buffer = malloc(100);
    sprintf(buffer, "%5.5Lf", ld2);
    assert_int(0, strcmp(buffer, "1.10000"), "Long double assignment local-local");

    gld = ld1;
    buffer = malloc(100);
    sprintf(buffer, "%5.5Lf", gld);
    assert_int(0, strcmp(buffer, "1.10000"), "Long double assignment local-global");

    gld = 1.2;
    ld1 = gld;
    buffer = malloc(100);
    sprintf(buffer, "%5.5Lf", ld1);
    assert_int(0, strcmp(buffer, "1.20000"), "Long double assignment global-local");

    gld = 1.3;
    ld1 = ld2 = gld;
    buffer = malloc(100);
    sprintf(buffer, "%5.5Lf %5.5Lf", ld1, ld2);
    assert_int(0, strcmp(buffer, "1.30000 1.30000"), "Long double assignment global-local-local");
}
#endif

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    #ifdef FLOATS
    test_assignment();
    #endif

    finalize();
}
