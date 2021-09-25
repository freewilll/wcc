#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../wcc.h"
#include "../test-lib.h"

int verbose;
int passes;
int failures;

// Shortcuts for parameters
enum {
    PI = 1, // integer
    PS = 2, // float/double
    PL = 3, // long double
};

int shortcut_to_type(int s) {
    if (s == PI) return TYPE_INT;
    else if (s == PS) return TYPE_FLOAT;
    else if (s == PL) return TYPE_LONG_DOUBLE;
    else panic1d("Unknown type shortcut", s);
}

char shortcut_to_char(int s) {
    if (s == PI) return 'i';
    else if (s == PS) return 's';
    else if (s == PL) return 'l';
    else panic1d("Unknown type shortcut", s);
}

char *fpa_result_str(FunctionParamAllocation *fpa) {
    // Construct string representation of int, sse and stack allocation
    // The hex values are the parameter index
    char *result = malloc(256);
    char *b = result;

    for (int i = 0; i < 6; i++) {
        int allocated = 0;
        for (int j = 0; j < fpa->arg_count; j++) {
            int location_counts = fpa->params[j].count;
            for (int k = 0; k < location_counts; k++) {
                if (fpa->params[j].locations[k].int_register == i) {
                    b += sprintf(b, "%x", j);
                    allocated = 1;
                }
            }
        }
        if (!allocated) b += sprintf(b, " ");
    }

    b += sprintf(b, " | ");
    for (int i = 0; i < 8; i++) {
        int allocated = 0;
        for (int j = 0; j < fpa->arg_count; j++) {
            int location_counts = fpa->params[j].count;
            for (int k = 0; k < location_counts; k++) {
                if (fpa->params[j].locations[k].sse_register == i) {
                    b += sprintf(b, "%0x", j);
                    allocated = 1;
                }
            }
        }
        if (!allocated) b += sprintf(b, " ");
    }

    b += sprintf(b, " | %03x %03x | ", fpa->size, fpa->padding);
    b[0] = 0;
    int first = 1;
    int stack_offset = 0;
    for (int i = 0; i < fpa->arg_count; i++)
        if (fpa->params[i].locations[0].stack_offset != -1) {
            while (stack_offset < fpa->params[i].locations[0].stack_offset) {
                b += sprintf(b, " ");
                stack_offset += 8;
            }
            b += sprintf(b, "%x", i);
            stack_offset += 8;
        }

    return result;
}

// Process 16 parameters and assert that string representation of the register & stack layout matches
void test_param_allocation(
        int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7,
        int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15, char *expected) {
    FunctionParamAllocation *fpa = init_function_param_allocaton("test_function");

    if (a0 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a0 )));
    if (a1 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a1 )));
    if (a2 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a2 )));
    if (a3 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a3 )));
    if (a4 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a4 )));
    if (a5 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a5 )));
    if (a6 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a6 )));
    if (a7 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a7 )));
    if (a8 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a8 )));
    if (a9 ) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a9 )));
    if (a10) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a10)));
    if (a11) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a11)));
    if (a12) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a12)));
    if (a13) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a13)));
    if (a14) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a14)));
    if (a15) add_function_param_to_allocation(fpa, new_type(shortcut_to_type(a15)));

    finalize_function_param_allocation(fpa);

    char *description = malloc(256);
    memset(description, 0, 256);
    sprintf(description, "Function param placing                 ");
    char *b = &(description[23]);

    if (a0 ) b[ 0] = shortcut_to_char(a0 );
    if (a1 ) b[ 1] = shortcut_to_char(a1 );
    if (a2 ) b[ 2] = shortcut_to_char(a2 );
    if (a3 ) b[ 3] = shortcut_to_char(a3 );
    if (a4 ) b[ 4] = shortcut_to_char(a4 );
    if (a5 ) b[ 5] = shortcut_to_char(a5 );
    if (a6 ) b[ 6] = shortcut_to_char(a6 );
    if (a7 ) b[ 7] = shortcut_to_char(a7 );
    if (a8 ) b[ 8] = shortcut_to_char(a8 );
    if (a9 ) b[ 9] = shortcut_to_char(a9 );
    if (a10) b[10] = shortcut_to_char(a10);
    if (a11) b[11] = shortcut_to_char(a11);
    if (a12) b[12] = shortcut_to_char(a12);
    if (a13) b[13] = shortcut_to_char(a13);
    if (a14) b[14] = shortcut_to_char(a14);
    if (a15) b[15] = shortcut_to_char(a15);

    char *got = fpa_result_str(fpa);

    assert_string(expected, got, description);
}

