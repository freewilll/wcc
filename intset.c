#include "wc4.h"

struct intset *new_intset() {
    struct intset *result;

    result = malloc(sizeof(struct intset *));
    memset(result, 0, sizeof(struct intset *));
    result->elements = malloc(MAX_INT_SET_ELEMENTS * sizeof(int));
    memset(result->elements, 0, MAX_INT_SET_ELEMENTS * sizeof(int));

    return result;
}

void *add_to_set(struct intset *s, int value) {
    s->elements[value] = 1;
}

void *delete_from_set(struct intset *s, int value) {
    s->elements[value] = 0;
}

int *in_set(struct intset *s, int value) {
    return s->elements[value] == 1;
}

struct intset *set_intersection(struct intset *s1, struct intset *s2) {
    struct intset *result;
    int i;

    result = new_intset();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        if (s1->elements[i] && s2->elements[i]) result->elements[i] = 1;

    return result;
}

struct intset *set_union(struct intset *s1, struct intset *s2) {
    struct intset *result;
    int i;

    result = new_intset();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        if (s1->elements[i] || s2->elements[i]) result->elements[i] = 1;

    return result;
}

struct intset *set_difference(struct intset *s1, struct intset *s2) {
    struct intset *result;
    int i;

    result = new_intset();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        if (s1->elements[i] && !s2->elements[i]) result->elements[i] = 1;

    return result;
}
