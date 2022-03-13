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
    char *               input;
    int                  input_size;
    int                  ip;                          // Offset in input
    char *               filename;                    // Current filename
    char *               override_filename;           // Overridden filename with #line
    LineMap *            line_map_start;              // Start of Linemap
    LineMap *            line_map;                    // Current position in linemap
    int                  line_number;                 // Current line number
    int                  line_number_offset;          // Difference in line number set by #line directive
    CppToken *           token;                       // Current token
    int                  hchar_lex_state;             // State of #include <...> lexing
    int                  output_line_number;          // How many lines have been outputted
    ConditionalInclude * conditional_include_stack;   // Includes
    int                  include_depth;               // How deep are we in nested includes
} CppState;

CppState state;

List *allocated_tokens;            // Keep track of all wmalloc'd tokens
List *allocated_tokens_duplicates; // Keep track of all wmalloc'd shallow copied tokens
List *allocated_strsets;
List *allocated_strings;

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

void debug_print_cll_token_sequence(CppToken *ts) {
    if (!ts) {
        printf("|\n");
        return;
    }

    CppToken *head = ts->next;
    ts = head;

    int c = 0;

    printf("|");
    do {
        if (ts->whitespace) printf("[%s]", ts->whitespace);

        if (strcmp("\n", ts->str))
            printf("%s|", ts->str);
        else
            printf("\\n");

        ts = ts->next;
        if (c == 1024) panic("Exceeded reasonable debug limit");
    } while (ts != head);
    printf("\n");
}

// Set cur_filename and cur_line globals from CPP state
void get_cpp_filename_and_line() {
    cur_line = state.line_number_offset + state.line_number;
    cur_filename = state.filename;
}

static void init_cpp_from_fh(FILE *f, char *path) {
    fseek(f, 0, SEEK_END);
    state.input_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    state.input = wmalloc(state.input_size + 1);
    int read = fread(state.input, 1, state.input_size, f);
    if (read != state.input_size) {
        printf("Unable to read input file\n");
        exit(1);
    }

    fclose(f);

    state.input[state.input_size] = 0;
    state.filename = wstrdup(path);
    cur_filename = state.filename;
    cur_line = 1;
    state.hchar_lex_state = HLS_START_OF_LINE;

    state.conditional_include_stack = wcalloc(1, sizeof(ConditionalInclude));
}

static void output_line_directive(int offset, int add_eol, CppToken *token) {
    if (!token) return;

    char *buf = wmalloc(256);
    char *filename = state.override_filename ? state.override_filename : state.filename;
    sprintf(buf, "# %d \"%s\"%s", state.line_number_offset + token->line_number + offset, filename, add_eol ? "\n" : "");
    append_to_string_buffer(output, buf);
    wfree(buf);
}

