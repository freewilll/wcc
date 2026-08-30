#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wcc.h"
#include "test-lib.h"

int verbose;
int passes;
int failures;

#define CVA_CVL(cva, i) (*((CallValueLocations *) cva->locations->elements[i]))

// Shortcuts for parameters
enum {
    PI = 1, // integer
    PS = 2, // float/double
    PL = 3, // long double
    P1 = 4, // int128
};

int shortcut_to_type(int s) {
    if (s == PI) return TYPE_INT;
    else if (s == PS) return TYPE_FLOAT;
    else if (s == PL) return TYPE_LONG_DOUBLE;
    else if (s == P1) return TYPE_INT128;
    else panic("Unknown type shortcut", s);
}

char shortcut_to_char(int s) {
    if (s == PI) return 'i';
    else if (s == PS) return 's';
    else if (s == PL) return 'l';
    else if (s == P1) return '1';
    else panic("Unknown type shortcut", s);
}

char *cva_result_str(CallValueAllocation *cva) {
    // Construct string representation of int, sse and stack allocation
    // The hex values are the parameter index
    char *result = malloc(256);
    char *b = result;

    for (int i = 0; i < 6; i++) {
        int allocated = 0;
        for (int j = 0; j < cva->locations->length; j++) {
            int location_counts = CVA_CVL(cva, j).count;
            for (int k = 0; k < location_counts; k++) {
                if (((CallValueLocations *) cva->locations->elements[j])->locations[k].int_register == i) {
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
        for (int j = 0; j < cva->locations->length; j++) {
            int location_counts = CVA_CVL(cva, j).count;
            for (int k = 0; k < location_counts; k++) {
                if (((CallValueLocations *)cva->locations->elements[j])->locations[k].fp_register == i) {
                    b += sprintf(b, "%0x", j);
                    allocated = 1;
                }
            }
        }
        if (!allocated) b += sprintf(b, " ");
    }

    b += sprintf(b, " | %03x %03x | ", cva->size, cva->padding);
    b[0] = 0;
    int first = 1;
    int stack_offset = 0;
    for (int i = 0; i < cva->locations->length; i++)
        if (((CallValueLocations *) cva->locations->elements[i])->locations[0].stack_offset != -1) {
            while (stack_offset < ((CallValueLocations *) cva->locations->elements[i])->locations[0].stack_offset) {
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
    CallValueAllocation *cva = init_call_value_allocaton("test_function");

    if (a0 ) add_type_to_cva(cva, new_type(shortcut_to_type(a0 )));
    if (a1 ) add_type_to_cva(cva, new_type(shortcut_to_type(a1 )));
    if (a2 ) add_type_to_cva(cva, new_type(shortcut_to_type(a2 )));
    if (a3 ) add_type_to_cva(cva, new_type(shortcut_to_type(a3 )));
    if (a4 ) add_type_to_cva(cva, new_type(shortcut_to_type(a4 )));
    if (a5 ) add_type_to_cva(cva, new_type(shortcut_to_type(a5 )));
    if (a6 ) add_type_to_cva(cva, new_type(shortcut_to_type(a6 )));
    if (a7 ) add_type_to_cva(cva, new_type(shortcut_to_type(a7 )));
    if (a8 ) add_type_to_cva(cva, new_type(shortcut_to_type(a8 )));
    if (a9 ) add_type_to_cva(cva, new_type(shortcut_to_type(a9 )));
    if (a10) add_type_to_cva(cva, new_type(shortcut_to_type(a10)));
    if (a11) add_type_to_cva(cva, new_type(shortcut_to_type(a11)));
    if (a12) add_type_to_cva(cva, new_type(shortcut_to_type(a12)));
    if (a13) add_type_to_cva(cva, new_type(shortcut_to_type(a13)));
    if (a14) add_type_to_cva(cva, new_type(shortcut_to_type(a14)));
    if (a15) add_type_to_cva(cva, new_type(shortcut_to_type(a15)));

    finalize_call_value_allocation(cva);

    char *description = calloc(1, 256);
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

    char *got = cva_result_str(cva);

    assert_string(expected, got, description);
}

// Convert list of CallValueLocation into string representation
// {argm}:[In|So|STp]
char *cvl_result_str(CallValueLocation *cvl, int count) {
    char *result = malloc(256);
    char *b = result;

    for (int i = 0; i < count; i++) {
        if (i != 0) b += sprintf(b, " ");
        b += sprintf(b, "%d:", i);
        if (cvl[i].int_register != -1) b += sprintf(b, "I%d", cvl[i].int_register);
        else if (cvl[i].fp_register != -1) b += sprintf(b, "S%d", cvl[i].fp_register);
        else b += sprintf(b, "ST%d", cvl[i].stack_offset);
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

    FILE *f = fopen(filename, "w");
    fprintf(f, "%s\n", type_str);
    fprintf(f, "\n");
    fclose(f);

    init_lexer_from_filename(filename);
    init_parser();
    init_scopes();

    return parse_type_name();
}

CallValueAllocation *run_with_multiple_structs(int initial_var, char *struct_str, int count) {
    CallValueAllocation *cva = init_call_value_allocaton("");
    if (initial_var) add_type_to_cva(cva, new_type(initial_var));
    for (int i = 0; i < count; i++) add_type_to_cva(cva,  parse_type_str(struct_str));
    finalize_call_value_allocation(cva);
    return cva;
}

void test_struct_params() {
    Type *type;

    CallValueAllocation *cva;

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int a, b, c; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "00     |          | 000 000 | ", sprint_type_in_english(type));

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int a, b; double d; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "0      | 0        | 000 000 | ", sprint_type_in_english(type));

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int a; float d; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "0      |          | 000 000 | ", sprint_type_in_english(type));

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int a, b; struct {int c; int d; } s; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "00     |          | 000 000 | ", sprint_type_in_english(type));

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { long double ld; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "       |          | 010 000 | 0", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 0).count, "Location counts are 1");

     // Example from ABI doc
    cva = init_call_value_allocaton("");
    add_type_to_cva(cva, new_type(TYPE_INT));
    add_type_to_cva(cva, new_type(TYPE_INT));
    add_type_to_cva(cva, parse_type_str("struct { int a, b; double d; }"));
    add_type_to_cva(cva, new_type(TYPE_INT));
    add_type_to_cva(cva, new_type(TYPE_INT));
    add_type_to_cva(cva, new_type(TYPE_LONG_DOUBLE));
    add_type_to_cva(cva, new_type(TYPE_DOUBLE));
    add_type_to_cva(cva, new_type(TYPE_DOUBLE));
    add_type_to_cva(cva, new_type(TYPE_INT));
    add_type_to_cva(cva, new_type(TYPE_INT));
    add_type_to_cva(cva, new_type(TYPE_INT));
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "012348 | 267      | 020 000 | 5 9a", "Example from ABI doc v0.98");
    assert_string("0:I2 1:S0", cvl_result_str(&(((CallValueLocations *) cva->locations->elements[2])->locations[0]), CVA_CVL(cva, 2).count), "Example from ABI doc v0.98 arg 2");

    // Test defaulting to memory
    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int i; long double ld; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "       |          | 020 000 | 0", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 0).count, "Location counts are 1");

    // Test defaulting to memory
    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { long i, j, k; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "       |          | 018 000 | 0", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 0).count, "Location counts are 1");

    // Test running out of registers for struct/union
    cva = run_with_multiple_structs(0, "struct { int i[4]; }", 4);
    assert_string(cva_result_str(cva), "001122 |          | 010 000 | 3", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 3).count, "Location counts are 1");

    // Test running out of registers for struct/union
    cva = run_with_multiple_structs(0, "struct { int i[4]; }", 5);
    assert_string(cva_result_str(cva), "001122 |          | 020 000 | 3 4", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 3).count, "Location counts are 1");
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts are 1");

    cva = run_with_multiple_structs(TYPE_INT, "struct { int i[4]; }", 3);
    assert_string(cva_result_str(cva), "01122  |          | 010 000 | 3", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 3).count, "Location counts are 1");

    cva = run_with_multiple_structs(TYPE_INT, "struct { int i[4]; }", 4);
    assert_string(cva_result_str(cva), "01122  |          | 020 000 | 3 4", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 3).count, "Location counts 1");
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts 1");

    cva = run_with_multiple_structs(0, "struct { float i[4]; }", 5);
    assert_string(cva_result_str(cva), "       | 00112233 | 010 000 | 4", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts 1");

    cva = run_with_multiple_structs(0, "struct { float i[4]; }", 6);
    assert_string(cva_result_str(cva), "       | 00112233 | 020 000 | 4 5", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts 1");
    assert_int(1, CVA_CVL(cva, 5).count, "Location counts 1");

    cva = run_with_multiple_structs(TYPE_FLOAT, "struct { float i[4]; }", 4);
    assert_string(cva_result_str(cva), "       | 0112233  | 010 000 | 4", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts are 1");

    cva = run_with_multiple_structs(TYPE_FLOAT, "struct { float i[4]; }", 5);
    assert_string(cva_result_str(cva), "       | 0112233  | 020 000 | 4 5", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts 1");
    assert_int(1, CVA_CVL(cva, 5).count, "Location counts 1");
}

void test_int128() {
    test_param_allocation(P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00     |          | 000 000 | ");
    test_param_allocation(P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0011   |          | 000 000 | ");
    test_param_allocation(P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0011   |          | 000 000 | ");
    test_param_allocation(P1, P1, PI, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00112  |          | 010 000 | 3");
    test_param_allocation(P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122 |          | 010 000 | 3");
    test_param_allocation(P1, P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122 |          | 020 000 | 3 4");
    test_param_allocation(P1, P1, P1, P1, P1, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122 |          | 030 008 | 3 4 5");
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv);

    init_memory_management_for_translation_unit();

    test_scalar_params();
    test_struct_params();
    test_int128();

    finalize();
}
