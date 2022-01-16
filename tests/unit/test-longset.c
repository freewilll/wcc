#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "../../wcc.h"

int main() {
    LongSet *ls1 = new_longset();
    longset_add(ls1, 1);
    if (!longset_in(ls1, 1)) panic("Expected 1 in set");
    if (longset_in(ls1, 2)) panic("Did not expect 2 in set");

    LongSet *ls2 = new_longset();
    longset_add(ls2, 2);

    // Unions
    LongSet *ls3 = longset_union(ls1, ls2);
    if (!longset_in(ls3, 1)) panic("Expected 1 in set");
    if (!longset_in(ls3, 2)) panic("Expected 2 in set");
    if (longset_in(ls3, 3)) panic("Did not expect 3 in set");

    LongSet *ls4 = new_longset();
    longset_add(ls4, 1);
    LongSet *ls5 = longset_union(ls3, ls4);
    if (!longset_in(ls5, 1)) panic("Expected 1 in set");
    if (!longset_in(ls5, 2)) panic("Expected 2 in set");
    if (longset_in(ls5, 3)) panic("Did not expect 3 in set");

    // Intersections
    LongSet *lsi1 = longset_intersection(ls1, ls2);
    if (longset_in(lsi1, 1)) panic("Did not expect 1 in set");
    if (longset_in(lsi1, 2)) panic("Did not expect 2 in set");
    if (longset_in(lsi1, 3)) panic("Did not expect 3 in set");

    LongSet *lsi2 = longset_intersection(ls1, ls3);
    if (!longset_in(lsi2, 1)) panic("Expected 1 in set");
    if (longset_in(lsi2, 2)) panic("Did not expect 2 in set");
    if (longset_in(lsi2, 3)) panic("Did not expect 3 in set");

    LongSet *lsi3 = longset_intersection(new_longset(), new_longset());
    if (longset_in(lsi3, 1)) panic("Did not expect 1 in set");

    LongSet *lsi4 = longset_intersection(ls1, new_longset());
    if (longset_in(lsi4, 1)) panic("Did not expect 1 in set");

    LongSet *lsi5 = longset_intersection(new_longset(), ls1);
    if (longset_in(lsi5, 1)) panic("Did not expect 1 in set");

    LongSet *ls6 = new_longset();
    longset_add(ls6, 1);
    if (!longset_in(ls6, 1)) panic("Expected 1 in set");
    longset_empty(ls6);
    if (longset_in(ls6, 1)) panic("Did not expect 1 in set");

    LongSet *ls7 = new_longset();
    longset_add(ls7, 1);
    longset_add(ls7, 2);
    longset_add(ls7, 3);
    int h = 0;
    for (LongSetIterator it = longset_iterator(ls7); !longset_iterator_finished(&it); longset_iterator_next(&it))
        h += 7 * h + longset_iterator_element(&it);
    if (h != 83) panic("Longset iteratator");
}
