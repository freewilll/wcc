#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../test-lib.h"

int check_stack_alignment();

int verbose;
int passes;
int failures;

// Run wcc and assert the assembly matches the expectation
static void check_output(char *code, char *expected, char *message) {
    char *input_filename = write_temp_c_file(code);

    char *output_filename = strdup("/tmp/test-XXXXXX.s");
    int fd = mkstemps(input_filename, 2);

    char command[128];
    sprintf(command, WCC " %s -I ../../include -Wno-extern-initializer -c -S -o %s", input_filename, output_filename);

    int result = system(command);
    if (result != 0) exit(result >> 8);

    FILE *f = fopen(output_filename, "r");
    if (!f) {
        printf("Unable to read %s\n", output_filename);
        exit(1);
    }

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int count = fread(buffer, sizeof(buffer), 1, f);
    if (count < 0) {
        printf("Unable to read %s\n", output_filename);
        exit(1);
    }

    fclose(f);

    char *stripped_buffer = strchr(buffer, '\n') + 1;
    stripped_buffer = strchr(stripped_buffer, '\n') + 1;
    stripped_buffer = strchr(stripped_buffer, '\n') + 1;

    if (!strcmp(stripped_buffer, expected)) {
        passes++;
        if (verbose) printf("%-60s ok\n", message);
    }
    else {
        failures++;
        printf("%-60s failed, content mismatch\n", message);
        printf("Expected:\n%s------------------\n", expected);
        printf("Got:\n%s----------------\n", stripped_buffer);
    }
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    // Read a test cases file that contains .c and expected .s code and assert the output is what is expected
    const char *test_cases_filename = SRC_DIR "/tests/integration/test-linkage-and-storage.cases";
    FILE *f = fopen(test_cases_filename, "r");
    if (!f) {
        printf("Unable to read %s\n", test_cases_filename);
        exit(1);
    }

    // Parse the test cases file and run the tests
    const int BUFFER_SIZE = 1024 * 1024;
    char *buffer = calloc(1024 * 1024, 1);

    char *message; //
    char *c_code = malloc(BUFFER_SIZE);
    char *s_code = malloc(BUFFER_SIZE);
    int c_code_pos = 0;
    int s_code_pos = 0;
    int reading_c_code = 0;
    int reading_s_code = 0;

    char line[1024];
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strlen(line) - 1] = 0;
        if (line[0] == '#') continue;

        int len = strlen(line);

        if (line[0] == '%') {
            message = strdup(&line[2]);
            reading_c_code = 1;
            c_code_pos = 0;
            s_code_pos = 0;
        }
        else if (line[0] == '-') {
            if (!reading_c_code && !reading_s_code) {
                reading_c_code = 1;
            }
            else if (reading_c_code) {
                c_code[c_code_pos] = 0;
                reading_c_code = 0;
                reading_s_code = 1;
            }
            else if (reading_s_code) {
                s_code[s_code_pos] = 0;
                reading_c_code = 0;
                reading_s_code = 0;

                // Run a test
                check_output(c_code, s_code, message);
            }
        }
        else {
            if (reading_c_code) {
                memcpy(&c_code[c_code_pos], line, len);
                c_code[c_code_pos + len] = '\n';
                c_code_pos += len + 1;
            }
            else if (reading_s_code) {
                memcpy(&s_code[s_code_pos], line, len);
                s_code[s_code_pos + len] = '\n';
                s_code_pos += len + 1;
            }
        }
    }

    fclose(f);

    finalize();
}
