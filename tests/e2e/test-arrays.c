#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

static void test_sizeof() {
    assert_int(12, sizeof("Hello world"), "sizeof Hello world");

    char *hw = "Hello world";
    assert_string("ello world", hw + 1, "ello world");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_sizeof();

    finalize();
}
