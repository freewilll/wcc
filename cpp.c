#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "wcc.h"

enum {
    MAX_CPP_OUTPUT_SIZE = 10 * 1024 * 1024,
};

enum hchar_lex_state {
    HLS_START_OF_LINE,
    HLS_SEEN_HASH,
    HLS_SEEN_INCLUDE,
    HLS_OTHER,
};

typedef struct conditional_include {
    int matched;    // Is the evaluated controlling expression 1?
    int skipping;   // Is a group being skipped?
    struct conditional_include *prev;
} ConditionalInclude;

// Lexing/parsing state for current file
typedef struct cpp_state {
    char *     input;
    int        input_size;
    int        ip;                 // Offset in input
    char *     filename;           // Current filename
    char *     override_filename;  // Overridden filename with #line
    LineMap *  line_map;           // Linemap
    int        line_number;        // Current line number
    int        line_number_offset; // Difference in line number set by #line directive
    CppToken * token;              // Current token
    int        hchar_lex_state;    // State of #include <...> lexing
    int        output_line_number; // How many lines have been outputted
} CppState;

CppState state;

ConditionalInclude *conditional_include_stack;

static int include_depth;

// Output
FILE *cpp_output_file;         // Output file handle
StringBuffer *output;          // Output string buffer;

static void cpp_next();
static void cpp_parse();
static CppToken *subst(CppToken *is, StrMap *fp, CppToken **ap, StrSet *hs, CppToken *os);
static CppToken *hsadd(StrSet *hs, CppToken *ts);

const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char *BUILTIN_INCLUDE_PATHS[5] = {
    BUILD_DIR "/include/",  // Set during compilation to local wcc source dir
    "/usr/local/include/",
    "/usr/include/x86_64-linux-gnu/",
    "/usr/include/",
    0,
};

void debug_print_token_sequence(CppToken *ts) {
    printf("|");
    while (ts) {
        if (ts->whitespace) printf("[%s]", ts->whitespace);
        printf("%s|", ts->str);
        ts = ts->next;
    }
    printf("\n");
}

void debug_print_cll_token_sequence(CppToken *ts) {
    if (!ts) {
        printf("|\n");
        return;
    }

    CppToken *head = ts->next;
    ts = head;

    printf("|");
    do {
        if (ts->whitespace) printf("[%s]", ts->whitespace);
        printf("%s|", ts->str);
        ts = ts->next;
    } while (ts != head);
    printf("\n");
}

static void init_cpp_from_fh(FILE *f, char *full_path, char *filename) {
    fseek(f, 0, SEEK_END);
    state.input_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    state.input = malloc(state.input_size + 1);
    int read = fread(state.input, 1, state.input_size, f);
    if (read != state.input_size) {
        printf("Unable to read input file\n");
        exit(1);
    }

    fclose(f);

    state.input[state.input_size] = 0;
    state.filename = filename;
    cur_filename = full_path;
    filename = filename;
    state.hchar_lex_state = HLS_START_OF_LINE;

    conditional_include_stack = malloc(sizeof(ConditionalInclude));
    memset(conditional_include_stack, 0, sizeof(ConditionalInclude));
}

static void output_line_directive(int offset, int add_eol) {
    char *buf = malloc(256);
    char *filename = state.override_filename ? state.override_filename : state.filename;
    sprintf(buf, "# %d \"%s\"%s", state.line_number_offset + state.line_number + offset, filename, add_eol ? "\n" : "");
    append_to_string_buffer(output, buf);
    free(buf);
}

// If the output has an amount newlines > threshold, collapse them into a # line statement
static void collapse_trailing_newlines(int threshold, int output_line) {
    int count = 0;
    while (output->position - count > 0 && output->data[output->position - count - 1] == '\n') count++;

    if (count > threshold) {
        // Rewind output by count -1 characters
        output->data[output->position - count + 1] = '\n';
        output->position -= count - 1;

        if (output_line) output_line_directive(-1, 1);
    }
}

// Run the preprocessor on an opened file handle
static void run_preprocessor_on_file(char *filename, int first_file) {
    if (opt_enable_trigraphs) transform_trigraphs();

    strip_backslash_newlines();

    // Initialize the lexer
    state.ip = 0;
    state.line_map = state.line_map->next;
    state.line_number = 1;
    state.line_number_offset = 0;
    state.override_filename = 0;
    cpp_next();

    append_to_string_buffer(output, "# 1 \"");
    append_to_string_buffer(output, filename);
    append_to_string_buffer(output, "\"");
    if (!first_file) append_to_string_buffer(output, " 1");
    append_to_string_buffer(output, "\n");

    cpp_parse();

    collapse_trailing_newlines(0, 0);
}

