#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

struct set *new_set() {
    struct set *result;

    result = malloc(sizeof(struct set *));
    memset(result, 0, sizeof(struct set *));
    result->elements = malloc(MAX_INT_SET_ELEMENTS * sizeof(int));
    memset(result->elements, 0, MAX_INT_SET_ELEMENTS * sizeof(int));

    return result;
}

void empty_set(struct set *s) {
    int i;

    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++) s->elements[i] = 0;
}

struct set *copy_set(struct set *s) {
    struct set *result;
    int i;

    result = new_set();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++) result->elements[i] = s->elements[i];

    return result;
}

int set_len(struct set *s) {
    int i, result;

    result = 0;
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++) if (s->elements[i]) result++;

    return result;
}

void print_set(struct set *s) {
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

void *add_to_set(struct set *s, int value) {
    s->elements[value] = 1;
}

void *delete_from_set(struct set *s, int value) {
    s->elements[value] = 0;
}

int in_set(struct set *s, int value) {
    return s->elements[value] == 1;
}

int set_eq(struct set *s1, struct set *s2) {
    int i;

    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        if (s1->elements[i] != s2->elements[i]) return 0;

    return 1;
}

struct set *set_intersection(struct set *s1, struct set *s2) {
    struct set *result;
    int i;

    result = new_set();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        result->elements[i] = s1->elements[i] && s2->elements[i];

    return result;
}

struct set *set_union(struct set *s1, struct set *s2) {
    struct set *result;
    int i;

    result = new_set();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        result->elements[i] = s1->elements[i] || s2->elements[i];

    return result;
}

struct set *set_difference(struct set *s1, struct set *s2) {
    struct set *result;
    int i;

    result = new_set();
    for (i = 0; i < MAX_INT_SET_ELEMENTS; i++)
        result->elements[i] = s1->elements[i] && !s2->elements[i];

    return result;
}
