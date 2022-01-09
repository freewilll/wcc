#include <stdio.h>
#include <stdlib.h>

#include "../../wcc.h"

void assert_int(int expected, int actual, char *message) {
    if (expected != actual) {
        printf("%s: expected %d, got %d\n", message, expected, actual);
        exit(1);
    }
}

int test_append(int initial_length) {
    List *l = new_list(initial_length);

    append_to_list(l, (void *) 1);
    assert_int(1, l->length, "Append 1a");
    assert_int(1, (long) l->elements[0], "Append 1c");

    append_to_list(l, (void *) 2);
    assert_int(2, l->length, "Append 2a");
    assert_int(1, (long) l->elements[0], "Append 2c");
    assert_int(2, (long) l->elements[1], "Append 2d");

    append_to_list(l, (void *) 3);
    append_to_list(l, (void *) 4);

    assert_int(4, l->length, "Append 3a");
    assert_int(1, (long) l->elements[0], "Append 2c");
    assert_int(2, (long) l->elements[1], "Append 3d");
    assert_int(3, (long) l->elements[2], "Append 3e");
    assert_int(4, (long) l->elements[3], "Append 3f");
}

int main() {
    test_append(0);
    test_append(2);
    test_append(4);
    test_append(8);
}
