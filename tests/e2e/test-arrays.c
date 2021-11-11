#include <stdlib.h>
#include <string.h>

#include "../test-lib.h"

int verbose;
int passes;
int failures;

char gi[10];

struct { int i, j; } gastr[10];

static void test_sizeof() {
    assert_int(12, sizeof("Hello world"), "sizeof Hello world");

    char *hw = "Hello world";
    assert_string("ello world", hw + 1, "ello world");
}

static void test_local_vars() {
    int ok;

    char ac[10];
    for (int i = 0; i < 10; i++) ac[i] = (i + 1) * 10;
    ok = 1; for (int i = 0; i < 10; i++) ok = ok && ac[i] == (i + 1) * 10;
    assert_int(1, ok, "Local char array assignment");

    short as[10];
    for (int i = 0; i < 10; i++) as[i] = (i + 1) * 10;
    ok = 1; for (int i = 0; i < 10; i++) ok = ok && as[i] == (i + 1) * 10;
    assert_int(1, ok, "Local short array assignment");

    int ai[10];
    for (int i = 0; i < 10; i++) ai[i] = (i + 1) * 10;
    ok = 1; for (int i = 0; i < 10; i++) ok = ok && ai[i] == (i + 1) * 10;
    assert_int(1, ok, "Local int array assignment");

    long al[10];
    for (int i = 0; i < 10; i++) al[i] = (i + 1) * 10;
    ok = 1; for (int i = 0; i < 10; i++) ok = ok && al[i] == (i + 1) * 10;
    assert_int(1, ok, "Local long array assignment");

    float af[10];
    for (int i = 0; i < 10; i++) af[i] = (i + 1) * 10.1;
    ok = 1; for (int i = 0; i < 10; i++) ok = ok && float_eq(af[i], (i + 1) * 10.1);
    assert_int(1, ok, "Local float array assignment");

    double ad[10];
    for (int i = 0; i < 10; i++) ad[i] = (i + 1) * 10.1;
    ok = 1; for (int i = 0; i < 10; i++) ok = ok && float_eq(ad[i], (i + 1) * 10.1);
    assert_int(1, ok, "Local double array assignment");

    long double ald[10];
    for (int i = 0; i < 10; i++) ald[i] = (i + 1) * 10.1;
    ok = 1; for (int i = 0; i < 10; i++) ok = ok && float_eq(ald[i], (i + 1) * 10.1);
    assert_int(1, ok, "Local long double array assignment");

    // Array of structs
    struct { int i, j; } astr[10];
    for (int i = 0; i < 10; i++) {
        astr[i].i = (i + 1) * 10;
        astr[i].j = (i + 1) * 20;
    }
    ok = 1;
    for (int i = 0; i < 10; i++) ok = ok && float_eq(astr[i].i, (i + 1) * 10);
    for (int i = 0; i < 10; i++) ok = ok && float_eq(astr[i].j, (i + 1) * 20);
    assert_int(1, ok, "Local struct array assignment");
}

static void test_global_vars() {
    // Array of ints
    int gai[10];
    for (int i = 0; i < 10; i++) gai[i] = (i + 1) * 10;
    int ok = 1; for (int i = 0; i < 10; i++) ok = ok && gai[i] == (i + 1) * 10;
    assert_int(1, ok, "Global int array assignment");

    // Array of structs
    struct { int i, j; } gastr[10];
    for (int i = 0; i < 10; i++) {
        gastr[i].i = (i + 1) * 10;
        gastr[i].j = (i + 1) * 20;
    }
    ok = 1;
    for (int i = 0; i < 10; i++) ok = ok && float_eq(gastr[i].i, (i + 1) * 10);
    for (int i = 0; i < 10; i++) ok = ok && float_eq(gastr[i].j, (i + 1) * 20);
    assert_int(1, ok, "Global struct array assignment");
}

static void test_address_of() {
    int i[10];
    assert_int(40, sizeof(*&i), "Sizeof int i[10]");
}

