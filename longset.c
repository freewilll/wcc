#include <stdlib.h>

#include "wcc.h"

LongSet *new_longset(void) {
    LongSet *ls = wmalloc(sizeof(LongSet));
    ls->longmap = new_longmap();

    return ls;
}

void free_longset(LongSet *ls) {
    free_longmap(ls->longmap);
    wfree(ls);
}

void longset_add(LongSet *ls, long element) {
    longmap_put(ls->longmap, element, (void *) 1);
}

void longset_delete(LongSet *ls, long element) {
    longmap_delete(ls->longmap, element);
}

void longset_empty(LongSet *ls) {
    longmap_empty(ls->longmap);
}

int longset_in(LongSet *ls, long element) {
    return !!longmap_get(ls->longmap, element);
}

int longset_eq(LongSet *ls1, LongSet *ls2) {
    int count1 = 0;

    longmap_foreach(ls1->longmap, it) {
        count1++;
        if (!longmap_get(ls2->longmap, longmap_iterator_key(&it))) return 0;
    }

    int count2 = 0;
    longmap_foreach(ls2->longmap, it) count2++;

    return (count1 == count2);

    return 1;
}

int longset_len(LongSet *ls) {
    int count = 0;

    longmap_foreach(ls->longmap, it)
        count++;

    return count;
}

LongSet *longset_copy(LongSet *ls) {
    LongSet *result = wmalloc(sizeof(LongSet));
    result->longmap = longmap_copy(ls->longmap);

    return result;
}


LongSet *longset_union(LongSet *ls1, LongSet *ls2) {
    LongSet *result = new_longset();

    longmap_foreach(ls1->longmap, it)
        longmap_put(result->longmap, longmap_iterator_key(&it), (void *) 1);

    longmap_foreach(ls2->longmap, it)
        longmap_put(result->longmap, longmap_iterator_key(&it), (void *) 1);

    return result;
}

LongSet *longset_intersection(LongSet *ls1, LongSet *ls2) {
    LongSet *result = new_longset();

    longmap_foreach(ls1->longmap, it) {
        long key = longmap_iterator_key(&it);
        if (longmap_get(ls2->longmap, key)) longmap_put(result->longmap, longmap_iterator_key(&it), (void *) 1);
    }

    return result;
}

void longset_iterator_next(LongSetIterator *iterator) {
    longmap_iterator_next(&iterator->longmap_iterator);
}

LongSetIterator longset_iterator(LongSet *ls) {
    LongSetIterator result;
    result.longmap_iterator = longmap_iterator(ls->longmap);

    return result;
}

void print_longset(LongSet *ls) {
    if (!ls) {
        printf("{}");
        return;
    }

    int first = 1;
    printf("{");
    for (LongSetIterator it = longset_iterator(ls); !longset_iterator_finished(&it); longset_iterator_next(&it)) {
        long element = longset_iterator_element(&it);
        if (!first) { printf(", "); }
        printf("%ld", element);
        first = 0;
    }
    printf("}");
}
