#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../wcc.h"
#include "../test-lib.h"


int verbose;
int passes;
int failures;

typedef struct line_map_tuple {
    int position;
    int line_number;
} LineMapTuple;

static void test_transform_trigraphs() {
    init_cpp_from_string("?" "?" "="); transform_trigraphs(); assert_string("#",  get_cpp_input(), "Trigraph #");
    init_cpp_from_string("?" "?" "("); transform_trigraphs(); assert_string("[",  get_cpp_input(), "Trigraph [");
    init_cpp_from_string("?" "?" "/"); transform_trigraphs(); assert_string("\\", get_cpp_input(), "Trigraph \\");
    init_cpp_from_string("?" "?" ")"); transform_trigraphs(); assert_string("]",  get_cpp_input(), "Trigraph ]");
    init_cpp_from_string("?" "?" "'"); transform_trigraphs(); assert_string("^",  get_cpp_input(), "Trigraph ^");
    init_cpp_from_string("?" "?" "<"); transform_trigraphs(); assert_string("{",  get_cpp_input(), "Trigraph {");
    init_cpp_from_string("?" "?" "!"); transform_trigraphs(); assert_string("|",  get_cpp_input(), "Trigraph |");
    init_cpp_from_string("?" "?" ">"); transform_trigraphs(); assert_string("}",  get_cpp_input(), "Trigraph }");
    init_cpp_from_string("?" "?" "-"); transform_trigraphs(); assert_string("~",  get_cpp_input(), "Trigraph ~");
}

static void test_strip_bsnl(char *input, char *expected_output, LineMapTuple expected_linemap[]) {
    init_cpp_from_string(strdup(input));
    strip_backslash_newlines();
    char *output = get_cpp_input();
    if (strcmp(expected_output, output)) {
        printf("Failed in comparison ...\n");
        printf("Input:\n%s\nExpected:\n%s\nGot:\n%s\n\n", input, expected_output, output);
        failures++;
    }

    LineMap *lm = get_cpp_linemap();
    int elm_index = 0;
    while (lm) {
        if (!expected_linemap[elm_index].line_number) {
            printf("Failed in line map ...\n");
            printf("Input:\n%s\nGot too many entries %d %d\n",
                input, lm->position, lm->line_number);
            failures++;
        }
        else {
            if (lm->position != expected_linemap[elm_index].position || lm->line_number != expected_linemap[elm_index].line_number) {
                printf("Failed in line map ...\n");
                printf("Input:\n%s\nExpected: %d %d\n\nGot:%d %d\n\n",
                    input, expected_linemap[elm_index].position, expected_linemap[elm_index].line_number, lm->position, lm->line_number);
            }
        }

        lm = lm->next;
        elm_index++;
    }

    if (expected_linemap[elm_index].line_number) {
        printf("Failed in line map ...\n");
        printf("Input:\n%s\nGot trailing entry/entries %d %d\n",
            input, expected_linemap[elm_index].position, expected_linemap[elm_index].line_number);
        failures++;
    }
}

static void test_strip_backslash_newlines(void) {
    test_strip_bsnl("foo",          "foo\n",        (LineMapTuple[]) {0,1, 0,0});
    test_strip_bsnl("foo\nbar",     "foo\nbar\n",   (LineMapTuple[]) {0,1, 4,2, 0,0});
    test_strip_bsnl("foo\nbar\n",   "foo\nbar\n",   (LineMapTuple[]) {0,1, 4,2, 8,3, 0,0});
    test_strip_bsnl("foo\\\nbar",   "foobar\n",     (LineMapTuple[]) {0,1, 3,2, 0,0});
    test_strip_bsnl("foo\\\n bar",  "foo bar\n",    (LineMapTuple[]) {0,1, 3,2, 0,0});
    test_strip_bsnl("foo \\\nbar",  "foo bar\n",    (LineMapTuple[]) {0,1, 4,2, 0,0});
    test_strip_bsnl("a\\\nb\\\nc",  "abc\n",        (LineMapTuple[]) {0,1, 1,2, 2,3, 0,0});
    test_strip_bsnl("a\\\n\\\nb",   "ab\n",         (LineMapTuple[]) {0,1, 1,3, 0,0});
}

static void test_single_token(char *input, int kind, char *expected) {
    CppToken *token = parse_cli_define(input)->tokens;
    if (!token) panic("Expected token");

    // Ensure there is one token, so the circular linked list's head == tail
    assert_long(1, token == token->next, input);

    token = token->next; // Go to the head of the CLL
    assert_int(kind, token->kind, input);
    assert_string(expected, token->str, input);
}

static void test_tokenization(void) {
    CppToken *tokens;

    // Two identifiers
    tokens = parse_cli_define("foo bar")->tokens->next;
    assert_int(CPP_TOK_IDENTIFIER, tokens->kind, "foo kind");
    assert_string("foo", tokens->str, "foo str");
    tokens = tokens->next;
    assert_int(CPP_TOK_IDENTIFIER, tokens->kind, "bar kind");
    assert_string("bar", tokens->str, "bar str");

    // String & character literals
    test_single_token("\"foo\"", CPP_TOK_STRING_LITERAL, "\"foo\"");
    test_single_token("'a'", CPP_TOK_STRING_LITERAL, "'a'");

    // Numbers
    test_single_token("12",         CPP_TOK_NUMBER, "12");
    test_single_token("99",         CPP_TOK_NUMBER, "99");
    test_single_token("1.21",       CPP_TOK_NUMBER, "1.21");
    test_single_token("123.456",    CPP_TOK_NUMBER, "123.456");
    test_single_token("1.21x",      CPP_TOK_NUMBER, "1.21x");
    test_single_token("1.21x_y_AZ", CPP_TOK_NUMBER, "1.21x_y_AZ");
    test_single_token("1.21E+1",    CPP_TOK_NUMBER, "1.21E+1");
    test_single_token("1.21e-1",    CPP_TOK_NUMBER, "1.21e-1");
    test_single_token("1.",         CPP_TOK_NUMBER, "1.");

    // Multi-character tokens
    test_single_token("&&",  CPP_TOK_OTHER, "&&");
    test_single_token("||",  CPP_TOK_OTHER, "||");
    test_single_token("==",  CPP_TOK_OTHER, "==");
    test_single_token("!=",  CPP_TOK_OTHER, "!=");
    test_single_token("<=",  CPP_TOK_OTHER, "<=");
    test_single_token(">=",  CPP_TOK_OTHER, ">=");
    test_single_token("++",  CPP_TOK_INC,   "++");
    test_single_token("--",  CPP_TOK_DEC,   "--");
    test_single_token("+=",  CPP_TOK_OTHER, "+=");
    test_single_token("-=",  CPP_TOK_OTHER, "-=");
    test_single_token("*=",  CPP_TOK_OTHER, "*=");
    test_single_token("/=",  CPP_TOK_OTHER, "/=");
    test_single_token("%=",  CPP_TOK_OTHER, "%=");
    test_single_token("&=",  CPP_TOK_OTHER, "&=");
    test_single_token("|=",  CPP_TOK_OTHER, "|=");
    test_single_token("^=",  CPP_TOK_OTHER, "^=");
    test_single_token("->",  CPP_TOK_OTHER, "->");
    test_single_token(">>=", CPP_TOK_OTHER, ">>=");
    test_single_token("<<=", CPP_TOK_OTHER, "<<=");
    test_single_token("...", CPP_TOK_OTHER, "...");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_transform_trigraphs();
    test_strip_backslash_newlines();
    test_tokenization();

    finalize();
}
