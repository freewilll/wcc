#include "../test-lib.h"

int verbose;
int passes;
int failures;

enum {A, B};
enum {C=2, D};
enum {E=-2, F};

void test_enum() {
    assert_int(4, sizeof(A), "sizeof");

    assert_int(0,  A, "enum 1");
    assert_int(1,  B, "enum 2");
    assert_int(2,  C, "enum 3");
    assert_int(3,  D, "enum 4");
    assert_int(-2, E, "enum 5");
    assert_int(-1, F, "enum 6");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_enum();

    finalize();
}