// Convert list of FunctionParamLocation into string representation
// {argm}:[In|So|STp]
char *fpl_result_str(FunctionParamLocation *fpl, int count) {
    char *result = malloc(256);
    char *b = result;

    for (int i = 0; i < count; i++) {
        if (i != 0) b += sprintf(b, " ");
        b += sprintf(b, "%d:", i);
        if (fpl[i].int_register != -1) b += sprintf(b, "I%d", fpl[i].int_register);
        else if (fpl[i].sse_register != -1) b += sprintf(b, "S%d", fpl[i].sse_register);
        else b += sprintf(b, "ST%d", fpl[i].stack_offset);
    }

    return result;
}

void test_scalar_params() {
    test_param_allocation(PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0      |          | 000 000 | ");
    test_param_allocation(PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "01     |          | 000 000 | ");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, "012345 |          | 050 000 | 6789abcdef");
    test_param_allocation(PS, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "       | 0        | 000 000 | ");
    test_param_allocation(PS, PS, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "       | 01       | 000 000 | ");
    test_param_allocation(PS, PS, PS, PS, PS, PS, PS, PS, PS, PS, PS, PS, PS, PS, PS, PS, "       | 01234567 | 040 000 | 89abcdef");
    test_param_allocation(PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "       |          | 010 000 | 0");
    test_param_allocation(PL, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "       |          | 020 000 | 0 1");
    test_param_allocation(PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, "       |          | 100 000 | 0 1 2 3 4 5 6 7 8 9 a b c d e f");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PS, PS, PS, PS, PS, PS, PS, PS, PI, PI, "012345 | 6789abcd | 010 000 | ef");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PS, PS, PS, PS, PS, PS, PS, PS, PI, PS, "012345 | 6789abcd | 010 000 | ef");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PS, PS, PS, PS, PS, PS, PS, PS, PS, PS, "012345 | 6789abcd | 010 000 | ef");
    test_param_allocation(PI, PL, 0 , 0 , 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0      |          | 010 000 | 1");
    test_param_allocation(PI, PL, PI, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "02     |          | 020 000 | 1 3");
    test_param_allocation(PI, PL, PI, PL, PI, PL, PI, PL, 0,  0,  0,  0,  0,  0,  0,  0,  "0246   |          | 040 000 | 1 3 5 7");
    test_param_allocation(PL, PI, PL, PI, PL, PI, PL, PI, 0,  0,  0,  0,  0,  0,  0,  0,  "1357   |          | 040 000 | 0 2 4 6");
    test_param_allocation(PL, PL, PL, PL, PL, PL, PI, PI, PI, PI, PI, PI, 0,  0,  0,  0,  "6789ab |          | 060 000 | 0 1 2 3 4 5");
    test_param_allocation(PI, PS, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0      | 1        | 010 000 | 2");
    test_param_allocation(PI, PS, PL, PI, PS, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "03     | 14       | 020 000 | 2 5");
    test_param_allocation(PI, PS, PL, PI, PS, PL, PI, PS, PL, PI, PS, PL, PI, PS, PL, PI, "0369cf | 147ad    | 050 000 | 2 5 8 b e");
}

Type *parse_type_str(char *type_str) {
    char *filename =  make_temp_filename("/tmp/XXXXXX.c");

    f = fopen(filename, "w");
    fprintf(f, "%s\n", type_str);
    fprintf(f, "\n");
    fclose(f);

    init_lexer(filename);
    init_parser();
    init_scopes();

    return parse_type_name();
}

void test_struct_params() {
    Type *type;

    FunctionParamAllocation *fpa;

    fpa = init_function_param_allocaton("");
    type = parse_type_str("struct { int a, b, c; }");
    add_function_param_to_allocation(fpa, type);
    finalize_function_param_allocation(fpa);
    assert_string(fpa_result_str(fpa), "00     |          | 000 000 | ", sprint_type_in_english(type));

    fpa = init_function_param_allocaton("");
    type = parse_type_str("struct { int a, b; double d; }");
    add_function_param_to_allocation(fpa, type);
    finalize_function_param_allocation(fpa);
    assert_string(fpa_result_str(fpa), "0      | 0        | 000 000 | ", sprint_type_in_english(type));

    fpa = init_function_param_allocaton("");
    type = parse_type_str("struct { int a; float d; }");
    add_function_param_to_allocation(fpa, type);
    finalize_function_param_allocation(fpa);
    assert_string(fpa_result_str(fpa), "0      |          | 000 000 | ", sprint_type_in_english(type));

    fpa = init_function_param_allocaton("");
    type = parse_type_str("struct { int a, b; struct {int c; int d; } s; }");
    add_function_param_to_allocation(fpa, type);
    finalize_function_param_allocation(fpa);
    assert_string(fpa_result_str(fpa), "00     |          | 000 000 | ", sprint_type_in_english(type));

    fpa = init_function_param_allocaton("");
    type = parse_type_str("struct { long double ld; }");
    add_function_param_to_allocation(fpa, type);
    finalize_function_param_allocation(fpa);
    assert_string(fpa_result_str(fpa), "       |          | 010 000 | 0", sprint_type_in_english(type));
    assert_int(1, fpa->params[0].count, "Location counts are 1");

     // Example from ABI doc
    fpa = init_function_param_allocaton("");
    add_function_param_to_allocation(fpa, new_type(TYPE_INT));
    add_function_param_to_allocation(fpa, new_type(TYPE_INT));
    add_function_param_to_allocation(fpa, parse_type_str("struct { int a, b; double d; }"));
    add_function_param_to_allocation(fpa, new_type(TYPE_INT));
    add_function_param_to_allocation(fpa, new_type(TYPE_INT));
    add_function_param_to_allocation(fpa, new_type(TYPE_LONG_DOUBLE));
    add_function_param_to_allocation(fpa, new_type(TYPE_DOUBLE));
    add_function_param_to_allocation(fpa, new_type(TYPE_DOUBLE));
    add_function_param_to_allocation(fpa, new_type(TYPE_INT));
    add_function_param_to_allocation(fpa, new_type(TYPE_INT));
    add_function_param_to_allocation(fpa, new_type(TYPE_INT));
    finalize_function_param_allocation(fpa);
    assert_string(fpa_result_str(fpa), "012348 | 267      | 020 000 | 5 9a", "Example from ABI doc v0.98");
    assert_string("0:I2 1:S0", fpl_result_str(&(fpa->params[2].locations[0]), fpa->params[2].count), "Example from ABI doc v0.98 arg 2");

    // Test defaulting to memory
    fpa = init_function_param_allocaton("");
    type = parse_type_str("struct { int i; long double ld; }");
    add_function_param_to_allocation(fpa, type);
    finalize_function_param_allocation(fpa);
    assert_string(fpa_result_str(fpa), "       |          | 020 000 | 0", sprint_type_in_english(type));
    assert_int(1, fpa->params[0].count, "Location counts are 1");

    // Test defaulting to memory
    fpa = init_function_param_allocaton("");
    type = parse_type_str("struct { long i, j, k; }");
    add_function_param_to_allocation(fpa, type);
    finalize_function_param_allocation(fpa);
    assert_string(fpa_result_str(fpa), "       |          | 018 000 | 0", sprint_type_in_english(type));
    assert_int(1, fpa->params[0].count, "Location counts are 1");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_scalar_params();
    test_struct_params();

    finalize();
}
