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

    // Construct string representation of int, sse and stack allocation
    // The hex values are the parameter index
    char *got = malloc(256);
    b = got;

    for (int i = 0; i < 6; i++) {
        int allocated = 0;
        for (int j = 0; j < fpa->arg_count; j++) {
            if (fpa->locations[j].int_register == i) {
                b += sprintf(b, "%x", j);
                allocated = 1;
            }
        }
        if (!allocated) b += sprintf(b, " ");
    }

    b += sprintf(b, " | ");
    for (int i = 0; i < 8; i++) {
        int allocated = 0;
        for (int j = 0; j < fpa->arg_count; j++) {
            if (fpa->locations[j].sse_register == i) {
                b += sprintf(b, "%0x", j);
                allocated = 1;
            }
        }
        if (!allocated) b += sprintf(b, " ");
    }

    b += sprintf(b, " | %03x %03x | ", fpa->size, fpa->padding);
    b[0] = 0;
    int first = 1;
    int stack_offset = 0;
    for (int i = 0; i < fpa->arg_count; i++)
        if (fpa->locations[i].stack_offset != -1) {
            while (stack_offset < fpa->locations[i].stack_offset) {
                b += sprintf(b, " ");
                stack_offset += 8;
            }
            b += sprintf(b, "%x", i);
            stack_offset += 8;
        }

    assert_string(expected, got, description);
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

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv, &verbose);

    test_scalar_params();

    finalize();
}
