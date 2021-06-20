#include "../test-lib.h"

int verbose;
int passes;
int failures;

void test_ifdef() {
    int i = 0;

    #ifdef TEST
    i = 1;
    #endif
    assert_int(0, i, "ifdef/endif with directive undefined");

    #define TEST
    #ifdef TEST
    i = 1;
    #endif
    assert_int(1, i, "ifdef/endif with directive defined");

    #undef TEST
    #ifdef TEST
    i = 1;
    #endif
    assert_int(0, 0, "ifdef/endif with directive undefined");

    #ifdef TEST
    i = 1;
    #else
    i = 2;
    #endif
    assert_int(2, i, "ifdef/else/endif with directive undefined");

    #define TEST
    #ifdef TEST
    i = 1;
    #else
    i = 2;
    #endif
    assert_int(1, i, "ifdef/else/endif with directive defined");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_ifdef();

    finalize();
}
