#include "../test-lib.h"

int verbose;
int passes;
int failures;

// Global enums
enum ge0 {GE0A, GE0B, GE0C};
enum ge1 {GE1A=1, GE1B, GE1C};

void test_enums() {
    assert_int(0, GE0A, "Enums GE0 1");
    assert_int(1, GE0B, "Enums GE0 2");
    assert_int(2, GE0C, "Enums GE0 3");

    assert_int(1, GE1A, "Enums GE1 1");
    assert_int(2, GE1B, "Enums GE1 2");
    assert_int(3, GE1C, "Enums GE1 3");

    enum e0 {E0A, E0B, E0C};                // Intentional no trailing comma
    assert_int(0, E0A, "Enums E0 1");
    assert_int(1, E0B, "Enums E0 2");
    assert_int(2, E0C, "Enums E0 3");

    enum e1 {
        E1A=1,
        E1B,
        E1C
    };
    assert_int(1, E1A, "Enums E1 1");
    assert_int(2, E1B, "Enums E1 2");
    assert_int(3, E1C, "Enums E1 3");

    enum e2 {
        E2A,
        E2B=2,
        E2C
    };
    assert_int(0, E2A, "Enums E2 1");
    assert_int(2, E2B, "Enums E2 2");
    assert_int(3, E2C, "Enums E2 3");

    enum e3 {
        E3A,
        E3B=2,
        E3C,
        E3D,
        E3E=3,
        E3F,                                // Intentional trailing comma
    };
    assert_int(0, E3A, "Enums E3 1");
    assert_int(2, E3B, "Enums E3 2");
    assert_int(3, E3C, "Enums E3 3");
    assert_int(4, E3D, "Enums E3 4");
    assert_int(3, E3E, "Enums E3 5");
    assert_int(4, E3F, "Enums E3 6");

    enum e5 {E5A, E5B};
    enum e5 v5;
    v5 = 100; assert_int(100, v5, "Enums E4 1");
    v5 = E5A; assert_int(E5A, v5, "Enums E4 2");
    v5 = E5B; assert_int(E5B, v5, "Enums E4 3");

    enum {EA1A, EA1B}  a1;
    enum {EA2A, EA2B,} a2;
    a1 = EA1A; assert_int(0, a1, "Enums EA1 1");
    a1 = EA1B; assert_int(1, a1, "Enums EA1 2");
    a2 = EA2A; assert_int(0, a2, "Enums EA2 1");
    a2 = EA2B; assert_int(1, a2, "Enums EA2 2");

    enum direct_decl3 {EA3A, EA3B}  a3;
    enum direct_decl4 {EA4A, EA4B,} a4;
    a3 = EA3A; assert_int(0, a3, "Enums EA3 1");
    a3 = EA3B; assert_int(1, a3, "Enums EA3 2");
    a4 = EA4A; assert_int(0, a4, "Enums EA4 1");
    a4 = EA4B; assert_int(1, a4, "Enums EA4 2");

    assert_int(4, sizeof(enum {EA5A, EA5B}), "Sizeof enum 1");
    assert_int(4, sizeof(enum e5),           "Sizeof enum 2");

    // Test separate declaration and var declaration works
    enum e6;
    enum e6 {E6A, E6B};
    enum e6 v6;
    v6 = E6A; assert_int(0, v6, "Enums E6 1");
    v6 = E6B; assert_int(1, v6, "Enums E6 2");

    assert_int(4, sizeof(struct s {enum {A} v;}), "Sizeof struct of enum 1");
    assert_int(4, sizeof(enum e6), "Sizeof struct of enum 2");

    enum e7 {
        E7A=-2,
        E7B,
        E7C,
        E7D,
    };
    assert_int(-2, E7A, "Enums E7 1");
    assert_int(-1, E7B, "Enums E7 2");
    assert_int(0,  E7C, "Enums E7 3");
    assert_int(1,  E7D, "Enums E7 4");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_enums();

    finalize();
}
