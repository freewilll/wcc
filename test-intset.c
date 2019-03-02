#include <stdio.h>

#include "wc4.h"

void assert(int expected, int actual, char *message) {
    if (expected != actual) {
        printf("%s: expected %d, got %d\n", message, expected, actual);
        exit(1);
    }
}

void test_add_delete() {
    struct intset *s;

    s = new_intset();

    add_to_set(s, 1);
    assert(0, in_set(s, 0));
    assert(1, in_set(s, 1));
    assert(0, in_set(s, 2));

    add_to_set(s, 2);
    assert(0, in_set(s, 0));
    assert(1, in_set(s, 1));
    assert(1, in_set(s, 2));

    delete_from_set(s, 1);
    assert(0, in_set(s, 0));
    assert(0, in_set(s, 1));
    assert(1, in_set(s, 2));
}

void test_merges() {
    struct intset *s1, *s2, *s3;

    s1 = new_intset(); add_to_set(s1, 1); add_to_set(s1, 2);
    s2 = new_intset(); add_to_set(s1, 1); add_to_set(s1, 3);

    s3 = set_intersection(s1, s3);
    assert(1, in_set(s3, 1));
    assert(0, in_set(s3, 2));
    assert(0, in_set(s3, 3));

    s3 = set_union(s1, s3);
    assert(1, in_set(s3, 1));
    assert(1, in_set(s3, 2));
    assert(1, in_set(s3, 3));

    s3 = set_difference(s1, s3);
    assert(0, in_set(s3, 1));
    assert(1, in_set(s3, 2));
    assert(0, in_set(s3, 3));
}

int main() {
    test_add_delete();
}