#include <stdio.h>
#include <stdlib.h>

#include "wcc.h"

void assert(int expected, int actual) {
    if (expected != actual) {
        printf("expected %d, got %d\n", expected, actual);
        exit(1);
    }
}

void test_add_delete() {
    Set *s;

    s = new_set(2);

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
    Set *s1, *s2, *s3;

    s1 = new_set(3); add_to_set(s1, 1); add_to_set(s1, 2);
    s2 = new_set(3); add_to_set(s2, 1); add_to_set(s2, 3);

    s3 = set_intersection(s1, s2);
    assert(1, in_set(s3, 1));
    assert(0, in_set(s3, 2));
    assert(0, in_set(s3, 3));

    s3 = set_union(s1, s2);
    assert(1, in_set(s3, 1));
    assert(1, in_set(s3, 2));
    assert(1, in_set(s3, 3));

    s3 = set_difference(s1, s2);
    assert(0, in_set(s3, 1));
    assert(1, in_set(s3, 2));
    assert(0, in_set(s3, 3));
}

void test_set_eq() {
    Set *s1, *s2, *s3;

    s1 = new_set(3); add_to_set(s1, 1); add_to_set(s1, 2);
    s2 = new_set(3); add_to_set(s2, 1); add_to_set(s2, 3);
    assert(0, set_eq(s1, s2));

    s2 = new_set(3); add_to_set(s2, 1); add_to_set(s2, 2);
    assert(1, set_eq(s1, s2));
}

void test_cache_elements() {
    Set *s;

    s = new_set(3); add_to_set(s, 1); add_to_set(s, 2);
    cache_set_elements(s);
    assert(2, s->cached_element_count);
    assert(1, s->cached_elements[0]);
    assert(2, s->cached_elements[1]);

    add_to_set(s, 3);
    cache_set_elements(s);
    assert(3, s->cached_element_count);
    assert(1, s->cached_elements[0]);
    assert(2, s->cached_elements[1]);
    assert(3, s->cached_elements[2]);

    delete_from_set(s, 2);
    cache_set_elements(s);
    assert(2, s->cached_element_count);
    assert(1, s->cached_elements[0]);
    assert(3, s->cached_elements[1]);
}

int main() {
    test_add_delete();
    test_merges();
    test_set_eq();
    test_cache_elements();
}
