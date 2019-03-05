#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

struct intset *new_intset() {
    struct intset *result;

    result = malloc(sizeof(struct intset *));
    memset(result, 0, sizeof(struct intset *));
    result->elements = malloc(MAX_INT_SET_ELEMENTS * sizeof(int));
    memset(result->elements, 0, MAX_INT_SET_ELEMENTS * sizeof(int));

    return result;
}

void empty_set(struct intset *s) {
    int i;

    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++) s->elements[i] = 0;
}

struct intset *copy_intset(struct intset *s) {
    struct intset *result;
    int i;

    result = new_intset();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++) result->elements[i] = s->elements[i];

    return result;
}

int set_len(struct intset *s) {
    int i, result;

    result = 0;
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++) if (s->elements[i]) result++;

    return result;
}

void print_set(struct intset *s) {
    int i;
    int first;

    first = 1;
    printf("{");
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++) {
        if (s->elements[i]) {
            if (!first) { printf(", "); }
            printf("%d", i);
            first = 0;
        }
    }
    printf("}");
}

void *add_to_set(struct intset *s, int value) {
    s->elements[value] = 1;
}

void *delete_from_set(struct intset *s, int value) {
    s->elements[value] = 0;
}

int in_set(struct intset *s, int value) {
    return s->elements[value] == 1;
}

int set_eq(struct intset *s1, struct intset *s2) {
    int i;

    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        if (s1->elements[i] != s2->elements[i]) return 0;

    return 1;
}

struct intset *set_intersection(struct intset *s1, struct intset *s2) {
    struct intset *result;
    int i;

    result = new_intset();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        result->elements[i] = s1->elements[i] && s2->elements[i];

    return result;
}

struct intset *set_union(struct intset *s1, struct intset *s2) {
    struct intset *result;
    int i;

    result = new_intset();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        result->elements[i] = s1->elements[i] || s2->elements[i];

    return result;
}

struct intset *set_difference(struct intset *s1, struct intset *s2) {
    struct intset *result;
    int i;

    result = new_intset();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        result->elements[i] = s1->elements[i] && !s2->elements[i];

    return result;
}
