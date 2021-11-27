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
    init_cpp_from_string(input);
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

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_transform_trigraphs();
    test_strip_backslash_newlines();

    finalize();
}
