#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

Set *new_set(int max_value) {
    Set *result;

    result = malloc(sizeof(Set));
    result->max_value = max_value;
    result->elements = malloc((max_value + 1) * sizeof(char));
    memset(result->elements, 0, (max_value + 1) * sizeof(char));
    result->cached_element_count = 0;
    result->cached_elements = 0;

    return result;
}

void free_set(Set *s) {
    free(s->elements);
    free(s);
}

void empty_set(Set *s) {
    int i;

    memset(s->elements, 0, (s->max_value + 1) * sizeof(char));
}

Set *copy_set(Set *s) {
    Set *result;

    result = new_set(s->max_value);
    memcpy(result->elements, s->elements, (s->max_value + 1) * sizeof(char));

    return result;
}

void copy_set_to(Set *dst, Set *src) {
    memcpy(dst->elements, src->elements, (src->max_value + 1) * sizeof(char));
}

void cache_set_elements(Set *s) {
    int i, count;
    int *cached_elements;

    if (!s->cached_elements) s->cached_elements = malloc((s->max_value + 1) * sizeof(int));
    cached_elements = s->cached_elements;

    count = 0;
    for (i = 0; i <= s->max_value; i++)
        if (s->elements[i]) cached_elements[count++] = i;

    s->cached_element_count = count;
}

int set_len(Set *s) {
    int i, result;

    result = 0;
    for (i = 0; i <= s->max_value; i++) result += s->elements[i];

    return result;
}

void print_set(Set *s) {
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

void *add_to_set(Set *s, int value) {
    if (value > s->max_value) panic2d("Max set value of %d exceeded with %d in add_to_set", s->max_value, value);
    s->elements[value] = 1;
}

void *delete_from_set(Set *s, int value) {
    if (value > s->max_value) panic2d("Max set value of %d exceeded with %d in delete_from_set", s->max_value, value);
    s->elements[value] = 0;
}

int in_set(Set *s, int value) {
    if (value > s->max_value) panic2d("Max set value of %d exceeded with %d in in_set", s->max_value, value);
    return s->elements[value] == 1;
}

int set_eq(Set *s1, Set *s2) {
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_eq");

    return memcmp(s1->elements, s2->elements, (s1->max_value + 1) * sizeof(char)) ? 0 : 1;
}

Set *set_intersection(Set *s1, Set *s2) {
    Set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_intersection");

    result = new_set(s1->max_value);
    for (i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] && s2->elements[i];

    return result;
}

void set_intersection_to(Set *dst, Set *s1, Set *s2) {
    Set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_intersection_to");
    if (s1->max_value != dst->max_value) panic("Unequal set sizes in set_intersection_to");

    for (i = 0; i <= s1->max_value; i++)
        dst->elements[i] = s1->elements[i] && s2->elements[i];
}

Set *set_union(Set *s1, Set *s2) {
    Set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_union");

    result = new_set(s1->max_value);
    for (i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] || s2->elements[i];

    return result;
}


void set_union_to(Set *dst, Set *s1, Set *s2) {
    Set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_union_to");
    if (s1->max_value != dst->max_value) panic("Unequal set sizes in set_union_to");

    for (i = 0; i <= s1->max_value; i++)
        dst->elements[i] = s1->elements[i] || s2->elements[i];
}

Set *set_difference(Set *s1, Set *s2) {
    Set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_difference");

    result = new_set(s1->max_value);
    for (i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] && !s2->elements[i];

    return result;
}

void set_difference_to(Set *dst, Set *s1, Set *s2) {
    Set *result;
    int i;

    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_difference_to");
    if (s1->max_value != dst->max_value) panic("Unequal set sizes in set_difference_to");

    for (i = 0; i <= s1->max_value; i++)
        dst->elements[i] = s1->elements[i] && !s2->elements[i];
}
