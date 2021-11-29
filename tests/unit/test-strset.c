#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../../wcc.h"

int main() {
    StrSet *ss1 = new_strset();
    strset_add(ss1, "foo");
    if (!strset_in(ss1, "foo")) panic("Expected foo in set");
    if (strset_in(ss1, "bar")) panic("Did not expect bar in set");

    StrSet *ss2 = new_strset();
    strset_add(ss2, "bar");

    // Unions
    StrSet *ss3 = strset_union(ss1, ss2);
    if (!strset_in(ss3, "foo")) panic("Expected foo in set");
    if (!strset_in(ss3, "bar")) panic("Expected bar in set");
    if (strset_in(ss3, "baz")) panic("Did not expect baz in set");

    StrSet *ss4 = new_strset();
    strset_add(ss4, "foo");
    StrSet *ss5 = strset_union(ss3, ss4);
    if (!strset_in(ss5, "foo")) panic("Expected foo in set");
    if (!strset_in(ss5, "bar")) panic("Expected bar in set");
    if (strset_in(ss5, "baz")) panic("Did not expect baz in set");

    // Intersections
    StrSet *ssi1 = strset_intersection(ss1, ss2);
    if (strset_in(ssi1, "foo")) panic("Did not expect foo in set");
    if (strset_in(ssi1, "bar")) panic("Did not expect bar in set");
    if (strset_in(ssi1, "baz")) panic("Did not expect baz in set");

    StrSet *ssi2 = strset_intersection(ss1, ss3);
    if (!strset_in(ssi2, "foo")) panic("Expected foo in set");
    if (strset_in(ssi2, "bar")) panic("Did not expect bar in set");
    if (strset_in(ssi2, "baz")) panic("Did not expect baz in set");

    StrSet *ssi3 = strset_intersection(new_strset(), new_strset());
    if (strset_in(ssi3, "foo")) panic("Did not expect foo in set");

    StrSet *ssi4 = strset_intersection(ss1, new_strset());
    if (strset_in(ssi4, "foo")) panic("Did not expect foo in set");

    StrSet *ssi5 = strset_intersection(new_strset(), ss1);
    if (strset_in(ssi5, "foo")) panic("Did not expect foo in set");
}
