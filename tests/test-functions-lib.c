#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wcc.h"
#include "test-lib.h"
#include "test-functions-lib.h"

static int shortcut_to_type(int s) {
    if (s == PI) return TYPE_INT;
    else if (s == PF) return TYPE_FLOAT;
    else if (s == PL) return TYPE_LONG_DOUBLE;
    else if (s == P1) return TYPE_INT128;
    else panic("Unknown type shortcut", s);
}

static char shortcut_to_char(int s) {
    if (s == PI) return 'i';
    else if (s == PF) return 'f';
    else if (s == PL) return 'l';
    else if (s == P1) return '1';
    else panic("Unknown type shortcut", s);
}

char *cva_result_str(CallValueAllocation *cva) {
    // Construct string representation of int, fp, stack allocation
    // The hex values are the parameter index
    char *result = malloc(256);
    char *b = result;

    // Add integer registers
    for (int i = 0; i < 8; i++) {
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

    // Add floating point registers
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

    // Add stack
    b += sprintf(b, " | %03x | ", cva->size);
    b[0] = 0;
    int first = 1;
    int stack_offset = 0;
    for (int i = 0; i < cva->locations->length; i++)
        // Output a space for each additional 8 bytes for stack occupations longer than 8 bytes
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
    move_indirect_stack_args(cva);


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

// Parse a function call with
// - an optional intitial_var type
// - count type_str
// and assert the allocation is correct.
void test_multiple_struct_params(int initial_var, char *type_str, int count, char *expected_cva_result_str) {
    Type *type = parse_type_str(type_str);
    char *english_type = sprint_type_in_english(type);
    CallValueAllocation *cva = init_call_value_allocaton("");

    if (initial_var) add_type_to_cva(cva, new_type(initial_var));

    for (int i = 0; i < count; i++) add_type_to_cva(cva,  parse_type_str(type_str));

    finalize_call_value_allocation(cva);
    move_indirect_stack_args(cva);

    assert_string(expected_cva_result_str, cva_result_str(cva), english_type);
}

void test_single_struct_param(char *type_str, char *expected_cva_result_str) {
    test_multiple_struct_params(0, type_str, 1, expected_cva_result_str);
}

