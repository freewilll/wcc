#include <stdlib.h>
#include <string.h>

#include "wcc.h"

enum {
    TOMBSTONE        = -1,
    DEFAULT_SIZE     = 16,
    MAX_LOAD_FACTOR  = 666,  // 0.666 * 1000
    EXPANSION_FACTOR = 333,  // 0.333 * 1000
};


static unsigned int hash(char *str) {
    // FNV hash function
    unsigned int result = 2166136261;
    char *p = str;
    while (*p) {
        result = (result ^ (*p)) * 16777619;
        p++;
    }
    return result;
}

static void maybe_rehash(StrMap *map) {
    if (map->used_count * 1000 < map->size * MAX_LOAD_FACTOR) return;

    int new_size;
    if (map->element_count * 1000 < map->size * EXPANSION_FACTOR)
        new_size = map->size;
    else
        new_size = map->size * 2;

    int mask = new_size - 1;
    char **keys = malloc(new_size * sizeof(char *));
    void **values = malloc(new_size * sizeof(void *));
    memset(keys, 0, new_size * sizeof(char *));
    memset(values, 0, new_size * sizeof(void *));

    for (int i = 0; i < map->size; i++) {
        char *key;
        if (!(key = map->keys[i]) || key == (char *) TOMBSTONE) continue;
        unsigned int pos = hash(key) & mask;
        while (1) {
            if (!keys[pos]) {
                keys[pos] = key;
                values[pos] = map->values[i];
                break;
            }
            pos = (pos + 1) & mask;
        }
    }

    free(map->keys);
    free(map->values);
    map->keys = keys;
    map->values = values;
    map->size = new_size;
    map->used_count = map->element_count;
}

void strmap_put(StrMap *map, char *key, void *value) {
    maybe_rehash(map);

    unsigned int mask = map->size - 1;
    unsigned int pos = hash(key) & mask;

    char *k;
    while (1) {
        k = map->keys[pos];
        if (!k || k == (char *) TOMBSTONE) {
            map->keys[pos] = key;
            map->values[pos] = value;
            map->element_count++;
            if (!k) map->used_count++;
            return;
        }

        if (!strcmp(k, key)) {
            map->values[pos] = value;
            return;
        }
        pos = (pos + 1) & mask;
    }
}

void *strmap_get(StrMap *map, char *key) {
    unsigned int mask = map->size - 1;
    unsigned int pos = hash(key) & mask;

    char *k;
    while ((k = map->keys[pos])) {
        if (k != (char *) TOMBSTONE && !strcmp(k, key)) return map->values[pos];
        pos = (pos + 1) & mask;
    }
    return 0;
}

void strmap_delete(StrMap *map, char *key) {
    unsigned int mask = map->size - 1;
    unsigned int pos = hash(key) & mask;

    char *k;
    while ((k = map->keys[pos])) {
        if (k != (char *) TOMBSTONE && !strcmp(k, key)) {
            map->keys[pos] = (char *) TOMBSTONE;
            map->values[pos] = 0;
            map->element_count--;
            return;
        }
        pos = (pos + 1) & mask;
    }
}

StrMap *new_strmap(void) {
    StrMap *map = malloc(sizeof(StrMap));
    memset(map, 0, sizeof(StrMap));
    map->size = DEFAULT_SIZE;
    map->keys = malloc(DEFAULT_SIZE * sizeof(void *));
    map->values = malloc(DEFAULT_SIZE * sizeof(char *));
    memset(map->keys, 0, DEFAULT_SIZE * sizeof(void *));
    memset(map->values, 0, DEFAULT_SIZE * sizeof(void *));
    return map;
}
