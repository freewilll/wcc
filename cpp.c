#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

enum {
    MAX_CPP_OUTPUT_SIZE = 10 * 1024 * 1024,
};

enum {
    CPP_TOK_OTHER,
    CPP_TOK_EOL,
    CPP_TOK_EOF
};

typedef struct cpp_token {
    int kind;           // One of CPP_TOK*
    char *whitespace;   // Preceding whitespace
    char c;
    int line_number;
} CppToken;

static char *cpp_input;
static int cpp_input_size;
static LineMap *cpp_line_map;

// Current parsing state
static int cpp_cur_ip;              // Offset in cpp_input
static char *cpp_cur_filename;      // Current filename
static LineMap *cpp_cur_line_map;   // Linemap
static int cpp_cur_line_number;     // Current line number
static CppToken *cpp_cur_token;     // Current token

// Output
FILE *cpp_output_file;                     // Output file handle
static char *cpp_output;                   // Overall output
static int cpp_output_pos;                 // Position in cpp_output;
static int allocated_output;               // How much has been allocated for cpp_output;

static void init_cpp(char *filename) {
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

// Advance global current input pointers
static void advance_cur_ip(void) {
    advance_ip(&cpp_cur_ip, &cpp_cur_line_map, &cpp_cur_line_number);
}

// Create a new CPP token
static CppToken *new_cpp_token(int kind) {
    CppToken *tok = malloc(sizeof(CppToken));
    memset(tok, 0, sizeof(CppToken));

    tok->kind = kind;

    return tok;
}

// Allocate twice the input size and round up to nearest power of two.
static void init_output(void) {
    allocated_output = 1;
    while (allocated_output < cpp_input_size * 2) allocated_output <<= 1;
    if (allocated_output > MAX_CPP_OUTPUT_SIZE) panic("Exceeded max CPP output size %d", MAX_CPP_OUTPUT_SIZE);
    cpp_output = malloc(sizeof(allocated_output));
}

// Append a character to the output
static void append_char_to_output(char c) {
    if (cpp_output_pos == allocated_output) {
        allocated_output *= 2;
        if (allocated_output > MAX_CPP_OUTPUT_SIZE) panic("Exceeded max CPP output size %d", MAX_CPP_OUTPUT_SIZE);
        cpp_output = realloc(cpp_output, allocated_output);
    }
    cpp_output[cpp_output_pos++] = c;
}

// Append a string to the output
static void append_string_to_output(char *s) {
    int len = strlen(s);
    int needed = cpp_output_pos + len;
    if (allocated_output < needed) {
        while (allocated_output < needed) allocated_output <<= 1;
        if (allocated_output > MAX_CPP_OUTPUT_SIZE) panic("Exceeded max CPP output size %d", MAX_CPP_OUTPUT_SIZE);
        cpp_output = realloc(cpp_output, allocated_output);
    }
    sprintf(cpp_output + cpp_output_pos, "%s", s);
    cpp_output_pos += len;
}

// Lex one CPP token, starting with optional whitespace
static void cpp_next() {
    char *whitespace = 0;
    int whitespace_pos = 0;
    while (cpp_cur_ip < cpp_input_size && (cpp_input[cpp_cur_ip] == '\t' || cpp_input[cpp_cur_ip] == ' ')) {
        if (!whitespace) whitespace = malloc(1024);
        if (whitespace_pos == 1024) panic("Ran out of whitespace buffer");
        whitespace[whitespace_pos++] = cpp_input[cpp_cur_ip];
        advance_cur_ip();
    }

    if (whitespace)
        whitespace[whitespace_pos] = 0;

    if (cpp_cur_ip >= cpp_input_size)
        cpp_cur_token = new_cpp_token(CPP_TOK_EOF);
    else {
        if (cpp_input[cpp_cur_ip] == '\n')
            cpp_cur_token = new_cpp_token(CPP_TOK_EOL);
        else {
            cpp_cur_token = new_cpp_token(CPP_TOK_OTHER);
            cpp_cur_token->c = cpp_input[cpp_cur_ip];
        }
        advance_cur_ip();
    }

    cpp_cur_token->line_number = cpp_cur_line_number;
    cpp_cur_token->whitespace = whitespace;

    return;
}

// Parse tokens from the lexer
static void cpp_parse() {
    int print_line_number = 1;

    while (cpp_cur_token->kind != CPP_TOK_EOF) {
        switch (cpp_cur_token->kind) {
            case CPP_TOK_EOF:
                break;
            case CPP_TOK_EOL:
                append_char_to_output('\n');
                print_line_number++;

                while (print_line_number < cpp_cur_line_number) {
                    print_line_number++;
                    append_char_to_output('\n');
                }

                break;
            default:
                if (cpp_cur_token->whitespace) append_string_to_output(cpp_cur_token->whitespace);
                append_char_to_output(cpp_cur_token->c);
                break;
        }

        cpp_next();
    }
}

void preprocess(char *filename, char *output_filename) {
    init_cpp(filename);

    if (opt_enable_trigraphs) transform_trigraphs();

    strip_backslash_newlines();

    // Initialize the lexer
    cpp_cur_ip = 0;
    cpp_cur_line_map = cpp_line_map->next;
    cpp_cur_line_number = 1;
    cpp_next();

    // Parse
    init_output();
    cpp_parse();
    append_char_to_output(0);

    // Print the output
    if (!output_filename || !strcmp(output_filename, "-"))
        cpp_output_file = stdout;
    else {
        // Open output file for writing
        cpp_output_file = fopen(output_filename, "w");
        if (cpp_output_file == 0) {
            perror(output_filename);
            exit(1);
        }
    }

    fprintf(cpp_output_file, "%s", cpp_output);
    fclose(cpp_output_file);
}
