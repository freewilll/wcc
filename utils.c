#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "wcc.h"

void panic(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    printf("%s:%d: ", cur_filename, cur_line);
    vprintf(format, ap);
    printf("\n");
    va_end(ap);
    exit(1);
}

Function *new_function(void) {
    Function *function = malloc(sizeof(Function));
    memset(function, 0, sizeof(Function));

    return function;
}

void quicksort_ulong_array(unsigned long *array, int left, int right) {
    if (left >= right) return;

    int i = left;
    int j = right;
    long pivot = array[i];

    while (1) {
        while (array[i] < pivot) i++;
        while (pivot < array[j]) j--;
        if (i >= j) break;

        unsigned long temp = array[i];
        array[i] = array[j];
        array[j] = temp;

        i++;
        j--;
    }

    quicksort_ulong_array(array, left, i - 1);
    quicksort_ulong_array(array, j + 1, right);
}

// Strip the filename component from a path
char *base_path(char *path) {
    int end = strlen(path) - 1;
    char *result = malloc(strlen(path) + 1);
    while (end >= 0 && path[end] != '/') end--;
    if (end >= 0) result = memcpy(result, path, end + 1);
    result[end + 1] = 0;

    return result;
}

int wasprintf(char **ret, const char *format, ...) {
    va_list ap1;
    va_start(ap1, format);
    va_list ap2;
    va_copy(ap2, ap1);
    int size = vsnprintf(NULL, 0, format, ap1);
    va_end(ap1);
    *ret = malloc(size + 1);
    vsnprintf(*ret, size + 1, format, ap2);
    va_end(ap2);

    return size;
}
