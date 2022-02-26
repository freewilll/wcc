#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

#include "wcc.h"

static struct timeval debug_log_start;

// Report an internal error and exit
void panic(char *format, ...) {
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "Internal error: ");
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

// Strip the filename component from a path
char *base_path(char *path) {
    int end = strlen(path) - 1;
    char *result = wmalloc(strlen(path) + 1);
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
    *ret = wmalloc(size + 1);
    vsnprintf(*ret, size + 1, format, ap2);
    va_end(ap2);

    return size;
}

// Allocate data for a new string buffer, rounding initial_size up to a power of two.
StringBuffer *new_string_buffer(int initial_size) {
    StringBuffer *sb = wmalloc(sizeof(StringBuffer));
    sb->position = 0;

    int allocated = 1;
    while (allocated < initial_size * 2) allocated <<= 1;
    sb->allocated = allocated;
    sb->data = wmalloc(allocated);

    return sb;
}

void free_string_buffer(StringBuffer *sb, int free_data) {
    if (free_data) free(sb->data);
    free(sb);
}

// Append a string to the string buffer
void append_to_string_buffer(StringBuffer *sb, char *str) {
    int len = strlen(str);
    int needed = sb->position + len + 1;
    if (sb->allocated < needed) {
        while (sb->allocated < needed) sb->allocated <<= 1;
        sb->data = wrealloc(sb->data, sb->allocated);
    }
    sprintf(sb->data + sb->position, "%s", str);
    sb->position += len;
}

// Null terminate the string in the string buffer
void terminate_string_buffer(StringBuffer *sb) {
    sb->data[sb->position] = 0;
}

int set_debug_logging_start_time() {
    gettimeofday(&debug_log_start, NULL);
}

int debug_log(char *format, ...) {
    struct timeval end;

    va_list ap;
    va_start(ap, format);

    gettimeofday(&end, NULL);
    long secs_used=(end.tv_sec - debug_log_start.tv_sec); //avoid overflow by subtracting first
    long microseconds = ((secs_used*1000000) + end.tv_usec) - (debug_log_start.tv_usec);
    secs_used = microseconds / 1000000;
    microseconds = (microseconds % 1000000);
    microseconds /= 1000;

    int hr=(int)(secs_used/3600);
    int min=((int)(secs_used/60))%60;
    int sec=(int)(secs_used%60);

    printf("%02d:%02d:%02d.%03ld ", hr, min, sec, microseconds);
    vprintf(format, ap);
    printf("\n");
}

// Wrapper around malloc that exits if malloc fails
void *wmalloc(size_t size) {
    void *result = malloc(size);

    if (!result) {
        printf("Failed to malloc %ld bytes\n", size);
        exit(1);
    }

    return result;
}

// Wrapper around realloc that exits if realloc fails
void *wrealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);

    if (!result) {
        printf("Failed to realloc %ld bytes\n", size);
        exit(1);
    }

    return result;
}

// Wrapper around calloc that exits if calloc fails
void *wcalloc(size_t nitems, size_t size) {
    void *result = calloc(nitems, size);

    if (!result) {
        printf("Failed to calloc %ld bytes\n", size);
        exit(1);
    }

    return result;
}
