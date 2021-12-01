#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wcc.h"

enum {
    MAX_CPP_OUTPUT_SIZE = 10 * 1024 * 1024,
};

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

static CppToken *subst(CppToken *is, CppToken *fp, CppToken *ap, StrSet *hs, CppToken *os);
static CppToken *hsadd(StrSet *hs, CppToken *ts);

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

    cpp_cur_ip = 0;
    cpp_cur_line_map = 0;
    cpp_cur_line_number = 1;
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

// Shallow copy a new CPP token
static CppToken *dup_cpp_token(CppToken *tok) {
    CppToken *result = malloc(sizeof(CppToken));
    *result = *tok;
    return result;
}

// Allocate twice the input size and round up to nearest power of two.
static void init_output(void) {
    allocated_output = 1;
    while (allocated_output < cpp_input_size * 2) allocated_output <<= 1;
    if (allocated_output > MAX_CPP_OUTPUT_SIZE) panic("Exceeded max CPP output size %d", MAX_CPP_OUTPUT_SIZE);
    cpp_output = malloc(allocated_output);
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

static void add_to_whitespace(char **whitespace, int *whitespace_pos, char c) {
    if (!*whitespace) *whitespace = malloc(1024);
    if (*whitespace_pos == 1024) panic("Ran out of whitespace buffer");
    (*whitespace)[(*whitespace_pos)++] = c;
}

static char *lex_whitespace(void) {
    char *whitespace = 0;
    int whitespace_pos = 0;

    // Process whitespace and comments
    while (cpp_cur_ip < cpp_input_size) {
        if ((cpp_input[cpp_cur_ip] == '\t' || cpp_input[cpp_cur_ip] == ' ')) {
            add_to_whitespace(&whitespace, &whitespace_pos, cpp_input[cpp_cur_ip]);
            advance_cur_ip();
            continue;
        }

        // Process // comment
        if (cpp_input_size - cpp_cur_ip >= 2 && cpp_input[cpp_cur_ip] == '/' && cpp_input[cpp_cur_ip + 1] == '/') {
            while (cpp_input[cpp_cur_ip] != '\n') advance_cur_ip();
            add_to_whitespace(&whitespace, &whitespace_pos, ' ');
            continue;
        }

        // Process /* comments */
        if (cpp_input_size - cpp_cur_ip >= 2 && cpp_input[cpp_cur_ip] == '/' && cpp_input[cpp_cur_ip + 1] == '*') {
            advance_cur_ip();
            advance_cur_ip();

            while (cpp_input_size - cpp_cur_ip >= 2 && (cpp_input[cpp_cur_ip] != '*' || cpp_input[cpp_cur_ip + 1] != '/'))
                advance_cur_ip();

            advance_cur_ip();
            advance_cur_ip();

            add_to_whitespace(&whitespace, &whitespace_pos, ' ');
            continue;
        }

        break;
    }

    if (whitespace)
        whitespace[whitespace_pos] = 0;

    return whitespace;

}

static void lex_string_and_char_literal(char delimiter) {
    char *i = cpp_input;

    if (i[cpp_cur_ip]== 'L')  advance_cur_ip();;
    advance_cur_ip();

    char *data = malloc(MAX_STRING_LITERAL_SIZE + 2);
    data[0] = delimiter;
    int size = 0;

    while (cpp_cur_ip < cpp_input_size) {
        if (i[cpp_cur_ip] == delimiter) {
            advance_cur_ip();
            break;
        }

        if (cpp_input_size - cpp_cur_ip >= 2 && i[cpp_cur_ip] == '\\') {
            if (size + 1 >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
            data[1 + size++] = '\\';
            data[1 + size++] = i[cpp_cur_ip + 1];
            advance_cur_ip();
            advance_cur_ip();
        }

        else {
            if (size >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
            data[1 + size++] = i[cpp_cur_ip];
            advance_cur_ip();
        }
    }

    data[size + 1] = delimiter;
    data[size + 2] = 0;
    cpp_cur_token = new_cpp_token(CPP_TOK_STRING_LITERAL);
    cpp_cur_token->str = data;
}

#define is_pp_number(c1, c2) (c1 >= '0' && c1 <= '9') || (cpp_input_size - cpp_cur_ip >= 2 && c1 == '.' && (c2 >= '0' && c2 <= '9'))
#define is_non_digit(c1) ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_')
#define is_exponent(c1, c2) (c1 == 'E' || c1 == 'e') && (c2 == '+' || c2 == '-')

// Lex one CPP token, starting with optional whitespace
static void cpp_next() {
    char *whitespace = lex_whitespace();

    char *i = cpp_input;

    if (cpp_cur_ip >= cpp_input_size)
        cpp_cur_token = new_cpp_token(CPP_TOK_EOF);

    else {
        char c1 = i[cpp_cur_ip];
        char c2 = i[cpp_cur_ip + 1];

        if (c1 == '\n') {
            cpp_cur_token = new_cpp_token(CPP_TOK_EOL);
            advance_cur_ip();
        }

        else if (c1 == '#') {
            cpp_cur_token = new_cpp_token(CPP_TOK_HASH);
            advance_cur_ip();
        }

        else if ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_') {
            char *identifier = malloc(1024);
            int j = 0;
            while (((i[cpp_cur_ip] >= 'a' && i[cpp_cur_ip] <= 'z') || (i[cpp_cur_ip] >= 'A' && i[cpp_cur_ip] <= 'Z') || (i[cpp_cur_ip] >= '0' && i[cpp_cur_ip] <= '9') || (i[cpp_cur_ip] == '_')) && cpp_cur_ip < cpp_input_size) {
                if (j == MAX_IDENTIFIER_SIZE) panic("Exceeded maximum identifier size %d", MAX_IDENTIFIER_SIZE);
                identifier[j] = i[cpp_cur_ip];
                j++;
                advance_cur_ip();
            }
            identifier[j] = 0;

            if      (!strcmp(identifier, "define")) cpp_cur_token = new_cpp_token(CPP_TOK_DEFINE);
            else if (!strcmp(identifier, "undef"))  cpp_cur_token = new_cpp_token(CPP_TOK_UNDEF);

            else {
                cpp_cur_token = new_cpp_token(CPP_TOK_IDENTIFIER);
                cpp_cur_token->str = identifier;
            }
        }

        else if ((c1 == '"') || (cpp_input_size - cpp_cur_ip >= 2 && c1 == 'L' && c2 == '"'))
            lex_string_and_char_literal('"');

        else if ((c1 == '\'') || (cpp_input_size - cpp_cur_ip >= 2 && c1 == 'L' && c2 == '\''))
            lex_string_and_char_literal('\'');

        else if (is_pp_number(c1, c2)) {
            int size = 1;
            char *start = &(i[cpp_cur_ip]);
            advance_cur_ip();

            while (c1 = i[cpp_cur_ip], c2 = i[cpp_cur_ip + 1], c1 == '.' || is_pp_number(c1, c2) || is_non_digit(c1) || is_exponent(c1, c2)) {
                if (is_exponent(c1, c2)) { advance_cur_ip(); size++; }
                advance_cur_ip();
                size++;
            }

            cpp_cur_token = new_cpp_token(CPP_TOK_NUMBER);
            cpp_cur_token->str = malloc(size + 1);
            memcpy(cpp_cur_token->str, start, size);
            cpp_cur_token->str[size] = 0;
        }

        else {
            cpp_cur_token = new_cpp_token(CPP_TOK_OTHER);
            cpp_cur_token->str = malloc(2);
            cpp_cur_token->str[0] = c1;
            cpp_cur_token->str[1] = 0;
            advance_cur_ip();
        }
    }

    cpp_cur_token->line_number = cpp_cur_line_number;
    cpp_cur_token->whitespace = whitespace;

    return;
}

// Append to list2 to circular linked list list1, creating list1 if it doesn't exist
CppToken *cll_append(CppToken *list1, CppToken *list2) {
    if (!list2) return list1;

    if (!list1) {
        // Make list2 into a circular linked list and return it
        list1 = list2;
        while (list2->next) list2 = list2->next;
        list2->next = list1;
        return list1;
    }

    CppToken *list1_head = list1->next;
    list1->next = list2;
    while (list1->next) list1 = list1->next;
    list1->next = list1_head;
    return list1;
}

// Convert a circular linked list to a linked list.
// cll must be at the tail of the linked list
static CppToken *convert_cll_to_ll(CppToken *cll) {
    if (!cll) return 0;

    CppToken *head = cll->next;
    cll->next = 0;
    return head;
}

// Implementation of Dave Prosser's C Preprocessing Algorithm
// This is the main macro expansion function: expand an input sequence into an output sequence.
static CppToken *expand(CppToken *is) {
    if (!is) return 0;

    if (is->str && is->hide_set && strset_in(is->hide_set, is->str)) {
        // The first token is in its own hide set, don't expand it
        // return the first token + the expanded rest
        if (!is->next) return is;
        CppToken *t = dup_cpp_token(is);
        t->next = expand(is->next);
        return t;
    }

    Directive *directive = 0;
    if (is->str) directive = strmap_get(directives, is->str);

    if (directive) {
        // Object like macro

        StrSet *identifier_hs = new_strset();
        strset_add(identifier_hs, is->str);
        StrSet *hs = strset_union(is->hide_set ? is->hide_set : new_strset(), identifier_hs);
        CppToken *substituted = subst(directive->tokens, 0, 0, hs, 0);
        if (substituted) substituted->next->whitespace = is->whitespace;
        CppToken *result = expand(convert_cll_to_ll(cll_append(substituted, is->next)));

        return result;
    }

    // The first token is an ordinary token, return the first token + the expanded rest
    CppToken *t = dup_cpp_token(is);
    t->next = expand(is->next);
    return t;
}

// Inputs:
//   is: input sequence
//   fp: formal parameters
//   ap: actual parameters
//   hs: hide set
//   os: output sequence is a circular linked list
// Output
//   output sequence in a circular linked list
static CppToken *subst(CppToken *is, CppToken *fp, CppToken *ap, StrSet *hs, CppToken *os) {
    if (!is) {
        if (!os) return os;
        return hsadd(hs, os);
    }

    // Append first token in is to os
    CppToken *new_token = dup_cpp_token(is);
    new_token->next = 0;
    os = cll_append(os, new_token);
    return subst(is->next, fp, ap, hs, os);
}

// Add a a hide set to a token sequence's hide set. ts and result are circular linked lists.
static CppToken *hsadd(StrSet *hs, CppToken *ts) {
    if (!hs) panic("Empty hs in hsadd");

    ts = convert_cll_to_ll(ts);

    CppToken *result = 0; // A circular linked list
    while (ts) {
        CppToken *new_token = dup_cpp_token(ts);
        new_token->next = 0;
        new_token->hide_set = strset_union(new_token->hide_set ? new_token->hide_set : new_strset(), hs);
        result = cll_append(result, new_token);
        ts = ts->next;
    }

    return result;
}

static CppToken *parse_define_tokens(void) {
    CppToken *tokens;

    if (cpp_cur_token->kind == CPP_TOK_EOL || cpp_cur_token->kind == CPP_TOK_EOF)
        tokens = 0;
    else {
        tokens = cpp_cur_token;
        CppToken *tokens = cpp_cur_token;
        while (cpp_cur_token->kind != CPP_TOK_EOL && cpp_cur_token->kind != CPP_TOK_EOF) {
            tokens->next = cpp_cur_token;
            tokens = tokens->next;
            cpp_next();
        }
        tokens->next = 0;
    }

    // Clear whitespace on initial token
    if (tokens) tokens->whitespace = 0;

    return tokens;
}

static void parse_directive(void) {
    cpp_next();
    switch (cpp_cur_token->kind) {
        case CPP_TOK_DEFINE:
            cpp_next();
            if (cpp_cur_token->kind != CPP_TOK_IDENTIFIER) panic("Expected identifier");

            char *identifier = cpp_cur_token->str;
            cpp_next();

            Directive *directive = malloc(sizeof(Directive));
            strmap_put(directives, identifier, directive);
            directive->tokens = parse_define_tokens();

            break;
        case CPP_TOK_UNDEF:
            cpp_next();
            if (cpp_cur_token->kind != CPP_TOK_IDENTIFIER) panic("Expected identifier");
            strmap_delete(directives, cpp_cur_token->str);
            cpp_next();

            // Ignore any tokens until EOL/EOF
            while (cpp_cur_token->kind != CPP_TOK_EOL && cpp_cur_token->kind != CPP_TOK_EOF)
                cpp_next();

            break;
    }
}

static void append_cpp_token_to_output(CppToken *token) {
    switch (token->kind) {
        case CPP_TOK_IDENTIFIER:
        case CPP_TOK_STRING_LITERAL:
        case CPP_TOK_NUMBER:
        case CPP_TOK_OTHER:
            if (token->whitespace) append_string_to_output(token->whitespace);
            append_string_to_output(token->str);
            break;
        default:
            panic("Unexpected token %d", token->kind);
    }
}

static void append_cpp_tokens_to_output(CppToken *token) {
    while (token) {
        append_cpp_token_to_output(token);
        token = token->next;
    }
}

// Parse tokens from the lexer
static void cpp_parse() {
    int print_line_number = 1;
    int in_start_of_line = 1;

    while (cpp_cur_token->kind != CPP_TOK_EOF) {
        switch (cpp_cur_token->kind) {
            case CPP_TOK_EOF:
                cpp_next();
                break;

            case CPP_TOK_EOL:
                append_char_to_output('\n');
                print_line_number++;

                while (print_line_number < cpp_cur_line_number) {
                    print_line_number++;
                    append_char_to_output('\n');
                }

                in_start_of_line = 1;

                cpp_next();
                break;

            case CPP_TOK_HASH:
                if (in_start_of_line)
                    parse_directive();
                else {
                    if (cpp_cur_token->whitespace) append_string_to_output(cpp_cur_token->whitespace);
                    append_char_to_output('#');
                    in_start_of_line = 0;
                    cpp_next();
                }

                break;

            case CPP_TOK_IDENTIFIER: {
                Directive *directive = strmap_get(directives, cpp_cur_token->str);
                if (directive)
                    append_cpp_tokens_to_output(expand(cpp_cur_token));
                else
                    append_cpp_tokens_to_output(cpp_cur_token);

                in_start_of_line = 0;

                cpp_next();
                break;
            }

            case CPP_TOK_STRING_LITERAL:
                append_cpp_token_to_output(cpp_cur_token);
                in_start_of_line = 0;

                cpp_next();
                break;

            default:
                append_cpp_tokens_to_output(expand(cpp_cur_token));
                in_start_of_line = 0;

                cpp_next();
                break;
        }
    }
}

CppToken *parse_cli_define(char *string) {
    init_cpp_from_string(string);
    cpp_next();
    return parse_define_tokens();
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
