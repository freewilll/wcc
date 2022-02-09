#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

Set *new_set(int max_value) {
    Set *result = malloc(sizeof(Set));
    result->max_value = max_value;
    result->elements = calloc(max_value + 1, sizeof(char));
    result->cached_element_count = 0;
    result->cached_elements = 0;

    return result;
}

void free_set(Set *s) {
    if (s->cached_elements) free(s->cached_elements);
    free(s->elements);
    free(s);
}

void empty_set(Set *s) {
    memset(s->elements, 0, (s->max_value + 1) * sizeof(char));
}

Set *copy_set(Set *s) {
    Set *result = new_set(s->max_value);
    memcpy(result->elements, s->elements, (s->max_value + 1) * sizeof(char));

    return result;
}

void copy_set_to(Set *dst, Set *src) {
    memcpy(dst->elements, src->elements, (src->max_value + 1) * sizeof(char));
}

void cache_set_elements(Set *s) {
    if (!s->cached_elements) s->cached_elements = malloc((s->max_value + 1) * sizeof(int));
    int *cached_elements = s->cached_elements;

    int count = 0;
    for (int i = 0; i <= s->max_value; i++)
        if (s->elements[i]) cached_elements[count++] = i;

    s->cached_element_count = count;
}

int set_len(Set *s) {
    int max_value = s->max_value;
    char *elements = s->elements;
    int result = 0;
    for (int i = 0; i <= max_value; i++) result += elements[i];

    return result;
}

void print_set(Set *s) {
    int first = 1;
    printf("{");
    for (int i = 0; i <= s->max_value; i++) {
        if (s->elements[i]) {
            if (!first) { printf(", "); }
            printf("%d", i);
            first = 0;
        }
    }
    printf("}");
}

void *delete_from_set(Set *s, int value) {
    if (value > s->max_value) panic("Max set value of %d exceeded with %d in delete_from_set", s->max_value, value);
    s->elements[value] = 0;
}

int in_set(Set *s, int value) {
    if (value > s->max_value) panic("Max set value of %d exceeded with %d in in_set", s->max_value, value);
    return s->elements[value] == 1;
}

int set_eq(Set *s1, Set *s2) {
    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_eq");

    return memcmp(s1->elements, s2->elements, (s1->max_value + 1) * sizeof(char)) ? 0 : 1;
}

Set *set_intersection(Set *s1, Set *s2) {
    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_intersection");

    Set *result = new_set(s1->max_value);
    for (int i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] && s2->elements[i];

    return result;
}

void set_intersection_to(Set *dst, Set *s1, Set *s2) {
    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_intersection_to");
    if (s1->max_value != dst->max_value) panic("Unequal set sizes in set_intersection_to");

    for (int i = 0; i <= s1->max_value; i++)
        dst->elements[i] = s1->elements[i] && s2->elements[i];
}

Set *set_union(Set *s1, Set *s2) {
    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_union");

    Set *result = new_set(s1->max_value);
    for (int i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] || s2->elements[i];

    return result;
}


void set_union_to(Set *dst, Set *s1, Set *s2) {
    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_union_to");
    if (s1->max_value != dst->max_value) panic("Unequal set sizes in set_union_to");

    for (int i = 0; i <= s1->max_value; i++)
        dst->elements[i] = s1->elements[i] || s2->elements[i];
}

Set *set_difference(Set *s1, Set *s2) {
    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_difference");

    Set *result = new_set(s1->max_value);
    for (int i = 0; i <= s1->max_value; i++)
        result->elements[i] = s1->elements[i] && !s2->elements[i];

    return result;
}

void set_difference_to(Set *dst, Set *s1, Set *s2) {
    if (s1->max_value != s2->max_value) panic("Unequal set sizes in set_difference_to");
    if (s1->max_value != dst->max_value) panic("Unequal set sizes in set_difference_to");

    for (int i = 0; i <= s1->max_value; i++)
        dst->elements[i] = s1->elements[i] && !s2->elements[i];
}
