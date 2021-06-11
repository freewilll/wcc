#include <stdlib.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

struct s1 {
    int i;
};

typedef struct s1 S1;

typedef struct _s2_ { int i; } S2;

void test_typedef1() {
    S1 *s;

    s = malloc(sizeof(S1));
    s->i = 1;
    assert_int(1, s->i, "Struct typedef 1");
}

void test_typedef2() {
    S2 *s;

    s = malloc(sizeof(S2));
    s->i = 1;
    assert_int(1, s->i, "Struct typedef 2");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_typedef1();
    test_typedef2();

    finalize();
}
