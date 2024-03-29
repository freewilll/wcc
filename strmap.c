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
    char **keys = wcalloc(new_size,  sizeof(char *));
    void **values = wcalloc(new_size,  sizeof(void *));

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

    wfree(map->keys);
    wfree(map->values);
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

int strmap_iterator_finished(StrMapIterator *iterator) {
    return iterator->pos == -1;
}

void strmap_iterator_next(StrMapIterator *iterator) {
    if (iterator->original_size != iterator->map->size) panic("longset size changed during iteration");

    iterator->pos++;

    while (iterator->pos < iterator->map->size && (!iterator->map->keys[iterator->pos] || iterator->map->keys[iterator->pos] == (char *) TOMBSTONE))
        iterator->pos++;

    if (iterator->pos == iterator->map->size || (!iterator->map->keys[iterator->pos] || iterator->map->keys[iterator->pos] == (char *) TOMBSTONE))
        iterator->pos = -1;
}

char *strmap_iterator_key(StrMapIterator *iterator) {
    if (iterator->pos == -1) panic("Attempt to iterate beyond the end of the iterator");
    return iterator->map->keys[iterator->pos];
}

StrMapIterator strmap_iterator(StrMap *map) {
    StrMapIterator result;
    result.map = map;
    result.pos = -1;
    result.original_size = map->size;
    strmap_iterator_next(&result);

    return result;
}

StrMap *new_strmap(void) {
    StrMap *map = wcalloc(1, sizeof(StrMap));
    map->size = DEFAULT_SIZE;
    map->keys = wcalloc(DEFAULT_SIZE, sizeof(void *));
    map->values = wcalloc(DEFAULT_SIZE, sizeof(char *));
    return map;
}

void free_strmap(StrMap *map) {
    wfree(map->keys);
    wfree(map->values);
    wfree(map);
}