void init_cpp_from_string(char *string) {
    state.input = string;
    state.input_size = strlen(string);
    state.filename = 0;

    state.ip = 0;
    state.line_map = 0;
    state.line_number = 1;
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

static void add_builtin_directive(char *identifier, DirectiveRenderer renderer) {
    Directive *directive = malloc(sizeof(Directive));
    memset(directive, 0, sizeof(Directive));
    directive->renderer = renderer;
    strmap_put(directives, identifier, directive);
}

static CppToken *render_file(CppToken *directive_token) {
    CppToken *result = new_cpp_token(CPP_TOK_STRING_LITERAL);
    wasprintf(&(result->str), "\"%s\"", state.override_filename ? state.override_filename : state.filename);
    return result;
}

static CppToken *render_line(CppToken *directive_token) {
    CppToken *result = new_cpp_token(CPP_TOK_NUMBER);
    wasprintf(&(result->str), "%d", directive_token->line_number_offset + directive_token->line_number);
    return result;
}

static CppToken *render_time(CppToken *directive_token) {
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    CppToken *result = new_cpp_token(CPP_TOK_STRING_LITERAL);
    wasprintf(&(result->str), "\"%02d:%02d:%02d\"", info->tm_hour, info->tm_min, info->tm_sec);
    return result;
}

static CppToken *render_date(CppToken *directive_token) {
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    CppToken *result = new_cpp_token(CPP_TOK_STRING_LITERAL);
    wasprintf(&(result->str), "\"%3s %2d %04d\"", months[info->tm_mon], info->tm_mday, info->tm_year + 1900);
    return result;
}

static CppToken *render_numeric_token(int value) {
    CppToken *result = new_cpp_token(CPP_TOK_NUMBER);
    wasprintf(&(result->str), "%d", value);
    return result;
}

static CppToken *render_stdc(CppToken *directive_token) {
    return render_numeric_token(1);
}

// Create empty directives strmap and add CLI directives to them
void init_directives(void) {
    directives = new_strmap();
    CliDirective *cd = cli_directives;
    while (cd) {
        strmap_put(directives, cd->identifier, cd->directive);
        cd = cd->next;
    }

    add_builtin_directive("__FILE__", render_file);
    add_builtin_directive("__LINE__", render_line);
    add_builtin_directive("__TIME__", render_time);
    add_builtin_directive("__DATE__", render_date);
    add_builtin_directive("__STDC__", render_stdc);

    Directive *directive = malloc(sizeof(Directive));
    memset(directive, 0, sizeof(Directive));
    directive->tokens = render_numeric_token(1);
    strmap_put(directives, "__x86_64__", directive);
}

char *get_cpp_input(void) {
    return state.input;
}

LineMap *get_cpp_linemap(void) {
    return state.line_map;
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
    char *output = malloc(state.input_size + 1);
    int ip = 0; // Input offset
    int op = 0; // Output offset

    while (ip < state.input_size) {
        if (state.input_size - ip >= 3) {
            // Check for trigraphs
            char c1 = state.input[ip];
            char c2 = state.input[ip + 1];
            char c3 = state.input[ip + 2];

                 if (c1 == '?' && c2 == '?' && c3 == '=')  { output[op++] = '#';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '(')  { output[op++] = '[';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '/')  { output[op++] = '\\'; ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == ')')  { output[op++] = ']';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '\'') { output[op++] = '^';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '<')  { output[op++] = '{';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '!')  { output[op++] = '|';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '>')  { output[op++] = '}';  ip += 3; }
            else if (c1 == '?' && c2 == '?' && c3 == '-')  { output[op++] = '~';  ip += 3; }
            else output[op++] = state.input[ip++];
        }
        else output[op++] = state.input[ip++];
    }

    output[op] = 0;
    state.input = output;
    state.input_size = op;
}

void strip_backslash_newlines(void) {
    char *output = malloc(state.input_size + 1);
    int ip = 0; // Input offset
    int op = 0; // Output offset
    int line_number = 1;

    state.line_map = malloc(sizeof(LineMap));
    state.line_map->position = ip;
    state.line_map->line_number = line_number;
    LineMap *lm = state.line_map;

    if (state.input_size == 0) {
        state.input = output;
        return;
    }

    while (ip < state.input_size) {
        if (state.input_size - ip >= 2 && state.input[ip] == '\\' && state.input[ip + 1] == '\n') {
            // Backslash newline
            line_number++;
            ip += 2;
            lm = add_to_linemap(lm, op, line_number);

        }
        else if (state.input[ip] == '\n') {
            // Newline
            output[op++] = '\n';
            line_number++;
            ip += 1;
            lm = add_to_linemap(lm, op, line_number);
        }
        else
            output[op++] = state.input[ip++];
    }

    // Add final newline if not already there
    if (op && output[op - 1] != '\n') {
        output[op++] = '\n';
        line_number++;
    }

    output[op] = 0;
    state.input = output;
    state.input_size = op;
    lm->next = 0;
}

// Determine if two tokens that don't have a space in between them already need one.
// The specs are unclear about this, so I followed gcc's cpp_avoid_paste() in lex.c.
static int need_token_space(CppToken *t1, CppToken *t2) {
    if (t2->kind == '=') {
        switch(t1->kind) {
            case '=':
            case '!':
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '<':
            case '>':
            case '&':
            case '|':
            case '^':
                return 1;
        }
    }

         if (t1->kind == '<' && t2->kind == '<') return 1;
    else if (t1->kind == '>' && t2->kind == '>') return 1;
    else if (t1->kind == '+' && t2->kind == '+') return 1;
    else if (t1->kind == '-' && t2->kind == '-') return 1;
    else if (t1->kind == '&' && t2->kind == '&') return 1;
    else if (t1->kind == '|' && t2->kind == '|') return 1;
    else if (t1->kind == '-' && t2->kind == '>') return 1;
    else if (t1->kind == '/' && t2->kind == '*') return 1;

    else if (t1->kind == '+' && t2->kind == CPP_TOK_INC) return 1;
    else if (t1->kind == '-' && t2->kind == CPP_TOK_DEC) return 1;
    else if (t1->kind == '.' && t2->kind == CPP_TOK_NUMBER) return 1;

    else if (t1->kind == CPP_TOK_IDENTIFIER && t2->kind == CPP_TOK_IDENTIFIER) return 1;
    else if (t1->kind == CPP_TOK_IDENTIFIER && t2->kind == CPP_TOK_NUMBER) return 1;
    else if (t1->kind == CPP_TOK_IDENTIFIER && t2->kind == CPP_TOK_STRING_LITERAL) return 1;
    else if (t1->kind == CPP_TOK_NUMBER     && t2->kind == CPP_TOK_IDENTIFIER) return 1;
    else if (t1->kind == CPP_TOK_NUMBER     && t2->kind == CPP_TOK_NUMBER) return 1;
    else if (t1->kind == CPP_TOK_STRING_LITERAL && t2->kind == CPP_TOK_STRING_LITERAL) return 1;

    return 0;
}

static void append_tokens_to_string_buffer(StringBuffer *sb, CppToken *token, int collapse_whitespace) {
    CppToken *prev = 0;
    while (token) {
        int is_eol = (token->kind == CPP_TOK_EOL);

        if (!is_eol && token->whitespace && !collapse_whitespace)
            append_to_string_buffer(sb, token->whitespace);
        else if (!is_eol && token->whitespace && collapse_whitespace)
            append_to_string_buffer(sb, " ");
        else if (prev && need_token_space(prev, token))
            append_to_string_buffer(sb, " ");

        collapse_trailing_newlines(8, 1);
        append_to_string_buffer(sb, token->str);

        prev = token;
        token = token->next;

        if (is_eol) state.output_line_number++;

        if (is_eol && token) {
            // Output sufficient newlines to catch up with token->line_number
            // This ensures that each line in the input ends up on the same line
            // in the output.
            while (state.output_line_number < token->line_number) {
                state.output_line_number++;
                append_to_string_buffer(sb, "\n");
            }
            collapse_trailing_newlines(8, 1);
        }
    }
}

static void append_tokens_to_output(CppToken *tokens) {
    append_tokens_to_string_buffer(output, tokens, 0);
}

static void advance_ip(int *ip, LineMap **lm, int *line_number) {
    (*ip)++;

    if (*lm && *ip == (*lm)->position) {
        *line_number = (*lm)->line_number;
        cur_line = *line_number;
        *lm = (*lm)->next;
    }
}

// Advance global current input pointers
static void advance_cur_ip(void) {
    advance_ip(&state.ip, &state.line_map, &state.line_number);
}

static void advance_cur_ip_by_count(int count) {
    for (int i = 0; i < count; i++)
        advance_ip(&state.ip, &state.line_map, &state.line_number);
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
    while (state.ip < state.input_size) {
        if ((state.input[state.ip] == '\t' || state.input[state.ip] == ' ')) {
            add_to_whitespace(&whitespace, &whitespace_pos, state.input[state.ip]);
            advance_cur_ip();
            continue;
        }

        // Process // comment
        if (state.input_size - state.ip >= 2 && state.input[state.ip] == '/' && state.input[state.ip + 1] == '/') {
            while (state.input[state.ip] != '\n') advance_cur_ip();
            add_to_whitespace(&whitespace, &whitespace_pos, ' ');
            continue;
        }

        // Process /* comments */
        if (state.input_size - state.ip >= 2 && state.input[state.ip] == '/' && state.input[state.ip + 1] == '*') {
            advance_cur_ip();
            advance_cur_ip();

            while (state.input_size - state.ip >= 2 && (state.input[state.ip] != '*' || state.input[state.ip + 1] != '/'))
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
    char *i = state.input;
    int data_offset = 0;

    char *data = malloc(MAX_STRING_LITERAL_SIZE + 2);

    if (i[state.ip] == 'L') {
        advance_cur_ip();
        data_offset = 1;
        data[0] = 'L';
    }

    advance_cur_ip();

    data[data_offset] = delimiter == '>' ? '<' : delimiter;
    int size = 0;

    while (state.ip < state.input_size) {
        if (i[state.ip] == delimiter) {
            advance_cur_ip();
            break;
        }

        if (state.input_size - state.ip >= 2 && i[state.ip] == '\\') {
            if (size + 1 >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
            data[data_offset + 1 + size++] = '\\';
            data[data_offset + 1 + size++] = i[state.ip + 1];
            advance_cur_ip();
            advance_cur_ip();
        }

        else {
            if (size >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
            data[data_offset + 1 + size++] = i[state.ip];
            advance_cur_ip();
        }
    }

    data[data_offset + size + 1] = delimiter;
    data[data_offset + size + 2] = 0;

    state.token = new_cpp_token(CPP_TOK_STRING_LITERAL);
    state.token->str = data;
}

#define is_pp_number(c1, c2) (c1 >= '0' && c1 <= '9') || (state.input_size - state.ip >= 2 && c1 == '.' && (c2 >= '0' && c2 <= '9'))
#define is_non_digit(c1) ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_')
#define is_exponent(c1, c2) (c1 == 'E' || c1 == 'e') && (c2 == '+' || c2 == '-')

#define make_token(kind, size) \
    do { \
        state.token = new_cpp_token(kind); \
        copy_token_str(start, size); \
        advance_cur_ip_by_count(size); \
    } while (0)

#define make_other_token(size) make_token(CPP_TOK_OTHER, size)

#define copy_token_str(start, size) \
    do { \
        state.token->str = malloc(size + 1); \
        memcpy(state.token->str, start, size); \
        state.token->str[size] = 0; \
    } while (0)

// Lex one CPP token, starting with optional whitespace
static void cpp_next() {
    char *whitespace = lex_whitespace();

    char *i = state.input;

    if (state.ip >= state.input_size)
        state.token = new_cpp_token(CPP_TOK_EOF);

    else {
        int left = state.input_size - state.ip;
        char c1 = i[state.ip];
        char c2 = i[state.ip + 1];
        char c3 = left >= 3 ? i[state.ip + 2] : 0;
        char *start = &(i[state.ip]);

        if (c1 == '\n') {
            state.token = new_cpp_token(CPP_TOK_EOL);
            state.token->str = "\n";
            state.token->line_number = state.line_number; // Needs to be the line number of the \n token, not the next token
            state.hchar_lex_state = HLS_START_OF_LINE;
            advance_cur_ip();
        }

        else if (c1 == '#' && c2 == '#')
            make_token(CPP_TOK_PASTE, 2);

        else if (c1 == '#')
            make_token(CPP_TOK_HASH, 1);

        else if (left >= 2 && c1 == '&' && c2 == '&'             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '|' && c2 == '|'             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '=' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '!' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '<' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '>' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '+' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '-' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '*' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '/' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '%' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '&' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '|' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '^' && c2 == '='             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '-' && c2 == '>'             )  { make_other_token(2); }
        else if (left >= 3 && c1 == '>' && c2 == '>' && c3 == '=')  { make_other_token(3); }
        else if (left >= 3 && c1 == '<' && c2 == '<' && c3 == '=')  { make_other_token(3); }
        else if (left >= 3 && c1 == '.' && c2 == '.' && c3 == '.')  { make_other_token(3); }
        else if (left >= 2 && c1 == '+' && c2 == '+'             )  { make_token(CPP_TOK_INC, 2); }
        else if (left >= 2 && c1 == '-' && c2 == '-'             )  { make_token(CPP_TOK_DEC, 2); }
        else if (             c1 == '('                          )  { make_token(CPP_TOK_LPAREN, 1); }
        else if (             c1 == ')'                          )  { make_token(CPP_TOK_RPAREN, 1); }
        else if (             c1 == ','                          )  { make_token(CPP_TOK_COMMA, 1); }

        else if ((c1 == '"') || (state.input_size - state.ip >= 2 && c1 == 'L' && c2 == '"'))
            lex_string_and_char_literal('"');

        else if ((c1 == '\'') || (state.input_size - state.ip >= 2 && c1 == 'L' && c2 == '\''))
            lex_string_and_char_literal('\'');

        else if (c1 == '<' && state.hchar_lex_state == HLS_SEEN_INCLUDE) {
            lex_string_and_char_literal('>');
            state.token->kind = CPP_TOK_HCHAR_STRING_LITERAL;
            state.hchar_lex_state = HLS_OTHER;
        }

        else if ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_') {
            char *identifier = malloc(1024);
            int j = 0;
            while (((i[state.ip] >= 'a' && i[state.ip] <= 'z') || (i[state.ip] >= 'A' && i[state.ip] <= 'Z') || (i[state.ip] >= '0' && i[state.ip] <= '9') || (i[state.ip] == '_')) && state.ip < state.input_size) {
                if (j == MAX_IDENTIFIER_SIZE) panic("Exceeded maximum identifier size %d", MAX_IDENTIFIER_SIZE);
                identifier[j] = i[state.ip];
                j++;
                advance_cur_ip();
            }
            identifier[j] = 0;

            #define is_identifier(t) (\
                t->kind == CPP_TOK_IDENTIFIER || \
                t->kind == CPP_TOK_INCLUDE    || \
                t->kind == CPP_TOK_DEFINE     || \
                t->kind == CPP_TOK_UNDEF      || \
                t->kind == CPP_TOK_IF         || \
                t->kind == CPP_TOK_IFDEF      || \
                t->kind == CPP_TOK_IFNDEF     || \
                t->kind == CPP_TOK_ELIF       || \
                t->kind == CPP_TOK_ELSE       || \
                t->kind == CPP_TOK_ENDIF      || \
                t->kind == CPP_TOK_LINE       || \
                t->kind == CPP_TOK_DEFINED)

            if      (!strcmp(identifier, "define"))   { state.token = new_cpp_token(CPP_TOK_DEFINE);  state.token->str = "define";  }
            else if (!strcmp(identifier, "include"))  { state.token = new_cpp_token(CPP_TOK_INCLUDE); state.token->str = "include"; }
            else if (!strcmp(identifier, "undef"))    { state.token = new_cpp_token(CPP_TOK_UNDEF);   state.token->str = "undef";   }
            else if (!strcmp(identifier, "if"))       { state.token = new_cpp_token(CPP_TOK_IF);      state.token->str = "if";      }
            else if (!strcmp(identifier, "ifdef"))    { state.token = new_cpp_token(CPP_TOK_IFDEF);   state.token->str = "ifdef";   }
            else if (!strcmp(identifier, "ifndef"))   { state.token = new_cpp_token(CPP_TOK_IFNDEF);  state.token->str = "ifndef";  }
            else if (!strcmp(identifier, "elif"))     { state.token = new_cpp_token(CPP_TOK_ELIF);    state.token->str = "elif";    }
            else if (!strcmp(identifier, "else"))     { state.token = new_cpp_token(CPP_TOK_ELSE);    state.token->str = "else";    }
            else if (!strcmp(identifier, "endif"))    { state.token = new_cpp_token(CPP_TOK_ENDIF);   state.token->str = "endif";   }
            else if (!strcmp(identifier, "line"))     { state.token = new_cpp_token(CPP_TOK_LINE);    state.token->str = "line";    }
            else if (!strcmp(identifier, "defined"))  { state.token = new_cpp_token(CPP_TOK_DEFINED); state.token->str = "defined"; }

            else {
                state.token = new_cpp_token(CPP_TOK_IDENTIFIER);
                state.token->str = identifier;
            }
        }

        else if (is_pp_number(c1, c2)) {
            int size = 1;
            char *start = &(i[state.ip]);
            advance_cur_ip();

            while (c1 = i[state.ip], c2 = i[state.ip + 1], c1 == '.' || is_pp_number(c1, c2) || is_non_digit(c1) || is_exponent(c1, c2)) {
                if (is_exponent(c1, c2)) { advance_cur_ip(); size++; }
                advance_cur_ip();
                size++;
            }

            state.token = new_cpp_token(CPP_TOK_NUMBER);
            copy_token_str(start, size);
        }

        else
            make_token(c1, 1);
    }

    // Set the line number of not already set
    if (!state.token->line_number) {
        state.token->line_number = state.line_number;
        state.token->line_number_offset = state.line_number_offset;
    }

    state.token->whitespace = whitespace;

    if (state.hchar_lex_state == HLS_START_OF_LINE && state.token->kind == CPP_TOK_HASH   ) state.hchar_lex_state = HLS_SEEN_HASH;
    if (state.hchar_lex_state == HLS_SEEN_HASH     && state.token->kind == CPP_TOK_INCLUDE) state.hchar_lex_state = HLS_SEEN_INCLUDE;

    return;
}

// Append list2 to circular linked list list1, creating list1 if it doesn't exist
CppToken *cll_append(CppToken *list1, CppToken *list2) {
    if (!list2) return list1;

    if (!list1) {
        // Make list2 into a circular linked list and return it
        list1 = list2;
        while (list2->next) list2 = list2->next;
        list2->next = list1;
        return list2;
    }

    CppToken *list1_head = list1->next;
    list1->next = list2;
    while (list1->next) list1 = list1->next;
    list1->next = list1_head;
    return list1;
}

// Convert a circular linked list to a linked list.
static CppToken *convert_cll_to_ll(CppToken *cll) {
    if (!cll) return 0;

    CppToken *head = cll->next;
    cll->next = 0;
    return head;
}

// Set line number on all tokens in circular linked list ts
static void set_line_number_on_token_sequence(CppToken *ts, int line_number) {
    CppToken *tail = ts;
    ts = ts->next;
    do {
        ts->line_number = line_number;
        ts = ts->next;
    } while (ts != tail);
}

#define add_token_to_actuals() CppToken *t = dup_cpp_token(token); t->next = 0; current_actual = cll_append(current_actual, t);

// Parse function-like macro call, starting with the '('
// Loop over ts; ts is advanced up to the enclosing ')'
// Returns a ptr to the token sequences for each actual parameter.
// Nested () are taken into account, for cases like f((1, 2), 3), which
// results in (1, 2) and 3 for the actual parameters.
CppToken **make_function_actual_parameters(CppToken **ts) {
    CppToken **result = malloc(sizeof(CppToken *) * MAX_CPP_MACRO_PARAM_COUNT);
    memset(result, 0, sizeof(CppToken *) * MAX_CPP_MACRO_PARAM_COUNT);

    int parenthesis_nesting_level = 0;
    CppToken *current_actual = 0;
    int index = 0;

    while (1) {
        CppToken *token = *ts;
        if (!token) panic("Expected macro function-like parameter or )");

        if (token->kind == CPP_TOK_RPAREN && !parenthesis_nesting_level) {
            // We're not in nested parentheses
            result[index++] = convert_cll_to_ll(current_actual);
            return result;
        }
        else if (token->kind == CPP_TOK_RPAREN) {
            // We're in nested parentheses
            add_token_to_actuals();
            parenthesis_nesting_level--;
        }
        else if (token->kind == CPP_TOK_LPAREN) {
            // Enter nested parentheses
            add_token_to_actuals();
            parenthesis_nesting_level++;
        }
        else if (token->kind == CPP_TOK_COMMA && !parenthesis_nesting_level) {
            // Finish current ap and start next ap
            result[index++] = convert_cll_to_ll(current_actual);
            current_actual = 0;
        }
        else {
            // It's an ordinary token, append it to current actual
            add_token_to_actuals();
        }

        *ts =(*ts)->next;
    }
}

// Union two sets. Either or both may be NULL
#define safe_strset_union(set1, set2) \
    set1 && set2 \
        ? strset_union(set1, set2) \
        : set1 \
            ? set1 \
            : set2

// Intersect two sets. Either or both may be NULL
#define safe_strset_intersection(set1, set2) \
    set1 && set2 \
        ? strset_intersection(set1, set2) \
        : set1 \
            ? set1 \
            : set2

// Implementation of Dave Prosser's C Preprocessing Algorithm
// This is the main macro expansion function: expand an input sequence into an output sequence.
static CppToken *expand(CppToken *is) {
    if (!is) return 0;

    CppToken *is1 = is->next;
    while (is1 && is1->kind == CPP_TOK_EOL) is1 = is1->next;

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

    if (directive && !directive->is_function) {
        // Object like macro

        StrSet *identifier_hs = new_strset();
        strset_add(identifier_hs, is->str);
        StrSet *hs = is->hide_set ? strset_union(is->hide_set, identifier_hs) : identifier_hs;
        CppToken *replacement_tokens = directive->renderer ? directive->renderer(is) : directive->tokens;
        CppToken *substituted = subst(replacement_tokens, 0, 0, hs, 0);
        if (substituted) {
            set_line_number_on_token_sequence(substituted, is->line_number);
            substituted->next->whitespace = is->whitespace;
        }
        CppToken *with_is_next = cll_append(substituted, is->next);
        return expand(convert_cll_to_ll(with_is_next));
    }

    else if (directive && directive->is_function && is1 && is1->kind == CPP_TOK_LPAREN) {
        // Function like macro

        CppToken *directive_token = is;
        StrSet *directive_token_hs = is->hide_set;
        is = is1->next;
        CppToken **actuals = make_function_actual_parameters(&is);
        if (!is || is->kind != CPP_TOK_RPAREN) panic("Expected )");

        int actual_count = 0;
        for (CppToken **tok = actuals; *tok; tok++, actual_count++);
        if (actual_count > directive->param_count) panic("Mismatch in number of macro parameters");

        StrSet *rparen_hs = is->hide_set;
        CppToken *rest = is->next;

        // Make the hideset for the macro subsitution
        // T is the directive token
        // HS is directive token hide set
        // HS' is ')' hide set
        // HS = (HS ∩ HS’) ∪ {T}
        StrSet *hs1 = safe_strset_intersection(directive_token_hs, rparen_hs);

        StrSet *hs2 = new_strset();
        strset_add(hs2, directive_token->str);

        StrSet *hs = safe_strset_union(hs1, hs2);

        CppToken *substituted = subst(directive->tokens, directive->param_identifiers, actuals, hs, 0);
        if (substituted) {
            set_line_number_on_token_sequence(substituted, directive_token->line_number);
            substituted->next->whitespace = directive_token->whitespace;
        }

        return expand(convert_cll_to_ll(cll_append(substituted, rest)));
    }

    // The first token is an ordinary token, return the first token + the expanded rest
    CppToken *t = dup_cpp_token(is);
    t->next = expand(is->next);
    return t;
}

// Returns a new token which is the stringized version of the input token sequence
static CppToken *stringize(CppToken *ts) {
    StringBuffer *rendered = new_string_buffer(128);

    append_tokens_to_string_buffer(rendered, ts, 1);

    terminate_string_buffer(rendered);

    StringBuffer *escaped = new_string_buffer(strlen(rendered->data) * 2);
    append_to_string_buffer(escaped, "\"");

    int in_string = 0;
    int in_char = 0;

    char *s = rendered->data;
    while (*s == ' ') s++; // Skip leading whitespace

    for (; *s; s++) {
        char c = *s;

        if (c == '"') {
            append_to_string_buffer(escaped, "\\\"");
            in_string = !in_string;
        }
        else if (c == '\'') {
            append_to_string_buffer(escaped, "'");
            in_char = !in_char;
        }
        else if (c == '\\') {
            char c2 = s[1];
            if (in_string && c2 == '"') {
                append_to_string_buffer(escaped, "\\\\\\\"");  // Escape \"
                s++;
                continue;
            }
            else if (in_char && c2 == '\'') {
                append_to_string_buffer(escaped, "\\\\'");  // Escape \'
                s++;
                continue;
            }
            else if ((in_string || in_char) && (c2 == '\\')) {
                append_to_string_buffer(escaped, "\\\\\\\\");  // Escape double backslash
                s++;
                continue;
            }

            if (in_string || in_char)
                append_to_string_buffer(escaped, "\\\\");
            else
                append_to_string_buffer(escaped, "\\");
        }
        else {
            char s[2] = {c, 0};
            append_to_string_buffer(escaped, s);
        }
    }

    append_to_string_buffer(escaped, "\"");

    CppToken *result = new_cpp_token(CPP_TOK_STRING_LITERAL);
    result->str = escaped->data;
    return result;
}

// Merge two token lists together. ls is a circular linked list, rs is an linked list.
// ls is mutated and the result is ls.
static CppToken *glue(CppToken *ls, CppToken *rs) {
    if (ls == 0) return cll_append(0, rs);
    if (rs == 0) panic("Attempt to glue an empty right side");

    // Mutating ls, this is allowed cause ls is append only
    wasprintf(&ls->str, "%s%s", ls->str, rs->str);
    ls->kind = CPP_TOK_OTHER;
    ls->hide_set = safe_strset_intersection(ls->hide_set, rs->hide_set);

    // Copy all elements from the right side except the first
    rs = rs->next;
    while (rs) {
        CppToken *tok = dup_cpp_token(rs);
        tok->next = 0;
        ls = cll_append(ls, tok);
        rs = rs->next;
    }

    return ls;
}

// Inputs:
//   is: input sequence
//   fp: formal parameters
//   ap: actual parameters
//   hs: hide set
//   os: output sequence is a circular linked list
// Output
//   output sequence in a circular linked list
static CppToken *subst(CppToken *is, StrMap *fp, CppToken **ap, StrSet *hs, CppToken *os) {
    // Empty token sequence, update the hide set and return output sequence
    if (!is) {
        if (!os) return os;
        return hsadd(hs, os);
    }

    CppToken *is1 = is->next;
    CppToken *is2 = is1 ? is1->next : 0;

    int is_fp_index = fp ? (int) (long) strmap_get(fp, is->str) : 0;
    int is1_fp_index = fp && is1 ? (int) (long) strmap_get(fp, is1->str) : 0;
    int is2_fp_index = fp && is2 ? (int) (long) strmap_get(fp, is2->str) : 0;

    // # FP
    if (is->kind == CPP_TOK_HASH && is1_fp_index) {
        CppToken *replacement = ap[is1_fp_index - 1];
        os = cll_append(os, stringize(replacement));

        return subst(is1->next, fp, ap, hs, os);
    }

    // ## FP
    if (is->kind == CPP_TOK_PASTE && is1_fp_index) {
        CppToken *replacement = ap[is1_fp_index - 1];
        if (!replacement)
            return subst(is1->next, fp, ap, hs, os);
        else
            return subst(is1->next, fp, ap, hs, glue(os, replacement));
    }

    // ## TS
    if (is->kind == CPP_TOK_PASTE) {
        if (!is1) panic("Got ## without a following token");
        CppToken *tok = dup_cpp_token(is1);
        tok->next = 0;
        return subst(is1->next, fp, ap, hs, glue(os, tok));
    }

    // FP ##
    if (is_fp_index && is1 && is1->kind == CPP_TOK_PASTE) {
        CppToken *replacement = ap[is_fp_index - 1];
        if (!replacement) {
            if (is2 && is2_fp_index) {
                // (empty) ## (replacement2) ...
                CppToken *replacement2 = ap[is2_fp_index - 1];
                return subst(is2->next, fp, ap, hs, cll_append(os, replacement2));
            }
            else
                // (empty) ## (empty) ...
                return subst(is2, fp, ap, hs, os);
        }
        else
            // (replacement) ## ...
            return subst(is1, fp, ap, hs, cll_append(os, replacement));
    }

    // FP
    if (is_fp_index) {
        CppToken *replacement = ap[is_fp_index - 1];

        CppToken *expanded = expand(replacement);
        if (expanded) expanded->whitespace = is->whitespace;

        if (expanded) os = cll_append(os, expanded);
        return subst(is->next, fp, ap, hs, os);
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

static char *get_current_file_path() {
    char *p = strrchr(state.filename, '/');
    if (!p) return strdup("./");
    char *path = malloc(p - state.filename + 2);
    memcpy(path, state.filename, p - state.filename + 1);
    path[p - state.filename + 1] = 0;

    return path;
}

static int try_and_open_include_file(char *full_path, char *path) {
    FILE *file = fopen(full_path, "r" );
    if (file) {
        init_cpp_from_fh(file, full_path, path);
        return 1;
    }

    return 0;
}

// Locate a filename in the search paths open it.
static int open_include_file(char *path, int is_system_include) {
    if (path[0] == '/')
        // Absolute path
        return try_and_open_include_file(path, path);

    // Relative path
    if (!is_system_include) {
        char *full_path;
        wasprintf(&full_path, "%s%s", get_current_file_path(), path);
        if (try_and_open_include_file(full_path, path)) return 1;
    }

    for (CliIncludePath *cip = cli_include_paths; cip; cip = cip->next) {
        char *full_path;
        wasprintf(&full_path, "%s/%s", cip->path, path);
        if (try_and_open_include_file(full_path, path)) return 1;
    }

    char *include_path;
    for (int i = 1; (include_path = BUILTIN_INCLUDE_PATHS[i]); i++) {
        char *full_path;
        wasprintf(&full_path, "%s%s", include_path, path);
        if (try_and_open_include_file(full_path, path)) return 1;
    }

    return 0;
}

static void skip_until_eol() {
    while (state.token->kind != CPP_TOK_EOF && state.token->kind != CPP_TOK_EOL) {
        cpp_next();
    }
}

static CppToken *gather_tokens_until_eol() {
    CppToken *tokens = 0;
    while (state.token->kind != CPP_TOK_EOF && state.token->kind != CPP_TOK_EOL) {
        tokens = cll_append(tokens, state.token);
        cpp_next();
    }

    return convert_cll_to_ll(tokens);
}

static CppToken *gather_tokens_until_eol_and_expand() {
    return expand(gather_tokens_until_eol());
}

static void parse_include() {
    if (include_depth == MAX_CPP_INCLUDE_DEPTH)
        panic("Exceeded maximum include depth %d", MAX_CPP_INCLUDE_DEPTH);

    char *path;
    int is_system;

    int have_pp_tokens = (state.token->kind != CPP_TOK_STRING_LITERAL && state.token->kind != CPP_TOK_HCHAR_STRING_LITERAL);
    CppToken *include_token;

    if (have_pp_tokens)
        // Parse #include pp-tokens
        include_token = gather_tokens_until_eol_and_expand();

    else {
        include_token = state.token;
        cpp_next();
    }

    if (!include_token || include_token->kind == CPP_TOK_EOL || include_token->kind == CPP_TOK_EOF)
        panic("Invalid #include");

    // Remove "" or <> tokens
    path = &(include_token->str[1]);
    path[strlen(path) - 1] = 0;
    is_system = include_token->kind == CPP_TOK_HCHAR_STRING_LITERAL;

    skip_until_eol();

    // Backup current parsing state
    CppState backup_state = state;

    if (!open_include_file(path, is_system)) panic("Unable to find %s in any include paths", path);

    include_depth++;
    run_preprocessor_on_file(path, 0);
    include_depth--;

    // Restore parsing state
    state = backup_state;

    char *buf = malloc(256);
    char *filename = state.override_filename ? state.override_filename : state.filename;
    sprintf(buf, "# %d \"%s\" 2", state.line_number_offset + state.line_number, filename);
    append_to_string_buffer(output, buf);
    free(buf);
}

static CppToken *parse_define_replacement_tokens(void) {
    if (state.token->kind == CPP_TOK_EOL || state.token->kind == CPP_TOK_EOF)
        return 0;

    if (state.token->kind == CPP_TOK_PASTE) panic("## at start of macro replacement list");

    CppToken *result = state.token;
    CppToken *tokens = state.token;
    while (state.token->kind != CPP_TOK_EOL && state.token->kind != CPP_TOK_EOF) {
        tokens->next = state.token;
        tokens = tokens->next;
        cpp_next();
    }
    tokens->next = 0;
    if (tokens->kind == CPP_TOK_PASTE) panic("## at end of macro replacement list");

    // Clear whitespace on initial token
    if (tokens) result->whitespace = 0;

    return result;
}

static Directive *parse_define_tokens(void) {
    Directive *directive = malloc(sizeof(Directive));
    memset(directive, 0, sizeof(Directive));

    CppToken *tokens;

    if (state.token->kind == CPP_TOK_EOL || state.token->kind == CPP_TOK_EOF) {
        // No tokens
        directive->is_function = 0;
        tokens = 0;
    }
    else {
        if (state.token->kind != CPP_TOK_LPAREN || (state.token->kind == CPP_TOK_LPAREN && state.token->whitespace != 0)) {
            // Parse object-like macro
            directive->is_function = 0;
            tokens = parse_define_replacement_tokens();
        }
        else {
            // Parse function-like macro
            cpp_next();

            directive->is_function = 1;
            directive->param_count = 0;
            directive->param_identifiers = new_strmap();
            int first = 1;

            while (state.token->kind != CPP_TOK_RPAREN && state.token->kind != CPP_TOK_EOF) {
                if (!first) {
                    if (state.token->kind != CPP_TOK_COMMA) panic("Expected comma");
                    cpp_next();
                }

                first = 0;

                if (state.token->kind != CPP_TOK_IDENTIFIER) panic("Expected identifier");
                if (directive->param_count == MAX_CPP_MACRO_PARAM_COUNT) panic("Exceeded max CPP function macro param count");
                if (strmap_get(directive->param_identifiers, state.token->str)) panic("Duplicate macro parameter %s", state.token->str);
                strmap_put(directive->param_identifiers, state.token->str, (void *) (long) ++directive->param_count);
                cpp_next();
            }

            if (state.token->kind != CPP_TOK_RPAREN) panic("Expected )");
            cpp_next();

            tokens = parse_define_replacement_tokens();
        }
    }

    directive->tokens = tokens;

    return directive;
}

// Enter group after if, ifdef or ifndef
static void enter_if() {
    ConditionalInclude *ci = malloc(sizeof(ConditionalInclude));
    memset(ci, 0, sizeof(ConditionalInclude));
    ci->prev = conditional_include_stack;
    ci->skipping = conditional_include_stack->skipping;
    conditional_include_stack = ci;
}

// Make a numeric token with a 0 or 1 value
static CppToken *make_boolean_token(int value) {
    CppToken *result = new_cpp_token(CPP_TOK_NUMBER);

    result->str = malloc(2);
    result->str[0] = '0' + value;
    result->str[1] = 0;

    return result;
}

// Expand defined FOO and defined(FOO) a token sequence. Returns a circular linked list.
static CppToken *expand_defineds(CppToken *is) {
    CppToken *os = 0;

    while (is) {
        CppToken *is1 = is->next;

        if (is->kind == CPP_TOK_DEFINED) {
            if (is1 && is1->kind == CPP_TOK_IDENTIFIER) {
                os = cll_append(os, make_boolean_token(!!strmap_get(directives, is1->str)));
                is = is1->next;
            }
            else if (is1 && is1->kind == CPP_TOK_LPAREN) {
                is = is1->next;
                if (!is || is->kind != CPP_TOK_IDENTIFIER)
                    panic("Expected identifier");

                os = cll_append(os, make_boolean_token(!!strmap_get(directives, is->str)));
                if (!is->next || is->next->kind != CPP_TOK_RPAREN)
                    panic("Expected )");
                is = is->next->next;
            }
            else
                panic("Expected identifier or ( after #defined");

        }
        else {
            CppToken *tok = dup_cpp_token(is);
            tok->next = 0;
            os = cll_append(os, tok);
            is = is->next;
        }
    }

    return os;
}

// Parse expression
// 1. Gather tokens until EOF/EOL.
// 2. Perform macro substitution.
// 3. Replace all identifiers with zero.
// 4. Render tokens into a string
// 5. Evaluate the string as an integral constant expression
static long parse_conditional_expression() {
    CppToken *tokens = 0;
    while (state.token->kind != CPP_TOK_EOF && state.token->kind != CPP_TOK_EOL) {
        tokens = cll_append(tokens, state.token);
        cpp_next();
    }
    if (!tokens) panic("Expected expresison");

    CppToken *expanded_defineds = expand_defineds(convert_cll_to_ll(tokens));
    CppToken *expanded = expand(convert_cll_to_ll(expanded_defineds));

    // Replace identifiers with 0
    for (CppToken *tok = expanded; tok; tok = tok->next)
        if (is_identifier(tok))  {
            tok->kind = CPP_TOK_NUMBER;
            tok->str = "0";
        }

    StringBuffer *rendered = new_string_buffer(128);
    append_tokens_to_string_buffer(rendered, expanded, 1);

    init_lexer_from_string(rendered->data);
    Value *value = parse_constant_integer_expression(1);

    return value->int_value;
}

static void parse_if_defined(int negate) {
    enter_if();

    if (!conditional_include_stack->skipping) {
        if (state.token->kind != CPP_TOK_IDENTIFIER) panic("Expected identifier");

        Directive *directive = strmap_get(directives, state.token->str);
        int value = negate ? !directive : !!directive;
        conditional_include_stack->skipping = !value;
        conditional_include_stack->matched = !!value;
    }
}

static void parse_line() {
    CppToken *tokens;

    if (state.token->kind != CPP_TOK_NUMBER)
        // Parse #line pp-tokens
        tokens = gather_tokens_until_eol_and_expand();
    else
        // Parse #line <number> ...
        tokens = gather_tokens_until_eol();

    if (tokens->kind == CPP_TOK_NUMBER) {
        state.line_number_offset = atoi(tokens->str) - state.line_number;
        tokens = tokens->next;

        if (tokens && tokens->kind == CPP_TOK_STRING_LITERAL) {
            char *filename = tokens->str;
            filename[strlen(filename) - 1] = 0;
            filename++;
            state.override_filename = filename;
            cur_filename = tokens->str;
        }

        // skip_until_eol();
        output_line_directive(0, 0);
    }
    else
        panic("Invalid #line directive");
}

// Ensure two directive have identical replacement lists, and if they are
// function-like, have identical paramter identifiers.
static void check_directive_redeclaration(Directive *d1, Directive *d2, char *identifier) {
    if (d1->is_function != d2->is_function)
        panic("Redeclared macro type mismatch for %s", identifier);

    if (d1->is_function) {
        // Compare parameter identifiers

        StrMap *d1p = d1->param_identifiers;
        StrMap *d2p = d2->param_identifiers;

        int d1_param_count = 0;
        for (StrMapIterator it = strmap_iterator(d1p); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
            d1_param_count++;
            char *param_identifier = strmap_iterator_key(&it);
            int position = (int) (long) strmap_get(d1p, param_identifier);
            if (position != (int) (long) strmap_get(d2p, param_identifier))
                panic("Redeclared directive mismatch for %s, parameters differ for %s", identifier, param_identifier);
        }

        int d2_param_count = 0;
        for (StrMapIterator it = strmap_iterator(d2p); !strmap_iterator_finished(&it); strmap_iterator_next(&it))
            d2_param_count++;

        if (d1_param_count != d2_param_count) panic("Param count mismatch for redeclared macro %s", identifier);
    }

    // Check replacement lists are identical
    CppToken *t1 = d1->tokens;
    CppToken *t2 = d2->tokens;

    int identical = 1;
    while (t1 && t2) {
        if (t1->kind != t2->kind) {
            identical = 0;
            break;
        }

        if (strcmp(t1->str, t2->str)) {
            identical = 0;
            break;
        }

        t1 = t1->next, t2 = t2->next;
    }


    if (!!t1 != !!t2) identical = 0;
    if (!identical) warning("Macro %s redefined", identifier);
}

static void parse_directive(void) {
    CppToken *directive_hash_token = state.token;
    cpp_next();

    switch (state.token->kind) {
        case CPP_TOK_INCLUDE:
            cpp_next();

            if (!conditional_include_stack->skipping)
                parse_include();

            skip_until_eol();
            break;

        case CPP_TOK_DEFINE:
            cpp_next();

            if (!conditional_include_stack->skipping) {
                if (state.token->kind != CPP_TOK_IDENTIFIER) panic("Expected identifier");

                char *identifier = state.token->str;
                cpp_next();

                Directive *existing_directive = strmap_get(directives, identifier);
                Directive *directive = parse_define_tokens();
                if (existing_directive) check_directive_redeclaration(existing_directive, directive, identifier);
                strmap_put(directives, identifier, directive);
            }
            else
                skip_until_eol();

            break;

        case CPP_TOK_UNDEF:
            cpp_next();

            if (!conditional_include_stack->skipping) {
                if (state.token->kind != CPP_TOK_IDENTIFIER) panic("Expected identifier");
                strmap_delete(directives, state.token->str);
                cpp_next();

                skip_until_eol();
            }

            break;

        case CPP_TOK_IFDEF:
        case CPP_TOK_IFNDEF: {
            int negate = (state.token->kind == CPP_TOK_IFNDEF);
            cpp_next();
            parse_if_defined(negate);

            skip_until_eol();
            break;
        }

        case CPP_TOK_IF:
            cpp_next();

            enter_if();
            if (!conditional_include_stack->skipping) {
                long value = parse_conditional_expression();
                conditional_include_stack->skipping = !value;
                conditional_include_stack->matched = !!value;
            }

            skip_until_eol();
            break;

        case CPP_TOK_ELIF: {
            cpp_next();

            // We're skipping a section and found an #elif. Ignore the directive
            if (!conditional_include_stack->prev->skipping) {
                if (conditional_include_stack->matched) {
                    conditional_include_stack->skipping = 1;
                    break;
                }

                long value = parse_conditional_expression();
                conditional_include_stack->skipping = !value;
                conditional_include_stack->matched = !!value;
            }

            skip_until_eol();
            break;
        }

        case CPP_TOK_ELSE:
            cpp_next();

            if (!conditional_include_stack->prev) panic("Found an #else without an #if");
            if (conditional_include_stack->prev->skipping) break;

            conditional_include_stack->skipping = conditional_include_stack->matched;

            skip_until_eol();
            break;

        case CPP_TOK_ENDIF: {
            cpp_next();

            if (!conditional_include_stack->prev) panic("Found an #endif without an #if");

            ConditionalInclude *prev = conditional_include_stack->prev;
            free(conditional_include_stack);
            conditional_include_stack = prev;

            skip_until_eol();
            break;
        }

        case CPP_TOK_LINE:
            cpp_next();

            if (!conditional_include_stack->skipping) parse_line();

            skip_until_eol();
            break;

        case CPP_TOK_NUMBER: {
            // # <number> ...
            // We're probably re-preprocessing everything, keep the line

            CppToken *tokens = 0;
            directive_hash_token->next = 0;
            tokens = cll_append(tokens, directive_hash_token);

            while (state.token->kind != CPP_TOK_EOF && state.token->kind != CPP_TOK_EOL) {
                tokens = cll_append(tokens, state.token);
                cpp_next();
            }

            append_tokens_to_output(expand(convert_cll_to_ll(tokens)));

            break;
        }

        default:
            panic("Unknown directive \"%s\"", state.token->str);
    }
}

// Parse tokens from the lexer
static void cpp_parse() {
    state.output_line_number = 1;
    int new_line = 1;
    CppToken *group_tokens = 0;

    while (state.token->kind != CPP_TOK_EOF) {
        while (state.token->kind != CPP_TOK_EOF) {
            if (new_line && state.token->kind == CPP_TOK_HASH) {
                append_tokens_to_output(expand(convert_cll_to_ll(group_tokens)));
                parse_directive();
                group_tokens = 0;
            }
            else {
                if (!conditional_include_stack->skipping) {
                    group_tokens = cll_append(group_tokens, state.token);
                    new_line = (state.token->kind == CPP_TOK_EOL);
                }
                cpp_next();
            }
        }
    }

    if (group_tokens) append_tokens_to_output(expand(convert_cll_to_ll(group_tokens)));
}

Directive *parse_cli_define(char *string) {
    init_cpp_from_string(string);
    cpp_next();
    return parse_define_tokens();
}

// Entrypoint for the preprocessor. This handles a top level file. It prepares the
// output, runs the preprocessor, then prints the output to a file handle.
void preprocess(char *filename, char *output_filename) {
    include_depth = 0;

    FILE *f = fopen(filename, "r");

    if (f == 0) {
        perror(filename);
        exit(1);
    }

    init_cpp_from_fh(f, filename, filename);

    output = new_string_buffer(state.input_size * 2);

    run_preprocessor_on_file(filename, 1);

    // Print the output
    terminate_string_buffer(output);

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

    fprintf(cpp_output_file, "%s", output->data);
    fclose(cpp_output_file);
}
