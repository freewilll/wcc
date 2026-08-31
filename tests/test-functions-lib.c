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
    // Construct string representation of int, fp and stack allocation
    // The hex values are the parameter index
    char *result = malloc(256);
    char *b = result;

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