static void test_multiple_dimensions() {
    char a[4][2];
    assert_int(8, sizeof(a), "sizeof char a[4][2]");

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 2; j++)
            a[i][j] = i * 2 + j * 3;

    assert_long(0x5020300, *((long *) &a), "4x2 array");
}

static void test_compound_assignment() {
    int a[2];
    a[0] = 1;
    a[1] = 2;

    assert_int(1, a[0]++, "a[0]++"); assert_int(2, a[0], "a[0]++");
    assert_int(2, a[0]--, "a[0]++"); assert_int(1, a[0], "a[0]--");
    assert_int(2, ++a[0], "a[0]++"); assert_int(2, a[0], "++a[0]");
    assert_int(1, --a[0], "a[0]++"); assert_int(1, a[0], "--a[0]");

    assert_int(2, a[1]++, "a[1]++"); assert_int(3, a[1], "a[1]++");
    assert_int(3, a[1]--, "a[1]++"); assert_int(2, a[1], "a[1]--");
    assert_int(3, ++a[1], "a[1]++"); assert_int(3, a[1], "++a[1]");
    assert_int(2, --a[1], "a[1]++"); assert_int(2, a[1], "--a[1]");

    a[0] += 10; assert_int(11, a[0], "a[0] += 10");
    a[1] += 10; assert_int(12, a[1], "a[1] += 10");
}

static void test_arithmetic() {
    int a[3];

    a[0] = 1;
    a[1] = 2;
    a[2] = 3;

    assert_int(1, *a,       "a");
    assert_int(1, *(a + 0), "(a + 0)");
    assert_int(2, *(a + 1), "(a + 1)");
    assert_int(3, *(a + 2), "(a + 2)");

    assert_int(1, &(a[1]) - a, "&(a[1]) - a");
    assert_int(2, &(a[2]) - a, "&(a[2]) - a");
}

static void test_struct() {
    union {
        struct { char c[8]; } s1;
        long l;
    } u1;
    for (int i = 0; i < 8; i++) u1.s1.c[i] = i + 1;
    assert_int(0x4030201, u1.l, "char c[8] in struct 1");

    union {
        struct { int i; char c[4]; } s1;
        long l;
    } u2;
    u2.s1.i = -1;
    for (int i = 0; i < 4; i++) u2.s1.c[i] = i + 1;
    assert_long(0x4030201ffffffff, u2.l, "int; char c[4] in struct 2");
}

void accept_array_with_address_of(int a[2]) {
    &a;
    assert_int(1, a[0], "accept_array_with_address_of 1");
    assert_int(2, a[1], "accept_array_with_address_of 2");
}

void test_function_array_param_with_address_of() {
    int a[2];
    a[0] = 1;
    a[1] = 2;

    accept_array_with_address_of(a);
}

void test_array_lvalues() {
    int arr[2];
    assert_int(&arr, &arr[0],                              "& on an array 1");
    assert_int(arr + 1, &arr[1],                           "& on an array 2");
    assert_int(sizeof(arr[1]), (int) &arr[1] - (int) &arr, "& on an array 3");
}

void test_negative_array_lookup() {
    // Negative lookup
    int i[3] = {1, 2, 3};
    assert_int(1, i[0], "Negative array lookup 1");
    assert_int(2, i[1], "Negative array lookup 2");
    assert_int(3, i[2], "Negative array lookup 3");
    int *pi = &i[3];
    assert_int(3, pi[-1], "Negative array lookup 4");
    assert_int(2, pi[-2], "Negative array lookup 5");
    assert_int(1, pi[-3], "Negative array lookup 6");
}

int main(int argc, char **argv) {

    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_sizeof();
    test_local_vars();
    test_global_vars();
    test_address_of();
    test_compound_assignment();
    test_arithmetic();
    test_struct();
    test_function_array_param_with_address_of();
    test_array_lvalues();
    test_negative_array_lookup();

    finalize();
}
