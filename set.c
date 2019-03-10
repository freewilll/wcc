#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wc4.h"

struct set *new_set(int max_value) {
    struct set *result;

    result = malloc(sizeof(struct set));
    result->max_value = max_value;
    result->elements = malloc((max_value + 1) * sizeof(char));
    memset(result->elements, 0, (max_value + 1) * sizeof(char));

    return result;
}

void free_set(struct set *s) {
    free(s->elements);
    free(s);
}

void empty_set(struct set *s) {
    int i;

    for (i = 0; i <= s->max_value; i++) s->elements[i] = 0;
}

struct set *copy_set(struct set *s) {
    struct set *result;
    int i;

    result = new_set(s->max_value);
    for (i = 0; i <= s->max_value; i++) result->elements[i] = s->elements[i];

    return result;
}

int set_len(struct set *s) {
    int i, result;

    result = 0;
    for (i = 0; i <= s->max_value; i++) if (s->elements[i]) result++;

    return result;
}

void print_set(struct set *s) {
    int i;
    int first;

    first = 1;
    printf("{");
    for (i = 0; i <= s->max_value; i++) {
        if (s->elements[i]) {
            if (!first) { printf(", "); }
            printf("%d", i);
            first = 0;
        }
    }
    printf("}");
}

void *add_to_set(struct set *s, int value) {
    if (value > s->max_value) panic2d("Max set value of %d exceeded with %d in add_to_set", s->max_value, value);
    s->elements[value] = 1;
}

void *delete_from_set(struct set *s, int value) {
    if (value > s->max_value) panic2d("Max set value of %d exceeded with %d in delete_from_set", s->max_value, value);
    s->elements[value] = 0;
}

int in_set(struct set *s, int value) {
    if (value > s->max_value) panic2d("Max set value of %d exceeded with %d in in_set", s->max_value, value);
    return s->elements[value] == 1;
}

int set_eq(struct set *s1, struct set *s2) {
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_eq");

    for (i = 0; i <= s1->max_value; i++)
        if (s1->elements[i] != s2->elements[i]) return 0;

    return 1;
}

struct set *set_intersection(struct set *s1, struct set *s2) {
    struct set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_intersection");

    result = new_set(s1->max_value);
    for (i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] && s2->elements[i];

    return result;
}

struct set *set_union(struct set *s1, struct set *s2) {
    struct set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_union");

    result = new_set(s1->max_value);
    for (i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] || s2->elements[i];

    return result;
}

struct set *set_difference(struct set *s1, struct set *s2) {
    struct set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_difference");

    result = new_set(s1->max_value);
    for (i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] && !s2->elements[i];

    return result;
}
