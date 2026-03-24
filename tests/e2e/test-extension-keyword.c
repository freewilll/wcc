#include "../test-lib.h"

int verbose;
int passes;
int failures;

__extension__ int x = 42;

// This test doesn't do much, only check that __extension__ keywords scattered around
// compiles fine.

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    assert_int(42, x, "x == 42");
    assert_int(42, __extension__ 42, "__extension__ 42 == 42");

    struct s { __extension__ int i; } s = {42};
    assert_int(42, s.i, "struct s { __extension__ int i; } s.i == 42");


    finalize();
}
