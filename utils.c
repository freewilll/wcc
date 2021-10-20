#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

void panic(char *message) {
    printf("%s:%d: %s\n", cur_filename, cur_line, message);
    exit(1);
}

void panic1d(char *fmt, int i) {
    printf("%s:%d: ", cur_filename, cur_line);
    printf(fmt, i);
    printf("\n");
    exit(1);
}

void panic1s(char *fmt, char *s) {
    printf("%s:%d: ", cur_filename, cur_line);
    printf(fmt, s);
    printf("\n");
    exit(1);
}

void panic2d(char *fmt, int i1, int i2) {
    printf("%s:%d: ", cur_filename, cur_line);
    printf(fmt, i1, i2);
    printf("\n");
    exit(1);
}

void panic2s(char *fmt, char *s1, char *s2) {
    printf("%s:%d: ", cur_filename, cur_line);
    printf(fmt, s1, s2);
    printf("\n");
    exit(1);
}

void panic1s1d(char *fmt, char *s, int i) {
    printf("%s:%d: ", cur_filename, cur_line);
    printf(fmt, s, i);
    printf("\n");
    exit(1);
}

Function *new_function(void) {
    Function *function = malloc(sizeof(Function));
    memset(function, 0, sizeof(Function));

    return function;
}
