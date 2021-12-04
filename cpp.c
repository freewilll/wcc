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
FILE *cpp_output_file;         // Output file handle
StringBuffer *output;          // Output string buffer;
static int output_line_number; // How many lines have been outputted

static CppToken *subst(CppToken *is, StrMap *fp, CppToken **ap, StrSet *hs, CppToken *os);
static CppToken *hsadd(StrSet *hs, CppToken *ts);

// Append a string to the output
#define append_string_to_output(str) do { append_to_string_buffer(output, str); } while(0)

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
    cur_filename = filename;
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
        cur_line = *line_number;
        *lm = (*lm)->next;
    }
}

// Advance global current input pointers
static void advance_cur_ip(void) {
    advance_ip(&cpp_cur_ip, &cpp_cur_line_map, &cpp_cur_line_number);
}

static void advance_cur_ip_by_count(int count) {
    for (int i = 0; i < count; i++)
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
    int data_offset = 0;

    char *data = malloc(MAX_STRING_LITERAL_SIZE + 2);

    if (i[cpp_cur_ip] == 'L') {
        advance_cur_ip();
        data_offset = 1;
        data[0] = 'L';
    }

    advance_cur_ip();

    data[data_offset] = delimiter;
    int size = 0;

    while (cpp_cur_ip < cpp_input_size) {
        if (i[cpp_cur_ip] == delimiter) {
            advance_cur_ip();
            break;
        }

        if (cpp_input_size - cpp_cur_ip >= 2 && i[cpp_cur_ip] == '\\') {
            if (size + 1 >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
            data[data_offset + 1 + size++] = '\\';
            data[data_offset + 1 + size++] = i[cpp_cur_ip + 1];
            advance_cur_ip();
            advance_cur_ip();
        }

        else {
            if (size >= MAX_STRING_LITERAL_SIZE) panic("Exceeded maximum string literal size %d", MAX_STRING_LITERAL_SIZE);
            data[data_offset + 1 + size++] = i[cpp_cur_ip];
            advance_cur_ip();
        }
    }

    data[data_offset + size + 1] = delimiter;
    data[data_offset + size + 2] = 0;
    cpp_cur_token = new_cpp_token(CPP_TOK_STRING_LITERAL);
    cpp_cur_token->str = data;
}

#define is_pp_number(c1, c2) (c1 >= '0' && c1 <= '9') || (cpp_input_size - cpp_cur_ip >= 2 && c1 == '.' && (c2 >= '0' && c2 <= '9'))
#define is_non_digit(c1) ((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_')
#define is_exponent(c1, c2) (c1 == 'E' || c1 == 'e') && (c2 == '+' || c2 == '-')

#define make_token(kind, size) \
    do { \
        cpp_cur_token = new_cpp_token(kind); \
        copy_token_str(start, size); \
        advance_cur_ip_by_count(size); \
    } while (0)

#define make_other_token(size) make_token(CPP_TOK_OTHER, size)

#define copy_token_str(start, size) \
    do { \
        cpp_cur_token->str = malloc(size + 1); \
        memcpy(cpp_cur_token->str, start, size); \
        cpp_cur_token->str[size] = 0; \
    } while (0)

// Lex one CPP token, starting with optional whitespace
static void cpp_next() {
    char *whitespace = lex_whitespace();

    char *i = cpp_input;

    if (cpp_cur_ip >= cpp_input_size)
        cpp_cur_token = new_cpp_token(CPP_TOK_EOF);

    else {
        int left = cpp_input_size - cpp_cur_ip;
        char c1 = i[cpp_cur_ip];
        char c2 = i[cpp_cur_ip + 1];
        char c3 = left >= 3 ? i[cpp_cur_ip + 2] : 0;
        char *start = &(i[cpp_cur_ip]);

        if (c1 == '\n') {
            cpp_cur_token = new_cpp_token(CPP_TOK_EOL);
            cpp_cur_token->str = "\n";
            cpp_cur_token->line_number = cpp_cur_line_number; // Needs to be the line number of the \n token, not the next token
            advance_cur_ip();
        }

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

        else if ((c1 == '"') || (cpp_input_size - cpp_cur_ip >= 2 && c1 == 'L' && c2 == '"'))
            lex_string_and_char_literal('"');

        else if ((c1 == '\'') || (cpp_input_size - cpp_cur_ip >= 2 && c1 == 'L' && c2 == '\''))
            lex_string_and_char_literal('\'');

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
            copy_token_str(start, size);
        }

        else
            make_token(c1, 1);
    }

    // Set the line number of not already set
    if (!cpp_cur_token->line_number) cpp_cur_token->line_number = cpp_cur_line_number;
    cpp_cur_token->whitespace = whitespace;

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
// cll must be at the tail of the linked list
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
        CppToken *substituted = subst(directive->tokens, 0, 0, hs, 0);
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
        StrSet *hs1 =
            directive_token_hs && rparen_hs
                ? strset_intersection(directive_token_hs, rparen_hs)
                : directive_token_hs
                    ? directive_token_hs
                    : rparen_hs;


        StrSet *hs2 = new_strset();
        strset_add(hs2, directive_token->str);

        StrSet *hs = hs1 ? strset_union(hs1, hs2) : hs2;

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

// Inputs:
//   is: input sequence
//   fp: formal parameters
//   ap: actual parameters
//   hs: hide set
//   os: output sequence is a circular linked list
// Output
//   output sequence in a circular linked list
static CppToken *subst(CppToken *is, StrMap *fp, CppToken **ap, StrSet *hs, CppToken *os) {
    if (!is) {
        if (!os) return os;
        return hsadd(hs, os);
    }

    int fp_index = fp ? (int) (long) strmap_get(fp, is->str) : 0;
    if (fp_index) {
        CppToken *replacement = ap[fp_index - 1];

        CppToken *expanded = expand(replacement);
        if (!expanded) {
            expanded = new_cpp_token(CPP_TOK_PADDING);
            expanded->str = "";
        }
        expanded->whitespace = is->whitespace;

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

static CppToken *parse_define_replacement_tokens(void) {
    if (cpp_cur_token->kind == CPP_TOK_EOL || cpp_cur_token->kind == CPP_TOK_EOF)
        return 0;

    CppToken *result = cpp_cur_token;
    CppToken *tokens = cpp_cur_token;
    while (cpp_cur_token->kind != CPP_TOK_EOL && cpp_cur_token->kind != CPP_TOK_EOF) {
        tokens->next = cpp_cur_token;
        tokens = tokens->next;
        cpp_next();
    }
    tokens->next = 0;

    // Clear whitespace on initial token
    if (tokens) result->whitespace = 0;

    return result;
}

static Directive *parse_define_tokens(void) {
    Directive *directive = malloc(sizeof(Directive));
    CppToken *tokens;

    if (cpp_cur_token->kind == CPP_TOK_EOL || cpp_cur_token->kind == CPP_TOK_EOF) {
        directive->is_function = 0;
        tokens = 0;
    }
    else {
        if (cpp_cur_token->kind != CPP_TOK_LPAREN) {
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

            while (cpp_cur_token->kind != CPP_TOK_RPAREN && cpp_cur_token->kind != CPP_TOK_EOF) {
                if (!first) {
                    if (cpp_cur_token->kind != CPP_TOK_COMMA) panic("Expected comma");
                    cpp_next();
                }

                first = 0;

                if (cpp_cur_token->kind != CPP_TOK_IDENTIFIER) panic("Expected identifier");
                if (directive->param_count == MAX_CPP_MACRO_PARAM_COUNT) panic("Exceeded max CPP function macro param count");
                strmap_put(directive->param_identifiers, cpp_cur_token->str, (void *) (long) ++directive->param_count);
                cpp_next();
            }

            if (cpp_cur_token->kind != CPP_TOK_RPAREN) panic("Expected )");
            cpp_next();

            tokens = parse_define_replacement_tokens();
        }
    }

    directive->tokens = tokens;

    return directive;
}

static void parse_directive(void) {
    cpp_next();
    switch (cpp_cur_token->kind) {
        case CPP_TOK_DEFINE:
            cpp_next();
            if (cpp_cur_token->kind != CPP_TOK_IDENTIFIER) panic("Expected identifier");

            char *identifier = cpp_cur_token->str;
            cpp_next();

            Directive *directive = parse_define_tokens();
            strmap_put(directives, identifier, directive);

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
        default:
            panic("Unknown directive %s", cpp_cur_token->str);
    }
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

    return 0;
}

static void append_cpp_tokens_to_output(CppToken *token) {
    CppToken *prev = 0;
    while (token) {
        if (token->whitespace)
            append_string_to_output(token->whitespace);
        else if (prev && need_token_space(prev, token))
            append_string_to_output(" ");

        append_string_to_output(token->str);

        int is_eol = (token->kind == CPP_TOK_EOL);

        prev = token;
        token = token->next;

        if (is_eol) output_line_number++;

        if (is_eol && token) {
            // Output sufficient newlines to catch up with token->line_number
            // This ensures that each line in the input ends up on the same line
            // in the output.
            while (output_line_number < token->line_number) {
                output_line_number++;
                append_string_to_output("\n");
            }
        }
    }
}

// Parse tokens from the lexer
static void cpp_parse() {
    output_line_number = 1;
    int new_line = 1;
    CppToken *group_tokens = 0;

    while (cpp_cur_token->kind != CPP_TOK_EOF) {
        while (cpp_cur_token->kind != CPP_TOK_EOF) {
            if (new_line && cpp_cur_token->kind == CPP_TOK_HASH) {
                if (group_tokens) append_cpp_tokens_to_output(expand(convert_cll_to_ll(group_tokens)));

                parse_directive();
                group_tokens = 0;
            }
            else {
                group_tokens = cll_append(group_tokens, cpp_cur_token);
                new_line = (cpp_cur_token->kind == CPP_TOK_EOL);
                cpp_next();
            }
        }
    }

    if (group_tokens) append_cpp_tokens_to_output(expand(convert_cll_to_ll(group_tokens)));
}

Directive *parse_cli_define(char *string) {
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
    output = new_string_buffer(cpp_input_size * 2);
    cpp_parse();
    output->data[output->position] = 0;

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

    fprintf(cpp_output_file, "%s", output->data);
    fclose(cpp_output_file);
}
