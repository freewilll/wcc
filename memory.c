#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <malloc.h>
#endif
#include "wcc.h"

static long total_allocation = 0;
static long peak_allocation = 0;
static long current_allocation = 0;
static int allocation_count = 0;
static int free_count = 0;

#ifndef __linux__
// malloc_usable_size is linux specific
size_t malloc_usable_size(void * ptr) {
    return 0;
}

#endif

static void add_to_allocation(size_t size) {
    current_allocation += size;
    total_allocation += size;
    if (current_allocation > peak_allocation) peak_allocation = current_allocation;
    allocation_count++;
}

static void remove_from_allocation(size_t size) {
    current_allocation -= size;
    free_count++;
}

// Wrapper around malloc that exits if malloc fails
void *wmalloc(size_t size) {
    void *result = malloc(size);

    if (!result) {
        printf("Failed to malloc %ld bytes\n", size);
        exit(1);
    }

    add_to_allocation(malloc_usable_size(result));

    return result;
}

// Wrapper around realloc that exits if realloc fails
void *wrealloc(void *ptr, size_t size) {
    remove_from_allocation(malloc_usable_size(ptr));
    void *result = realloc(ptr, size);

    if (!result) {
        printf("Failed to realloc %ld bytes\n", size);
        exit(1);
    }

    add_to_allocation(malloc_usable_size(result));

    return result;
}

// Wrapper around calloc that exits if calloc fails
void *wcalloc(size_t nitems, size_t size) {
    void *result = calloc(nitems, size);

    if (!result) {
        printf("Failed to calloc %ld bytes\n", size);
        exit(1);
    }
    add_to_allocation(malloc_usable_size(result));

    return result;
}

// Wrapper around strdup that exits if strdup fails
char *wstrdup(const char *str) {
    char *ptr = strdup(str);

    if (!ptr) {
        printf("Failed to strdup %s\n", str);
        exit(1);
    }

    add_to_allocation(malloc_usable_size(ptr));

    return ptr;
}

// Wrapper around free
void wfree(void *ptr) {
    if (!ptr) return;

    remove_from_allocation(malloc_usable_size(ptr));

    free(ptr);
}

static void print_human_readable_value(long value) {
    if (value < 0) {
        printf("-");
        value = -value;
    }

    char buffer[128];
    sprintf(buffer, "%ld", value);
    int len = strlen(buffer);
    for (int i = 0; i < len; i++) {
        printf("%d", buffer[i] - '0');
        int remaining = len - i;
        if (remaining >= 3 && ((remaining  - 1) % 3 == 0)) printf(",");
    }
}

void process_memory_allocation_stats(void) {
    if (print_heap_usage || (fail_on_leaked_memory && current_allocation)) {
        printf("In use at exit: ");
        print_human_readable_value(current_allocation); printf(" bytes\n");

        printf("Total heap usage: ");
        print_human_readable_value(allocation_count); printf(" allocs, ");
        print_human_readable_value(free_count);       printf(" frees, ");
        print_human_readable_value(total_allocation); printf(" bytes allocated, ");
        print_human_readable_value(peak_allocation); printf(" bytes peak usage\n");
    }

   if (fail_on_leaked_memory && current_allocation) {
        printf("Memory has been leaked.\n");
        exit(1);
    }
}
