#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

static char *cpp_input;
static int cpp_input_size;
static int cpp_ip;
static char *cpp_cur_filename;
static LineMap *cpp_line_map;

static void init_cpp(char *filename) {
    cpp_ip = 0;
    FILE *f  = fopen(filename, "r");

    if (f == 0) {
        perror(filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    cpp_input_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    cpp_input = malloc(cpp_input_size + 1);
    int read = fread(cpp_input, 1, cpp_input_size, f);
    if (read != cpp_input_size) {
        printf("Unable to read input file\n");
        exit(1);
    }

    fclose(f);

    cpp_input[cpp_input_size] = 0;
    cpp_cur_filename = filename;
}

void init_cpp_from_string(char *string) {
    cpp_ip = 0;
    cpp_input = string;
    cpp_input_size = strlen(string);
    cpp_cur_filename = 0;
}

char *get_cpp_input(void) {
    return cpp_input;
}

LineMap *get_cpp_linemap(void) {
    return cpp_line_map;
}

static LineMap *add_to_linemap(LineMap *lm, int position, int line_number) {
    if (lm->position == position) {
        lm->line_number = line_number;
        return;
    }

    lm->next = malloc(sizeof(LineMap));
    lm->next->position = position;
    lm->next->line_number = line_number;
    return lm->next;
}

void transform_trigraphs(void) {
    char *output = malloc(cpp_input_size + 1);
    int ip = 0; // Input offset
    int op = 0; // Output offset

    while (ip < cpp_input_size) {
        if (cpp_input_size - ip >= 3) {
            // Check for trigraphs
            char c1 = cpp_input[ip];
            char c2 = cpp_input[ip + 1];
            char c3 = cpp_input[ip + 2];

                 if (c1 == '?' && c2 == '?' && c3 == '=')  { output[op++] = '#';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '(')  { output[op++] = '[';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '/')  { output[op++] = '\\'; ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == ')')  { output[op++] = ']';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '\'') { output[op++] = '^';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '<')  { output[op++] = '{';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '!')  { output[op++] = '|';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '>')  { output[op++] = '}';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '-')  { output[op++] = '~';  ip += 3; }
            else output[op++] = cpp_input[ip++];
        }
        else output[op++] = cpp_input[ip++];
    }

    output[op] = 0;
    cpp_input = output;
    cpp_input_size = op;
}

void strip_backslash_newlines(void) {
    char *output = malloc(cpp_input_size + 1);
    int ip = 0; // Input offset
    int op = 0; // Output offset
    int line_number = 1;

    cpp_line_map = malloc(sizeof(LineMap));
    cpp_line_map->position = ip;
    cpp_line_map->line_number = line_number;
    LineMap *lm = cpp_line_map;

    if (cpp_input_size == 0) {
        cpp_input = output;
        return;
    }

    while (ip < cpp_input_size) {
        if (cpp_input_size - ip >= 2 && cpp_input[ip] == '\\' && cpp_input[ip + 1] == '\n') {
            // Backslash newline
            line_number++;
            ip += 2;
            lm = add_to_linemap(lm, op, line_number);

        }
        else if (cpp_input[ip] == '\n') {
            // Newline
            output[op++] = '\n';
            line_number++;
            ip += 1;
            lm = add_to_linemap(lm, op, line_number);
        }
        else
            output[op++] = cpp_input[ip++];
    }

    // Add final newline if not already there
    if (op && output[op - 1] != '\n') {
        output[op++] = '\n';
        line_number++;
    }

    output[op] = 0;
    cpp_input = output;
    cpp_input_size = op;
    lm->next = 0;
}

static void advance_ip(int *ip, LineMap **lm, int *line_number) {
    (*ip)++;

    if (*lm && *ip == (*lm)->position) {
        *line_number = (*lm)->line_number;
        *lm = (*lm)->next;
    }
}

static void print_cpp_input(int print_line_numbers) {
    int ip = 0;
    LineMap *lm = cpp_line_map->next;
    int line_number = 1;

    if (print_line_numbers) printf("[%4d]", line_number);

    int print_line_number = 1;
    while (ip < cpp_input_size) {
        printf("%c", cpp_input[ip]);

        int is_newline = cpp_input[ip] == '\n';

        int old_line_number = line_number;
        advance_ip(&ip, &lm, &line_number);
        if (print_line_numbers && line_number != old_line_number) printf("[%4d]", line_number);

        if (is_newline) {
            print_line_number++;

            while (print_line_number < line_number) {
                print_line_number++;
                printf("\n");
            }
        }
    }
}

void preprocess(char *filename) {
    init_cpp(filename);
    if (opt_enable_trigraphs) transform_trigraphs();
    strip_backslash_newlines();
    print_cpp_input(0);
}
