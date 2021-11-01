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
