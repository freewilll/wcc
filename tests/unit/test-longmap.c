#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../../wcc.h"

void panic(char *message) {
    printf("%s\n", message);
    exit(1);
}

int main() {
    LongMap *map = new_longmap();

    int COUNT = 10000;
    for (int i = -COUNT; i < COUNT; i++) {
        long key = i * 42;
        char *value = malloc(16);
        sprintf(value, "bar %d", i);
        longmap_put(map, key, value);
    }

    // Lookup the keys
    for (int i = -COUNT; i < COUNT; i++) {
        long key = i * 42;
        char *value = malloc(16);
        sprintf(value, "bar %d", i);
        char *got_value = longmap_get(map, key);
        if (!got_value) panic("Didn't get a match\n");
        if (strcmp(value, got_value)) panic("Got something horrible 1");

        got_value = longmap_get(map, 47);
        if (got_value) panic("Expected no match");
    }

    // Test iteration
    int i = 0, h = 0;
    for (LongMapIterator *it = new_longmap_iterator(map); !longmap_iterator_finished(it); longmap_iterator_next(it), i++)
        h += 7 * h + longmap_iterator_key(it);
    if (h != -46927138) panic("Iteration checksum ok");

    // Reassign the even numbered elements with baz %d
    for (int i = -COUNT + 1; i < COUNT; i += 2) {
        long key = i * 42;
        char *value = malloc(16);
        sprintf(value, "baz %d", i);
        longmap_put(map, key, value);
    }

    // Recheck the keys with alternating values
    for (int i = -COUNT + 1; i < COUNT; i++) {
        long key = i * 42;
        char *value = malloc(16);
        sprintf(value, i % 2 == 0 ? "bar %d" : "baz %d", i);

        char *got_value = longmap_get(map, key);
        if (!got_value) panic("Didn't get a match\n");
        if (strcmp(value, got_value)) panic("Got something horrible 2");
        got_value = longmap_get(map, 47);
        if (got_value) panic("Expected no match");
    }

    // Delete half of the keys
    for (int i = -COUNT + 1; i < COUNT; i += 2) {
        long key = i * 42;
        longmap_delete(map, key);
    }

    // Check half keys are gone
    for (int i = -COUNT + 1; i < COUNT; i++) {
        long key = i * 42;
        char *got_value = longmap_get(map, key);
        if ((!!got_value) == (!!(i % 2))) panic("Mismatch in key deletion");
    }

    // Insert/deletion thrashing in a new map. This tests rehashing without
    // a resize.
    map = new_longmap();
    for (int i = -COUNT; i < COUNT; i++) {
        long key = i * 42;
        char value[] = "foo";
        longmap_put(map, key, value);
        char *got_value = longmap_get(map, key);
        if (!got_value) panic("Did not get a key");
        longmap_delete(map, key);
        got_value = longmap_get(map, key);
        if (got_value) panic("Got a key");
    }
}
