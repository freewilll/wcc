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

    LongSet *ls8 = new_longset();
    LongSet *ls9 = new_longset();
    if (!longset_eq(ls8, ls9)) panic("Longset eq 1");
    longset_add(ls8, 1);
    if (longset_eq(ls8, ls9)) panic("Longset eq 2");
    longset_empty(ls8);
    longset_add(ls9, 1);
    if (longset_eq(ls8, ls9)) panic("Longset eq 3");
    longset_add(ls8, 1);
    if (!longset_eq(ls8, ls9)) panic("Longset eq 4");
    longset_add(ls8, 2);
    longset_add(ls9, 2);
    if (!longset_eq(ls8, ls9)) panic("Longset eq 5");

    LongSet* ls10 = longset_copy(ls9);
    if (!longset_eq(ls9, ls10)) panic("Longset copy");

    LongSet *ls11 = new_longset();
    if (longset_len(ls11) != 0) panic("longset len 1");
    longset_add(ls11, 1); if (longset_len(ls11) != 1) panic("longset len 2");
    longset_add(ls11, 1); if (longset_len(ls11) != 1) panic("longset len 3");
    longset_add(ls11, 2); if (longset_len(ls11) != 2) panic("longset len 4");
    longset_add(ls11, 3); if (longset_len(ls11) != 3) panic("longset len 5");

    LongSet *ls12 = new_longset();
    longset_add(ls12, 1);
    longset_add(ls12, 2);
    longset_add(ls12, 3);
    longset_delete(ls12, 1);
    if (longset_in(ls12, 1)) panic("Dit not expect 1 in set");
    if (!longset_in(ls12, 2)) panic("Expected 2 in set");
    if (!longset_in(ls12, 3)) panic("Expected 3 in set");
}
