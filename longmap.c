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
    long *keys = malloc(new_size * sizeof(long));
    void **values = malloc(new_size * sizeof(void *));
    char *status = malloc(new_size * sizeof(char));
    memset(keys, 0, new_size * sizeof(long));
    memset(values, 0, new_size * sizeof(void *));
    memset(status, 0, new_size * sizeof(char));

    for (int i = 0; i < map->size; i++) {
        if (map->status[i] != STATUS_USED) continue;
        long key = map->keys[i];
        long pos = hash(key) & mask;
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

    free(map->keys);
    free(map->values);
    free(map->status);
    map->keys = keys;
    map->values = values;
    map->status = status;
    map->size = new_size;
    map->used_count = map->element_count;
}

void longmap_put(LongMap *map, long key, void *value) {
    maybe_rehash(map);

    long mask = map->size - 1;
    long pos = hash(key) & mask;

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
    long pos = hash(key) & mask;

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
    long pos = hash(key) & mask;

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

LongMap *new_longmap(void) {
    LongMap *map = malloc(sizeof(LongMap));
    memset(map, 0, sizeof(LongMap));
    map->size = DEFAULT_SIZE;
    map->keys = malloc(DEFAULT_SIZE * sizeof(long));
    map->values = malloc(DEFAULT_SIZE * sizeof(void *));
    map->status = malloc(DEFAULT_SIZE * sizeof(char));
    memset(map->keys, 0, DEFAULT_SIZE * sizeof(long));
    memset(map->values, 0, DEFAULT_SIZE * sizeof(void *));
    memset(map->status, 0, DEFAULT_SIZE * sizeof(char));
    return map;
}