// If the output has an amount newlines > threshold, collapse them into a # line statement
static void collapse_trailing_newlines(int threshold, int output_line, CppToken *token) {
    int count = 0;
    while (output->position - count > 0 && output->data[output->position - count - 1] == '\n') count++;

    if (count > threshold) {
        // Rewind output by count -1 characters
        output->data[output->position - count + 1] = '\n';
        output->position -= count - 1;

        if (output_line) output_line_directive(0, 1, token);
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
    state.include_depth = 0;

    cpp_next();

    append_to_string_buffer(output, "# 1 \"");
    append_to_string_buffer(output, filename);
    append_to_string_buffer(output, "\"");
    if (!first_file) append_to_string_buffer(output, " 1");
    append_to_string_buffer(output, "\n");

    cpp_parse();

    collapse_trailing_newlines(0, 0, 0);

    wfree(state.input);
    wfree(state.filename);

    LineMap *lm = state.line_map_start;
    while (lm) {
        LineMap *next = lm->next;
        wfree(lm);
        lm = next;
    } while (lm);
}

void init_cpp_from_string(char *string) {
    state.input = wstrdup(string);
    state.input_size = strlen(string);
    state.filename = 0;

    state.ip = 0;
    state.line_map = 0;
    state.line_number = 1;
}

// Shallow copy a new CPP token
static CppToken *dup_cpp_token(CppToken *tok) {
    CppToken *result = wmalloc(sizeof(CppToken));
    *result = *tok;
    result->next = result;
    append_to_list(allocated_tokens_duplicates, result);
    return result;
}

// Append a single token to a circular linked list. token is modified.
static CppToken *cll_append_token(CppToken *list, CppToken *token) {
    if (!list) {
        // Convert token into a CLL and return it
        token->next = token;
        return token;
    }

    token->next = list->next;
    list->next = token;
    return token;
}

// Concatenate two circular linked lists
static CppToken *concat_clls(CppToken *list1, CppToken *list2) {
    if (!list1) return list2;
    if (!list2) return list1;

    CppToken *head = list1->next;
    list1->next = list2->next;
    list2->next = head;

    return list2;
}

// Return the next token in the circular linked list, or zero if at the end
#define cll_next(cll, token) ((cll == token) ? 0 : token->next)

// Make a new cll from token->next, or zero if token is at the end
// Beware, this alters the head of cll.
#define cll_from_next(cll, token) (cll == token ? 0 : (cll->next = token->next, cll))

// Duplicate a circular linked list of tokens, making a shallow copy of each token
static CppToken *dup_cll(CppToken *src) {
    if (!src) return 0;

    CppToken *dst = 0;
    for (CppToken *src_token = src->next; src_token; src_token = cll_next(src, src_token))
        dst = cll_append_token(dst, dup_cpp_token(src_token));

    return dst;
}

// Free a CPP token
static void free_cpp_token(CppToken *token) {
    if (token->str) wfree(token->str);
    if (token->whitespace) wfree(token->whitespace);
    wfree(token);
}

// Create a new CPP token
static CppToken *new_cpp_token(int kind) {
    CppToken *tok = wcalloc(1, sizeof(CppToken));
    tok->next = tok;

    append_to_list(allocated_tokens, tok);

    tok->kind = kind;

    return tok;
}

static void add_builtin_directive(char *identifier, DirectiveRenderer renderer) {
    Directive *directive = wcalloc(1, sizeof(Directive));
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

Directive *make_numeric_directive(int value) {
    Directive *directive = wcalloc(1, sizeof(Directive));
    directive->tokens = render_numeric_token(value);
    return directive;
}

Directive *make_empty_directive(void) {
    Directive *directive = wcalloc(1, sizeof(Directive));
    return directive;
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

    strmap_put(directives, "__STDC__", make_numeric_directive(1));
    strmap_put(directives, "__x86_64__", make_numeric_directive(1));
    strmap_put(directives, "__LP64__", make_numeric_directive(1));
    strmap_put(directives, "__linux__", make_numeric_directive(1));
    strmap_put(directives, "__GNUC__", make_numeric_directive(2));
    strmap_put(directives, "__USER_LABEL_PREFIX__", make_empty_directive());
}

void free_directive(Directive *d) {
    if (d) {
        if (d->param_identifiers) free_strmap(d->param_identifiers);
        wfree(d);
    }
}

void free_directives(void) {
    for (StrMapIterator it = strmap_iterator(directives); !strmap_iterator_finished(&it); strmap_iterator_next(&it)) {
        char *key = strmap_iterator_key(&it);
        Directive *d = (Directive *) strmap_get(directives, key);
        free_directive(d);
    }
    free_strmap(directives);
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
        return lm;
    }

    lm->next = wmalloc(sizeof(LineMap));
    lm->next->position = position;
    lm->next->line_number = line_number;
    lm->next->next = NULL;
    return lm->next;
}

void transform_trigraphs(void) {
    char *output = wmalloc(state.input_size + 1);
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

    wfree(state.input);

    output[op] = 0;
    state.input = output;
    state.input_size = op;
}

void strip_backslash_newlines(void) {
    // Add one for the last zero and another one for a potential trailing newline
    char *output = wmalloc(state.input_size + 2); //
    int ip = 0; // Input offset
    int op = 0; // Output offset
    int line_number = 1;

    state.line_map_start = wmalloc(sizeof(LineMap));
    state.line_map = state.line_map_start;
    state.line_map->position = ip;
    state.line_map->line_number = line_number;
    state.line_map->next = NULL;
    LineMap *lm = state.line_map;

    if (state.input_size == 0) {
        wfree(state.input);
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

    wfree(state.input);

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

static void append_tokens_to_string_buffer(StringBuffer *sb, CppToken *tokens, int collapse_whitespace, int update_output) {
    if (!tokens) return;

    CppToken *prev = 0;
    CppToken *head = tokens->next;
    CppToken *token = head;

    do {
        int is_eol = (token->kind == CPP_TOK_EOL);
        int seen_whitespace = is_eol;

        // If collapsing whitespace, pretend EOLs don't exist
        if (collapse_whitespace) {
            is_eol = 0;

            while (token && token->kind == CPP_TOK_EOL) {
                prev = token;
                token = cll_next(tokens, token);
            }
        }

        if (!token) return;

        if (!is_eol && token->whitespace && !collapse_whitespace)
            append_to_string_buffer(sb, token->whitespace);
        else if (!is_eol && token->whitespace && collapse_whitespace)
            append_to_string_buffer(sb, " ");
        else if (prev && need_token_space(prev, token))
            append_to_string_buffer(sb, " ");
        else if (seen_whitespace && collapse_whitespace)
            append_to_string_buffer(sb, " ");

        if (update_output && !is_eol) collapse_trailing_newlines(8, 1, token);
        append_to_string_buffer(sb, token->str);

        prev = token;
        token = token->next;

        if (update_output) {
            if (is_eol) state.output_line_number++;

            if (is_eol && token) {
                // Output sufficient newlines to catch up with token->line_number.
                // This ensures that each line in the input ends up on the same line
                // in the output.
                while (state.output_line_number < token->line_number) {
                    state.output_line_number++;
                    append_to_string_buffer(sb, "\n");
                }
            }
        }
    } while (token != head);
}

static void append_tokens_to_output(CppToken *tokens) {
    append_tokens_to_string_buffer(output, tokens, 0, 1);
}

#define advance_ip() do { \
    if (state.line_map && state.ip == state.line_map->position) { \
        state.line_number = state.line_map->line_number; \
        cur_line = state.line_number; \
        state.line_map = state.line_map->next; \
    } \
    state.ip++; \
} while(0)

static void advance_ip_by_count(int count) {
    for (int i = 0; i < count; i++)
        advance_ip();
}

static void add_to_whitespace(char **whitespace, int *whitespace_pos, char c) {
    if (!*whitespace) *whitespace = wmalloc(1024);
    if (*whitespace_pos == 1024) panic("Ran out of whitespace buffer");
    (*whitespace)[(*whitespace_pos)++] = c;
}

static char *lex_whitespace(void) {
    char *whitespace = 0;
    int whitespace_pos = 0;

    // Process whitespace and comments
    while (state.ip < state.input_size) {
        char c = state.input[state.ip];
        if (c == '\f' || c == '\v' || c == '\r') state.input[state.ip] = ' ';

        if ((state.input[state.ip] == '\t' || state.input[state.ip] == ' ')) {
            add_to_whitespace(&whitespace, &whitespace_pos, state.input[state.ip]);
            advance_ip();
            continue;
        }

        // Process // comment
        if (state.input_size - state.ip >= 2 && state.input[state.ip] == '/' && state.input[state.ip + 1] == '/') {
            while (state.input[state.ip] != '\n') advance_ip();
            add_to_whitespace(&whitespace, &whitespace_pos, ' ');
            continue;
        }

        // Process /* comments */
        if (state.input_size - state.ip >= 2 && state.input[state.ip] == '/' && state.input[state.ip + 1] == '*') {
            advance_ip();
            advance_ip();

            while (state.input_size - state.ip >= 2 && (state.input[state.ip] != '*' || state.input[state.ip + 1] != '/'))
                advance_ip();

            advance_ip();
            advance_ip();

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

    char *data = wmalloc(MAX_STRING_LITERAL_SIZE + 2);

    if (i[state.ip] == 'L') {
        advance_ip();
        data_offset = 1;
        data[0] = 'L';
    }

    advance_ip();

    data[data_offset] = delimiter == '>' ? '<' : delimiter;
    int size = 0;

    while (state.ip < state.input_size) {
        if (i[state.ip] == delimiter) {
            advance_ip();
            break;
        }

        if (state.input_size - state.ip >= 2 && i[state.ip] == '\\') {
            if (size + 1 >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
            data[data_offset + 1 + size++] = '\\';
            data[data_offset + 1 + size++] = i[state.ip + 1];
            advance_ip();
            advance_ip();
        }

        else {
            if (size >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
            data[data_offset + 1 + size++] = i[state.ip];
            advance_ip();
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
        advance_ip_by_count(size); \
    } while (0)

#define make_other_token(size) make_token(CPP_TOK_OTHER, size)

#define copy_token_str(start, size) \
    do { \
        state.token->str = wmalloc(size + 1); \
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
            state.token->str = wstrdup("\n");
            state.token->line_number = state.line_number; // Needs to be the line number of the \n token, not the next token
            state.hchar_lex_state = HLS_START_OF_LINE;
            advance_ip();
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
        else if (left >= 2 && c1 == '<' && c2 == '<'             )  { make_other_token(2); }
        else if (left >= 2 && c1 == '>' && c2 == '>'             )  { make_other_token(2); }
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
            char *identifier = wmalloc(1024);
            int j = 0;
            while (((i[state.ip] >= 'a' && i[state.ip] <= 'z') || (i[state.ip] >= 'A' && i[state.ip] <= 'Z') || (i[state.ip] >= '0' && i[state.ip] <= '9') || (i[state.ip] == '_')) && state.ip < state.input_size) {
                if (j == MAX_IDENTIFIER_SIZE) panic("Exceeded maximum identifier size %d", MAX_IDENTIFIER_SIZE);
                identifier[j] = i[state.ip];
                j++;
                advance_ip();
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
                t->kind == CPP_TOK_DEFINED    || \
                t->kind == CPP_TOK_WARNING    || \
                t->kind == CPP_TOK_ERROR      || \
                t->kind == CPP_TOK_PRAGMA)

            if      (!strcmp(identifier, "define"))   { state.token = new_cpp_token(CPP_TOK_DEFINE);  state.token->str = identifier; }
            else if (!strcmp(identifier, "include"))  { state.token = new_cpp_token(CPP_TOK_INCLUDE); state.token->str = identifier; }
            else if (!strcmp(identifier, "undef"))    { state.token = new_cpp_token(CPP_TOK_UNDEF);   state.token->str = identifier; }
            else if (!strcmp(identifier, "if"))       { state.token = new_cpp_token(CPP_TOK_IF);      state.token->str = identifier; }
            else if (!strcmp(identifier, "ifdef"))    { state.token = new_cpp_token(CPP_TOK_IFDEF);   state.token->str = identifier; }
            else if (!strcmp(identifier, "ifndef"))   { state.token = new_cpp_token(CPP_TOK_IFNDEF);  state.token->str = identifier; }
            else if (!strcmp(identifier, "elif"))     { state.token = new_cpp_token(CPP_TOK_ELIF);    state.token->str = identifier; }
            else if (!strcmp(identifier, "else"))     { state.token = new_cpp_token(CPP_TOK_ELSE);    state.token->str = identifier; }
            else if (!strcmp(identifier, "endif"))    { state.token = new_cpp_token(CPP_TOK_ENDIF);   state.token->str = identifier; }
            else if (!strcmp(identifier, "line"))     { state.token = new_cpp_token(CPP_TOK_LINE);    state.token->str = identifier; }
            else if (!strcmp(identifier, "defined"))  { state.token = new_cpp_token(CPP_TOK_DEFINED); state.token->str = identifier; }
            else if (!strcmp(identifier, "warning"))  { state.token = new_cpp_token(CPP_TOK_WARNING); state.token->str = identifier; }
            else if (!strcmp(identifier, "error"))    { state.token = new_cpp_token(CPP_TOK_ERROR);   state.token->str = identifier; }
            else if (!strcmp(identifier, "pragma"))   { state.token = new_cpp_token(CPP_TOK_PRAGMA);  state.token->str = identifier; }

            else {
                state.token = new_cpp_token(CPP_TOK_IDENTIFIER);
                state.token->str = identifier;
            }
        }

        else if (is_pp_number(c1, c2)) {
            int size = 1;
            char *start = &(i[state.ip]);
            advance_ip();

            while (c1 = i[state.ip], c2 = i[state.ip + 1], c1 == '.' || is_pp_number(c1, c2) || is_non_digit(c1) || is_exponent(c1, c2)) {
                if (is_exponent(c1, c2)) { advance_ip(); size++; }
                advance_ip();
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

// Set line number on all tokens in ts
static void set_line_number_on_token_sequence(CppToken *ts, int line_number) {
    CppToken *tail = ts;
    ts = ts->next;
    do {
        ts->line_number = line_number;
        ts = ts->next;
    } while (ts != tail);
}

#define add_token_to_actuals() current_actual = cll_append_token(current_actual, dup_cpp_token(token));

// Parse function-like macro call, starting with the '('
// Loop over ts; ts is advanced up to the enclosing ')'
// Returns a ptr to the token sequences for each actual parameter.
// Nested () are taken into account, for cases like f((1, 2), 3), which
// results in (1, 2) and 3 for the actual parameters.
CppToken **make_function_actual_parameters(CppToken **ts) {
    CppToken **result = wcalloc(MAX_CPP_MACRO_PARAM_COUNT, sizeof(CppToken *));

    int parenthesis_nesting_level = 0;
    CppToken *current_actual = 0;
    int index = 0;

    while (1) {
        CppToken *token = *ts;
        if (!token) error("Expected macro function-like parameter or )");

        if (token->kind == CPP_TOK_RPAREN && !parenthesis_nesting_level) {
            // We're not in nested parentheses
            result[index++] = current_actual;
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
            result[index++] = current_actual;
            current_actual = 0;
        }
        else {
            // It's an ordinary token, append it to current actual
            add_token_to_actuals();
        }

        *ts =(*ts)->next;
    }
}

void free_function_actual_parameters(CppToken **actuals) {
    wfree(actuals); // The tokens are freed as part of the garbage freeing
}

// Call strset_union and register the allocated strset for freeing later on.
StrSet *cpp_strset_union(StrSet *set1, StrSet *set2) {
    StrSet *s = strset_union(set1, set2);
    append_to_list(allocated_strsets, s);
    return s;
}

// Union two sets. Either or both may be NULL
#define safe_strset_union(set1, set2) \
    set1 && set2 \
        ? cpp_strset_union(set1, set2) \
        : set1 \
            ? set1 \
            : set2

// Call strset_intersection and register the allocated strset for freeing later on.
StrSet *cpp_strset_intersection(StrSet *set1, StrSet *set2) {
    StrSet *s = strset_intersection(set1, set2);
    append_to_list(allocated_strsets, s);
    return s;
}

// Intersect two sets. Either or both may be NULL
#define safe_strset_intersection(set1, set2) \
    set1 && set2 \
        ? cpp_strset_intersection(set1, set2) \
        : 0;

// Implementation of Dave Prosser's C Preprocessing Algorithm
// This is the main macro expansion function: expand an input sequence into an output sequence.
static CppToken *expand(CppToken *is) {
    if (!is) return 0;

    // Skip past any non-directives, to eliminate unnecessary recursion
    CppToken *left = 0;
    CppToken *is_head = is->next;
    CppToken *is_tail = is;
    CppToken *tok = is_head;
    while (tok->str && !strmap_get(directives, tok->str)) {
        left = cll_append_token(left, dup_cpp_token(tok));
        tok = tok->next;
        if (tok == is_head) break;
    }

    if (left) {
        if (tok == is_head) return left;

        // Make is the remainder
        is_tail->next = tok; // Make tok the head of the cll
        CppToken *result = concat_clls(left, expand(is));
        return result;
    }

    CppToken *tok1 = tok->next != is_head ? tok->next : 0;
    while (tok1 && tok1->kind == CPP_TOK_EOL) tok1 = cll_next(is, tok1);

    if (tok->str && tok->hide_set && strset_in(tok->hide_set, tok->str)) {
        // The first token tok in its own hide set, don't expand it
        // return the first token + the expanded rest
        if (is_tail == is_head) return is_tail; // Only one token
        CppToken *t = dup_cpp_token(tok);
        return concat_clls(t, expand(cll_from_next(is, tok)));
    }

    Directive *directive = 0;
    if (tok->str) directive = strmap_get(directives, tok->str);

    if (directive && !directive->is_function) {
        // Object like macro

        StrSet *identifier_hs = new_strset();
        append_to_list(allocated_strsets, identifier_hs);
        strset_add(identifier_hs, tok->str);
        StrSet *hs = tok->hide_set ? cpp_strset_union(tok->hide_set, identifier_hs) : identifier_hs;
        CppToken *replacement_tokens = directive->renderer ? directive->renderer(tok) : directive->tokens;
        CppToken *substituted = subst(replacement_tokens, 0, 0, hs, 0);

        if (substituted) {
            set_line_number_on_token_sequence(substituted, tok->line_number);
            substituted->next->whitespace = tok->whitespace;
        }

        return expand(concat_clls(substituted, cll_from_next(is, tok)));
    }

    else if (directive && directive->is_function && tok1 && tok1->kind == CPP_TOK_LPAREN) {
        // Function like macro

        CppToken *directive_token = tok;
        StrSet *directive_token_hs = tok->hide_set;
        tok = cll_next(is, tok1);
        CppToken **actuals = make_function_actual_parameters(&tok);
        if (!tok || tok->kind != CPP_TOK_RPAREN) error("Expected )");

        int actuals_count = 0;
        for (CppToken **tok = actuals; *tok; tok++, actuals_count++);
        if (actuals_count > directive->param_count) error("Mismatch in number of macro parameters");

        StrSet *rparen_hs = tok->hide_set;

        // Make the hideset for the macro subsitution
        // T is the directive token
        // HS is directive token hide set
        // HS' is ')' hide set
        // HS = (HS ∩ HS’) ∪ {T}
        StrSet *hs1 = safe_strset_intersection(directive_token_hs, rparen_hs);

        StrSet *hs2 = new_strset();
        strset_add(hs2, directive_token->str);
        if (hs2) append_to_list(allocated_strsets, hs2);

        StrSet *hs = safe_strset_union(hs1, hs2);

        CppToken *substituted = 0;
        if (directive->tokens)
            substituted = subst(dup_cll(directive->tokens), directive->param_identifiers, actuals, hs, 0);

        free_function_actual_parameters(actuals);

        if (substituted) {
            set_line_number_on_token_sequence(substituted, directive_token->line_number);
            substituted->next->whitespace = directive_token->whitespace;
        }

        return expand(concat_clls(substituted, cll_from_next(is, tok)));
    }

    // The first token is an ordinary token, return the first token + the expanded rest
    CppToken *t = dup_cpp_token(tok);
    return concat_clls(t, expand(cll_from_next(is, tok)));
}

// Returns a new token which is the stringized version of the input token sequence
static CppToken *stringize(CppToken *ts) {
    StringBuffer *rendered = new_string_buffer(128);

    append_tokens_to_string_buffer(rendered, ts, 1, 0);

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

    free_string_buffer(escaped, 0);
    free_string_buffer(rendered, 1);

    return result;
}

// Merge two token lists together.
// ls is mutated and the result is ls.
static CppToken *glue(CppToken *ls, CppToken *rs) {
    if (ls == 0) return rs;
    if (rs == 0) error("Attempt to glue an empty right side");

    // Mutating ls, this is allowed since ls is append only
    wasprintf(&ls->str, "%s%s", ls->str, rs->next->str);
    append_to_list(allocated_strings, ls->str);
    ls->kind = CPP_TOK_OTHER;
    ls->hide_set = safe_strset_intersection(ls->hide_set, rs->next->hide_set);

    // Copy all elements from the right side except the first
    CppToken *rs_tok = rs->next;
    rs_tok = cll_next(rs, rs_tok);
    while (rs_tok) {
        ls = cll_append_token(ls, dup_cpp_token(rs_tok));
        rs_tok = cll_next(rs, rs_tok);
    }

    return ls;
}

// Inputs:
//   is: input sequence
//   fp: formal parameters
//   ap: actual parameters
//   hs: hide set
//   os: output sequence
// Output
//   output sequence
static CppToken *subst(CppToken *is, StrMap *fp, CppToken **ap, StrSet *hs, CppToken *os) {
    // Empty token sequence, update the hide set and return output sequence
    if (!is) {
        if (!os) return os;
        return hsadd(hs, os);
    }

    CppToken *is_head = is->next;
    CppToken *is_tail = is;
    CppToken *tok = is_head;

    CppToken *tok1 = cll_next(is, tok);
    CppToken *tok2 = tok1 ? cll_next(is, tok1) : 0;

    int tok_fp_index = fp ? (int) (long) strmap_get(fp, tok->str) : 0;
    int tok1_fp_index = fp && tok1 ? (int) (long) strmap_get(fp, tok1->str) : 0;
    int tok2_fp_index = fp && tok2 ? (int) (long) strmap_get(fp, tok2->str) : 0;

    // # FP
    if (tok->kind == CPP_TOK_HASH && tok1_fp_index) {
        CppToken *replacement = ap[tok1_fp_index - 1];
        os = cll_append_token(os, stringize(replacement));

        return subst(cll_from_next(is, tok1), fp, ap, hs, os);
    }

    // ## FP
    if (tok->kind == CPP_TOK_PASTE && tok1_fp_index) {
        CppToken *replacement = ap[tok1_fp_index - 1];
        if (!replacement)
            return subst(cll_from_next(is, tok1), fp, ap, hs, os);
        else
            return subst(cll_from_next(is, tok1), fp, ap, hs, glue(os, dup_cll(replacement)));
    }

    // ## TS
    if (tok->kind == CPP_TOK_PASTE) {
        if (!tok1) error("Got ## without a following token");
        CppToken *tok = dup_cpp_token(tok1);
        return subst(cll_from_next(is, tok1), fp, ap, hs, glue(os, tok));
    }

    // FP ##
    if (tok_fp_index && tok1 && tok1->kind == CPP_TOK_PASTE) {
        CppToken *replacement = ap[tok_fp_index - 1];
        if (!replacement) {
            if (tok2 && tok2_fp_index) {
                // (empty) ## (replacement2) ...
                CppToken *replacement2 = ap[tok2_fp_index - 1];
                return subst(cll_from_next(is, tok2), fp, ap, hs, concat_clls(os, dup_cll(replacement2)));
            }
            else
                // (empty) ## (empty) ...
                return subst(cll_from_next(is, tok1), fp, ap, hs, os);
        }
        else
            // (replacement) ## ...
            return subst(cll_from_next(is, tok), fp, ap, hs, concat_clls(os, dup_cll(replacement)));
    }

    // FP
    if (tok_fp_index) {
        CppToken *replacement = ap[tok_fp_index - 1];

        CppToken *expanded = expand(dup_cll(replacement));
        if (expanded) expanded->next->whitespace = tok->whitespace;

        if (expanded) os = concat_clls(os, expanded);
        return subst(cll_from_next(is, tok), fp, ap, hs, os);
    }

    // Append first token in is to os
    os = cll_append_token(os, dup_cpp_token(tok));
    CppToken *org_head = is_tail->next;
    CppToken *result = subst(cll_from_next(is, tok), fp, ap, hs, os);
    is_tail->next = org_head;
    return result;
}

// Add a a hide set to a token sequence's hide set.
static CppToken *hsadd(StrSet *hs, CppToken *ts) {
    if (!hs) panic("Empty hs in hsadd");
    if (!ts) return 0;

    CppToken *result = 0;
    for (CppToken *tok = ts->next; tok; tok = cll_next(ts, tok)) {
        CppToken *new_token = dup_cpp_token(tok);

        if (new_token->hide_set)
            new_token->hide_set = cpp_strset_union(new_token->hide_set, hs);
        else {
            new_token->hide_set = dup_strset(hs);
            if (new_token->hide_set) append_to_list(allocated_strsets, new_token->hide_set);
        }

        result = cll_append_token(result, new_token);
    }

    return result;
}

static char *get_current_file_path() {
    char *p = strrchr(state.filename, '/');
    if (!p) return "";
    char *path = wmalloc(p - state.filename + 2);
    memcpy(path, state.filename, p - state.filename + 1);
    path[p - state.filename + 1] = 0;

    return path;
}

static int try_and_open_include_file(char *full_path, char *path) {
    FILE *file = fopen(full_path, "r" );
    if (file) {
        init_cpp_from_fh(file, full_path);
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
        int ok = try_and_open_include_file(full_path, path);
        wfree(full_path);
        if (ok) return 1;
    }

    for (CliIncludePath *cip = cli_include_paths; cip; cip = cip->next) {
        char *full_path;
        wasprintf(&full_path, "%s/%s", cip->path, path);
        int ok = try_and_open_include_file(full_path, path);
        wfree(full_path);
        if (ok) return 1;
    }

    char *include_path;
    for (int i = 0; (include_path = BUILTIN_INCLUDE_PATHS[i]); i++) {
        char *full_path;
        wasprintf(&full_path, "%s%s", include_path, path);
        int ok = try_and_open_include_file(full_path, path);
        wfree(full_path);
        if (ok) return 1;
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
        tokens = cll_append_token(tokens, state.token);
        cpp_next();
    }

    return tokens;
}

static CppToken *gather_tokens_until_eol_and_expand() {
    return expand(gather_tokens_until_eol());
}

static void parse_include() {
    if (state.include_depth == MAX_CPP_INCLUDE_DEPTH)
        error("Exceeded maximum include depth %d", MAX_CPP_INCLUDE_DEPTH);

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
        error("Invalid #include");

    // Remove "" or <> tokens
    path = &(include_token->str[1]);
    path[strlen(path) - 1] = 0;
    is_system = include_token->kind == CPP_TOK_HCHAR_STRING_LITERAL;

    skip_until_eol();
    collapse_trailing_newlines(0, 0, 0);

    // Backup current parsing state
    CppState backup_state = state;

    if (!open_include_file(path, is_system)) error("Unable to find %s in any include paths", path);

    state.include_depth++;
    run_preprocessor_on_file(state.filename, 0);
    state.include_depth--;

    wfree(state.conditional_include_stack);

    // Restore parsing state
    state = backup_state;
    cur_filename = state.filename;

    char *buf = wmalloc(256);
    char *filename = state.override_filename ? state.override_filename : state.filename;
    sprintf(buf, "# %d \"%s\" 2", state.line_number_offset + state.line_number + 1, filename);
    append_to_string_buffer(output, buf);
    wfree(buf);
}

static CppToken *parse_define_replacement_tokens(void) {
    if (state.token->kind == CPP_TOK_EOL || state.token->kind == CPP_TOK_EOF)
        return 0;

    if (state.token->kind == CPP_TOK_PASTE) error("## at start of macro replacement list");

    CppToken *result = 0;
    result = cll_append_token(result, state.token);

    while (state.token->kind != CPP_TOK_EOL && state.token->kind != CPP_TOK_EOF) {
        result = cll_append_token(result, state.token);
        cpp_next();
    }
    if (result->kind == CPP_TOK_PASTE) error("## at end of macro replacement list");

    // Clear whitespace on initial token
    wfree(result->next->whitespace);
    result->next->whitespace = NULL;

    return result;
}

static Directive *parse_define_tokens(void) {
    Directive *directive = wcalloc(1, sizeof(Directive));

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
                    if (state.token->kind != CPP_TOK_COMMA) error("Expected comma");
                    cpp_next();
                }

                first = 0;

                if (!is_identifier(state.token)) error("Expected identifier");
                if (directive->param_count == MAX_CPP_MACRO_PARAM_COUNT) panic("Exceeded max CPP function macro param count");
                if (strmap_get(directive->param_identifiers, state.token->str)) error("Duplicate macro parameter %s", state.token->str);
                strmap_put(directive->param_identifiers, state.token->str, (void *) (long) ++directive->param_count);
                cpp_next();
            }

            if (state.token->kind != CPP_TOK_RPAREN) error("Expected )");
            cpp_next();

            tokens = parse_define_replacement_tokens();
        }
    }

    directive->tokens = tokens;

    return directive;
}

// Enter group after if, ifdef or ifndef
static void enter_if() {
    ConditionalInclude *ci = wcalloc(1, sizeof(ConditionalInclude));
    ci->prev = state.conditional_include_stack;
    ci->skipping = state.conditional_include_stack->skipping;
    state.conditional_include_stack = ci;
}

// Make a numeric token with a 0 or 1 value
static CppToken *make_boolean_token(int value) {
    CppToken *result = new_cpp_token(CPP_TOK_NUMBER);

    result->str = wmalloc(2);
    result->str[0] = '0' + value;
    result->str[1] = 0;

    return result;
}

// Expand defined FOO and defined(FOO) in a token sequence.
static CppToken *expand_defineds(CppToken *is) {
    CppToken *os = 0;

    CppToken *tok = is->next;

    while (tok) {
        CppToken *tok1 = cll_next(is, tok);

        if (tok->kind == CPP_TOK_DEFINED) {
            if (tok1 && tok1->kind == CPP_TOK_IDENTIFIER) {
                os = cll_append_token(os, make_boolean_token(!!strmap_get(directives, tok1->str)));
                tok = cll_next(is, tok1);
            }
            else if (tok1 && tok1->kind == CPP_TOK_LPAREN) {
                tok = cll_next(is, tok1);
                if (!tok || tok->kind != CPP_TOK_IDENTIFIER)
                    error("Expected identifier");

                os = cll_append_token(os, make_boolean_token(!!strmap_get(directives, tok->str)));
                if (tok->next == is->next || tok->next->kind != CPP_TOK_RPAREN)
                    error("Expected )");
                tok = cll_next(is, cll_next(is, tok));
            }
            else
                error("Expected identifier or ( after #defined");

        }
        else {
            os = cll_append_token(os, dup_cpp_token(tok));
            tok = cll_next(is, tok);
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
        tokens = cll_append_token(tokens, state.token);
        cpp_next();
    }
    if (!tokens) error("Expected expression");

    CppToken *expanded_defineds = expand_defineds(tokens);
    CppToken *expanded = expand(expanded_defineds);

    // Replace identifiers with 0
    for (CppToken *tok = expanded->next; tok; tok = cll_next(expanded, tok))
        if (is_identifier(tok))  {
            tok->kind = CPP_TOK_NUMBER;
            tok->str = "0";
        }

    StringBuffer *rendered = new_string_buffer(128);
    append_tokens_to_string_buffer(rendered, expanded, 1, 0);

    char *data = rendered->data;
    free_string_buffer(rendered, 0);
    init_lexer_from_string(data);
    Value *value = parse_constant_integer_expression(1);
    wfree(data);
    free_lexer();

    return value->int_value;
}

static void parse_if_defined(int negate) {
    enter_if();

    if (!state.conditional_include_stack->skipping) {
        if (!is_identifier(state.token)) error("Expected identifier");

        Directive *directive = strmap_get(directives, state.token->str);
        int value = negate ? !directive : !!directive;
        state.conditional_include_stack->skipping = !value;
        state.conditional_include_stack->matched = !!value;
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

    CppToken *token = tokens->next;

    if (state.token->kind != CPP_TOK_EOL && state.token->kind != CPP_TOK_EOF) panic("Expected EOL or EOF");

    if (token->kind == CPP_TOK_NUMBER) {
        state.line_number_offset = atoi(token->str) - state.line_number - 1;

        // Amend the next token's line number offset too
        state.token->line_number_offset = state.line_number_offset;

        token = cll_next(tokens, token);

        if (token && token->kind == CPP_TOK_STRING_LITERAL) {
            char *filename = token->str;
            filename[strlen(filename) - 1] = 0;
            filename++;
            state.override_filename = filename;
            cur_filename = token->str;
        }

        output_line_directive(1, 0, state.token);
    }
    else
        error("Invalid #line directive");
}

// Ensure two directive have identical replacement lists, and if they are
// function-like, have identical paramter identifiers.
static void check_directive_redeclaration(Directive *d1, Directive *d2, char *identifier) {
    if (d1->is_function != d2->is_function)
        error("Redeclared macro type mismatch for %s", identifier);

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
                error("Redeclared directive mismatch for %s, parameters differ for %s", identifier, param_identifier);
        }

        int d2_param_count = 0;
        for (StrMapIterator it = strmap_iterator(d2p); !strmap_iterator_finished(&it); strmap_iterator_next(&it))
            d2_param_count++;

        if (d1_param_count != d2_param_count) error("Param count mismatch for redeclared macro %s", identifier);
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

        t1 = cll_next(d1->tokens, t1);
        t2 = cll_next(d2->tokens, t2);
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

            if (!state.conditional_include_stack->skipping)
                parse_include();

            skip_until_eol();
            break;

        case CPP_TOK_DEFINE:
            cpp_next();

            if (!state.conditional_include_stack->skipping) {
                if (!is_identifier(state.token)) error("Expected identifier");

                char *identifier = state.token->str;
                cpp_next();

                Directive *existing_directive = strmap_get(directives, identifier);
                Directive *directive = parse_define_tokens();
                if (existing_directive) {
                    check_directive_redeclaration(existing_directive, directive, identifier);
                    free_directive(existing_directive);
                }
                strmap_put(directives, identifier, directive);
            }
            else
                skip_until_eol();

            break;

        case CPP_TOK_UNDEF:
            cpp_next();

            if (!state.conditional_include_stack->skipping) {
                if (!is_identifier(state.token)) error("Expected identifier");
                Directive *existing_directive = strmap_get(directives, state.token->str);
                if (existing_directive) free_directive(existing_directive);
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
            if (!state.conditional_include_stack->skipping) {
                long value = parse_conditional_expression();
                state.conditional_include_stack->skipping = !value;
                state.conditional_include_stack->matched = !!value;
            }

            skip_until_eol();
            break;

        case CPP_TOK_ELIF: {
            cpp_next();

            // We're skipping a section and found an #elif. Ignore the directive
            if (!state.conditional_include_stack->prev->skipping) {
                if (state.conditional_include_stack->matched) {
                    state.conditional_include_stack->skipping = 1;
                    break;
                }

                long value = parse_conditional_expression();
                state.conditional_include_stack->skipping = !value;
                state.conditional_include_stack->matched = !!value;
            }

            skip_until_eol();
            break;
        }

        case CPP_TOK_ELSE:
            cpp_next();

            if (!state.conditional_include_stack->prev) error("Found an #else without an #if");
            if (state.conditional_include_stack->prev->skipping) break;

            state.conditional_include_stack->skipping = state.conditional_include_stack->matched;

            skip_until_eol();
            break;

        case CPP_TOK_ENDIF: {
            cpp_next();

            if (!state.conditional_include_stack->prev) error("Found an #endif without an #if");

            ConditionalInclude *prev = state.conditional_include_stack->prev;
            wfree(state.conditional_include_stack);
            state.conditional_include_stack = prev;

            break;
        }

        case CPP_TOK_LINE:
            cpp_next();

            if (!state.conditional_include_stack->skipping) parse_line();

            break;

        case CPP_TOK_NUMBER: {
            // # <number> ...
            // We're probably re-preprocessing everything, keep the line

            CppToken *tokens = 0;
            tokens = cll_append_token(tokens, directive_hash_token);

            while (state.token->kind != CPP_TOK_EOF && state.token->kind != CPP_TOK_EOL) {
                tokens = cll_append_token(tokens, state.token);
                cpp_next();
            }

            append_tokens_to_output(expand(tokens));

            break;
        }

        case CPP_TOK_PRAGMA:
            // Ignore #pragma

            skip_until_eol();
            break;

        case CPP_TOK_WARNING:
            // #warning

            if (!state.conditional_include_stack->skipping) {
                StringBuffer *message = new_string_buffer(128);
                append_tokens_to_string_buffer(message, gather_tokens_until_eol(), 0, 0);
                warning("%s", message->data);
            }

            break;

        case CPP_TOK_ERROR:
            // #error

            if (!state.conditional_include_stack->skipping) {
                StringBuffer *message = new_string_buffer(128);
                append_tokens_to_string_buffer(message, gather_tokens_until_eol(), 0, 0);
                error("%s", message->data);
            }

            break;

        case CPP_TOK_EOL:
        case CPP_TOK_EOF:
            // null directive

            break;

        default:
            if (!state.conditional_include_stack->skipping)
                error("Unknown directive \"%s\"", state.token->str);
    }
}

// Parse tokens from the lexer
static void cpp_parse() {
    state.output_line_number = 1;
    int new_line = 1;
    CppToken *group_tokens = 0;

    while (state.token->kind != CPP_TOK_EOF) {
        if (new_line && state.token->kind == CPP_TOK_HASH) {
            append_tokens_to_output(expand(group_tokens));
            parse_directive();
            group_tokens = 0;
        }
        else {
            if (!state.conditional_include_stack->skipping) {
                group_tokens = cll_append_token(group_tokens, state.token);
                new_line = (state.token->kind == CPP_TOK_EOL);
            }
            cpp_next();
        }
    }

    if (group_tokens) append_tokens_to_output(expand(group_tokens));
}

void init_cpp(void) {
    allocated_tokens = new_list(1024);
    allocated_tokens_duplicates = new_list(1024);
    allocated_strsets = new_list(1024);
    allocated_strings = new_list(1024);
}

void free_cpp_allocated_garbage() {
    if (!allocated_tokens) return; // CPP has not been initted

    // Free allocated_tokens
    for (int i = 0; i < allocated_tokens->length; i++)
        free_cpp_token(allocated_tokens->elements[i]);
    free_list(allocated_tokens);

    for (int i = 0; i < allocated_tokens_duplicates->length; i++)
        wfree(allocated_tokens_duplicates->elements[i]);
    free_list(allocated_tokens_duplicates);

    // Free any allocated strsets
    for (int i = 0; i < allocated_strsets->length; i++)
        free_strset(allocated_strsets->elements[i]);
    free_list(allocated_strsets);

    // Free any allocated strings
    for (int i = 0; i < allocated_strings->length; i++)
        wfree(allocated_strings->elements[i]);
    free_list(allocated_strings);
}

Directive *parse_cli_define(char *string) {
    init_cpp_from_string(string);
    cpp_next();
    Directive *d = parse_define_tokens();
    wfree(state.input);
    return d;
}

static void parse_cli_directive_string(char *expr) {
    expr = wstrdup(expr);

    CliDirective *cli_directive = wmalloc(sizeof(CliDirective));
    cli_directive->next = 0;

    char *key;
    char *value;

    char *p = strchr(expr, '=');
    if (p) {
        if (p == expr) simple_error("Invalid directive");
        *p = 0;
        key = expr;
        value = p + 1;
    }
    else {
        key = expr;
        value = "1";
    }

    cli_directive->identifier = wstrdup(key);
    cli_directive->directive = parse_cli_define(value);

    if (!cli_directives) cli_directives = cli_directive;
    else {
        CliDirective *cd = cli_directives;
        while (cd->next) cd = cd->next;
        cd->next = cli_directive;
    }

    wfree(expr);
}

static void parse_cli_directive_strings(List *cli_directive_strings) {
    cli_directives = 0;

    for (int i = 0; i < cli_directive_strings->length; i++)
        parse_cli_directive_string(cli_directive_strings->elements[i]);
}

static void free_cli_directives(void) {
    if (!cli_directives) return;

    CliDirective *cd = cli_directives;
    while (cd) {
        CliDirective *next = cd->next;
        wfree(cd->identifier);
        wfree(cd);
        cd = next;
    }

}

// Entrypoint for the preprocessor. This handles a top level file. It prepares the
// output, runs the preprocessor, then prints the output to a file handle.
char *preprocess(char *filename, List *cli_directive_strings) {    init_cpp();
    parse_cli_directive_strings(cli_directive_strings);

    init_directives();

    FILE *f = fopen(filename, "r");

    if (f == 0) {
        perror(filename);
        exit(1);
    }

    init_cpp_from_fh(f, filename);

    output = new_string_buffer(state.input_size * 2);

    run_preprocessor_on_file(filename, 1);

    if (state.conditional_include_stack->prev) error("Unterminated #if");

    terminate_string_buffer(output);

    char *data = output->data;
    free_string_buffer(output, 0);

    free_directives();
    free_cpp_allocated_garbage();

    wfree(state.conditional_include_stack);
    init_cpp(); // For the next round

    free_cli_directives();

    return data;
}

// Run preprocessor on a file. If output_filename is '-' or not defined, send output
// to stdout.
void preprocess_to_file(char *input_filename, char *output_filename, List *cli_directive_strings) {
    char *data = preprocess(input_filename, cli_directive_strings);

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

    fprintf(cpp_output_file, "%s", data);
    if (cpp_output_file != stdout) fclose(cpp_output_file);

    wfree(data);
}
