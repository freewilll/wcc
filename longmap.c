#include <stdlib.h>
#include <string.h>

#include "wcc.h"

enum {
    STATUS_EMPTY,
    STATUS_USED,
    STATUS_TOMBSTONE
};

enum {
    DEFAULT_SIZE     = 16,
    MAX_LOAD_FACTOR  = 666,  // 0.666 * 1000
    EXPANSION_FACTOR = 333,  // 0.333 * 1000
};


// Default hash function
static long hash(long l) {
    return l;
}

static void maybe_rehash(LongMap *map) {
    if (map->used_count * 1000 < map->size * MAX_LOAD_FACTOR) return;

    int new_size;
    if (map->element_count * 1000 < map->size * EXPANSION_FACTOR)
        new_size = map->size;
    else
        new_size = map->size * 2;

    int mask = new_size - 1;
    long *keys = wcalloc(new_size, sizeof(long));
    void **values = wcalloc(new_size, sizeof(void *));
    char *status = wcalloc(new_size, sizeof(char));

    for (int i = 0; i < map->size; i++) {
        if (map->status[i] != STATUS_USED) continue;
        long key = map->keys[i];
        long pos = map->hashfunc(key) & mask;
        while (1) {
            if (status[pos] == STATUS_EMPTY) {
                keys[pos] = key;
                values[pos] = map->values[i];
                status[pos] = STATUS_USED;
                break;
            }
            pos = (pos + 1) & mask;
        }
    }

    wfree(map->keys);
    wfree(map->values);
    wfree(map->status);
    map->keys = keys;
    map->values = values;
    map->status = status;
    map->size = new_size;
    map->used_count = map->element_count;
}

void longmap_put(LongMap *map, long key, void *value) {
    maybe_rehash(map);

    long mask = map->size - 1;
    long pos = map->hashfunc(key) & mask;

    while (1) {
        char status = map->status[pos];
        if (status != STATUS_USED) {
            map->keys[pos] = key;
            map->values[pos] = value;
            map->status[pos] = STATUS_USED;
            map->element_count++;
            if (status == STATUS_EMPTY) map->used_count++;
            return;
        }

        if (map->keys[pos] == key) {
            map->values[pos] = value;
            return;
        }
        pos = (pos + 1) & mask;
    }
}

void *longmap_get(LongMap *map, long key) {
    long mask = map->size - 1;
    long pos = map->hashfunc(key) & mask;

    char status = map->status[pos];
    while (status != STATUS_EMPTY) {
        long k = map->keys[pos];
        if (status != STATUS_TOMBSTONE && k == key) return map->values[pos];
        pos = (pos + 1) & mask;
        status = map->status[pos];
    }
    return 0;
}

void longmap_delete(LongMap *map, long key) {
    long mask = map->size - 1;
    long pos = map->hashfunc(key) & mask;

    char status = map->status[pos];
    while (status != STATUS_EMPTY) {
        long k = map->keys[pos];
        if (status != STATUS_TOMBSTONE && k == key) {
            map->status[pos] = STATUS_TOMBSTONE;
            map->element_count--;
            return;
        }
        pos = (pos + 1) & mask;
        status = map->status[pos];
    }
}

// Populate keys with the keys in the map & return the count. Memory for keys is
// allocated, the caller is expected to free it.
int longmap_keys(LongMap *map, int **keys) {
    *keys = wmalloc(map->size * sizeof(int));
    int count = 0;

    longmap_foreach(map, it) (*keys)[count++] = longmap_iterator_key(&it);

    return count;
}

void longmap_empty(LongMap *map) {
    memset(map->status, 0, map->size * sizeof(char));
    map->used_count = 0;
    map->element_count = 0;
}

LongMap *longmap_copy(LongMap *map) {
    LongMap *result = wcalloc(1, sizeof(LongMap));
    result->keys = wmalloc(map->size * sizeof(long));
    result->values = wmalloc(map->size * sizeof(void *));
    result->status = wmalloc(map->size * sizeof(char));
    memcpy(result->keys, map->keys, map->size * sizeof(long));
    memcpy(result->values, map->values, map->size * sizeof(void *));
    memcpy(result->status, map->status, map->size * sizeof(char));
    result->size = map->size;
    result->used_count = map->used_count;
    result->element_count = map->element_count;
    result->hashfunc = map->hashfunc;

    return result;
}

void longmap_iterator_next(LongMapIterator *iterator) {
    LongMap *map = iterator->map;
    char *status = iterator->map->status;
    int size = map->size;

    if (iterator->original_size != map->size) panic("longmap size changed during iteration");

    iterator->pos++;

    while (iterator->pos < size && status[iterator->pos] != STATUS_USED)
        iterator->pos++;

    if (iterator->pos == size || status[iterator->pos] != STATUS_USED)
        iterator->pos = -1;
}

LongMapIterator longmap_iterator(LongMap *map) {
    LongMapIterator result;
    result.map = map;
    result.pos = -1;
    result.original_size = map->size;
    longmap_iterator_next(&result);

    return result;
}

LongMap *new_longmap(void) {
    LongMap *map = wcalloc(1, sizeof(LongMap));
    map->size = DEFAULT_SIZE;
    map->keys = wcalloc(DEFAULT_SIZE, sizeof(long));
    map->values = wcalloc(DEFAULT_SIZE, sizeof(void *));
    map->status = wcalloc(DEFAULT_SIZE, sizeof(char));
    map->hashfunc = hash;
    return map;
}

void free_longmap(LongMap *map) {
    wfree(map->keys);
    wfree(map->values);
    wfree(map->status);
    wfree(map);
}
