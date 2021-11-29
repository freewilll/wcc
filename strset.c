#include <stdlib.h>
#include <string.h>

#include "wcc.h"

StrSet *new_strset(void) {
    StrSet *ss = malloc(sizeof(StrSet));
    ss->strmap = new_strmap();

    return ss;
}

void strset_add(StrSet *ss, char *element) {
    strmap_put(ss->strmap, element, (void *) 1);
}

int strset_in(StrSet *ss, char *element) {
    return !!strmap_get(ss->strmap, element);
}

StrSet *strset_union(StrSet *ss1, StrSet *ss2) {
    StrSet *result = new_strset();

    for (StrMapIterator it = strmap_iterator(ss1->strmap); !strmap_iterator_finished(&it); strmap_iterator_next(&it))
        strmap_put(result->strmap, strmap_iterator_key(&it), (void *) 1);

    for (StrMapIterator it = strmap_iterator(ss2->strmap); !strmap_iterator_finished(&it); strmap_iterator_next(&it))
        strmap_put(result->strmap, strmap_iterator_key(&it), (void *) 1);

    return result;
}

StrSet *strset_intersection(StrSet *ss1, StrSet *ss2) {
    StrSet *result = new_strset();

    for (StrMapIterator it = strmap_iterator(ss1->strmap); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        char *key = strmap_iterator_key(&it);
        if (strmap_get(ss2->strmap, key)) strmap_put(result->strmap, strmap_iterator_key(&it), (void *) 1);
    }

    return result;
}
