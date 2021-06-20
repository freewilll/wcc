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
    Map *map = new_map();

    int COUNT = 10000;
    for (int i = 0; i < COUNT; i++) {
        char *key = malloc(16);
        char *value = malloc(16);
        sprintf(key, "foo %d", i);
        sprintf(value, "bar %d", i);
        map_put(map, key, value);
    }

    // Lookup the keys
    for (int i = 0; i < COUNT; i++) {
        char *key = malloc(16);
        char *value = malloc(16);
        sprintf(key, "foo %d", i);
        sprintf(value, "bar %d", i);
        char *got_value = map_get(map, key);
        if (!got_value) panic("Didn't get a match\n");
        if (strcmp(value, got_value)) panic("Got something horrible 1");

        got_value = map_get(map, "nuttin");
        if (got_value) panic("Expected no match");
    }

    // Reassign the even numbered elements with baz %d
    for (int i = 1; i < COUNT; i += 2) {
        char *key = malloc(16);
        char *value = malloc(16);
        sprintf(key, "foo %d", i);
        sprintf(value, "baz %d", i);
        map_put(map, key, value);
    }

    // Recheck the keys with alternating values
    for (int i = 0; i < COUNT; i++) {
        char *key = malloc(16);
        char *value = malloc(16);
        sprintf(key, "foo %d", i);
        sprintf(value, i % 2 == 0 ? "bar %d" : "baz %d", i);

        char *got_value = map_get(map, key);
        if (!got_value) panic("Didn't get a match\n");
        if (strcmp(value, got_value)) panic("Got something horrible 2");
        got_value = map_get(map, "nuttin");
        if (got_value) panic("Expected no match");
    }

    // Delete half of the keys
    for (int i = 1; i < COUNT; i += 2) {
        char *key = malloc(16);
        sprintf(key, "foo %d", i);
        map_delete(map, key);
    }

    // Check half keys are gone
    for (int i = 0; i < COUNT; i++) {
        char *key = malloc(16);
        sprintf(key, "foo %d", i);
        char *got_value = map_get(map, key);
        if ((!!got_value) == (!!(i % 2))) panic("Mismatch in key deletion");
    }

    // Insert/deletion thrashing in a new map. This tests rehashing without
    // a resize.
    map = new_map();
    for (int i = 0; i < COUNT; i++) {
        char *key = malloc(16);
        char value[] = "foo";
        sprintf(key, "foo %d", i);
        map_put(map, key, value);
        char *got_value = map_get(map, key);
        if (!got_value) panic("Did not get a key");
        map_delete(map, key);
        got_value = map_get(map, key);
        if (got_value) panic("Got a key");
    }
}
