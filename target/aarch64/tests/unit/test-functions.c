#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wcc.h"
#include "test-lib.h"
#include "test-functions-lib.h"

int verbose;
int passes;
int failures;

void test_scalar_params() {
    test_param_allocation(PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0        |          | 000 | ");
    test_param_allocation(PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "01       |          | 000 | ");
    test_param_allocation(PI, PI, PI, PI, PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "012345   |          | 000 | ");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  "01234567 |          | 000 | ");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, "01234567 |          | 040 | 89abcdef");
    test_param_allocation(PF, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 0        | 000 | ");
    test_param_allocation(PF, PF, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 01       | 000 | ");
    test_param_allocation(PF, PF, PF, PF, PF, PF, PF, PF, 0,  0,  0,  0,  0,  0,  0,  0,  "         | 01234567 | 000 | ");
    test_param_allocation(PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, "         | 01234567 | 040 | 89abcdef");
    test_param_allocation(PL, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 01       | 000 | ");
    test_param_allocation(PL, PL, PL, PL, PL, PL, PL, PL, 0,  0,  0,  0,  0,  0,  0,  0,  "         | 01234567 | 000 | ");
    test_param_allocation(PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, "         | 01234567 | 080 | 8 9 a b c d e f");
    test_param_allocation(PI, PF, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0        | 12       | 000 | ");
    test_param_allocation(PI, PF, PL, PI, PF, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "03       | 1245     | 000 | ");
    test_param_allocation(PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, "0369cf   | 124578ab | 020 | d e");
}

void test_struct_params() {
    // Structs with only integers
    test_single_struct_param("struct { int a; }",           "0        |          | 000 | ");
    test_single_struct_param("struct { int a, b; }",        "0        |          | 000 | ");
    test_single_struct_param("struct { int a, b, c; }",     "00       |          | 000 | ");
    test_single_struct_param("struct { int a, b, c, d; }",  "00       |          | 000 | ");
    test_single_struct_param("struct { int [4]; }",         "00       |          | 000 | ");
    test_single_struct_param("struct { int [5]; }",         "0        |          | 020 | "); // Structs > 16 bytes go on the stack
    test_single_struct_param("struct { int [6]; }",         "0        |          | 020 | ");
    test_single_struct_param("struct { int [7]; }",         "0        |          | 020 | ");
    test_single_struct_param("struct { int [8]; }",         "0        |          | 020 | ");
    test_single_struct_param("struct { int [9]; }",         "0        |          | 030 | ");

    // HFA Structs
    test_single_struct_param("struct { float f[1]; }",        "         | 0        | 000 | "); // HFA structs
    test_single_struct_param("struct { float f[2]; }",        "         | 00       | 000 | ");
    test_single_struct_param("struct { float f[3]; }",        "         | 000      | 000 | ");
    test_single_struct_param("struct { float f[4]; }",        "         | 0000     | 000 | ");
    test_single_struct_param("struct { double d[1]; }",       "         | 0        | 000 | ");
    test_single_struct_param("struct { double d[2]; }",       "         | 00       | 000 | ");
    test_single_struct_param("struct { double d[3]; }",       "         | 000      | 000 | ");
    test_single_struct_param("struct { double d[4]; }",       "         | 0000     | 000 | ");
    test_single_struct_param("struct { long double ld[1]; }", "         | 0        | 000 | ");
    test_single_struct_param("struct { long double ld[2]; }", "         | 00       | 000 | ");
    test_single_struct_param("struct { long double ld[3]; }", "         | 000      | 000 | ");
    test_single_struct_param("struct { long double ld[4]; }", "         | 0000     | 000 | ");

    // Mixed structs
    test_single_struct_param("struct { int i; float f; }",       "0        |          | 000 | ");
    test_single_struct_param("struct { int i; double f; }",      "00       |          | 000 | ");
    test_single_struct_param("struct { float f; double d; }",    "00       |          | 000 | ");
    test_single_struct_param("struct { float f[2]; double d; }", "00       |          | 000 | ");
    test_single_struct_param("struct { float f[3]; double d; }", "0        |          | 020 | "); // Structs > 16 bytes go on the stack

    // Non-HFA structs sized > 16 on the stack
    test_single_struct_param("struct { char c[17]; }",           "0        |          | 020 | ");
    test_single_struct_param("struct { float f[5]; }",           "0        |          | 020 | ");
    test_single_struct_param("struct { double d[5]; }",          "0        |          | 030 | ");
    test_single_struct_param("struct { long double d[5]; }",     "0        |          | 050 | ");

    // Test running out of registers for struct/union
    test_multiple_struct_params(0,          "struct { int i[4]; }",   4, "00112233 |          | 000 | ");
    test_multiple_struct_params(0,          "struct { int i[4]; }",   5, "00112233 |          | 010 | 4");
    test_multiple_struct_params(TYPE_INT,   "struct { int i[4]; }",   3, "0112233  |          | 000 | ");
    test_multiple_struct_params(TYPE_INT,   "struct { int i[4]; }",   4, "0112233  |          | 010 | 4");
    test_multiple_struct_params(TYPE_INT,   "struct { int i[4]; }",   5, "0112233  |          | 020 | 4 5");
    test_multiple_struct_params(0,          "struct { float f[4]; }", 5, "         | 00001111 | 030 | 2 3 4");
    test_multiple_struct_params(0,          "struct { float f[4]; }", 6, "         | 00001111 | 040 | 2 3 4 5");
    test_multiple_struct_params(TYPE_FLOAT, "struct { float f[4]; }", 4, "         | 01111    | 030 | 2 3 4");
    test_multiple_struct_params(TYPE_FLOAT, "struct { float f[4]; }", 5, "         | 01111    | 040 | 2 3 4 5");

    test_multiple_struct_params(0,          "struct { double d[2]; }", 1, "         | 00       | 000 | ");
    test_multiple_struct_params(0,          "struct { double d[2]; }", 2, "         | 0011     | 000 | ");
    test_multiple_struct_params(0,          "struct { double d[2]; }", 5, "         | 00112233 | 010 | 4");
    test_multiple_struct_params(0,          "struct { double d[2]; }", 6, "         | 00112233 | 020 | 4 5");
    test_multiple_struct_params(TYPE_FLOAT, "struct { double d[2]; }", 4, "         | 0112233  | 010 | 4");
    test_multiple_struct_params(TYPE_FLOAT, "struct { double d[2]; }", 5, "         | 0112233  | 020 | 4 5");

    test_multiple_struct_params(0,          "struct { long double d[2]; }", 1, "         | 00       | 000 | ");
    test_multiple_struct_params(0,          "struct { long double d[2]; }", 2, "         | 0011     | 000 | ");
    test_multiple_struct_params(0,          "struct { long double d[2]; }", 4, "         | 00112233 | 000 | ");
    test_multiple_struct_params(0,          "struct { long double d[2]; }", 5, "         | 00112233 | 020 | 4");
    test_multiple_struct_params(0,          "struct { long double d[2]; }", 6, "         | 00112233 | 040 | 4   5");
    test_multiple_struct_params(TYPE_FLOAT, "struct { long double d[2]; }", 4, "         | 0112233  | 020 | 4");
    test_multiple_struct_params(TYPE_FLOAT, "struct { long double d[2]; }", 5, "         | 0112233  | 040 | 4   5");

    // C.3 in aarch64 ABI doc
    // If the argument is an HFA or an HVA then the NSRN is set to 8 and the size of the argument is rounded up to the nearest multiple of 8 bytes.
    CallValueAllocation *cva = init_call_value_allocaton("");
    for (int i = 0; i < 7; i++) add_type_to_cva(cva, new_type(TYPE_FLOAT)); // Allocate 7 floating point registers
    add_type_to_cva(cva, parse_type_str("struct { float f[4]; }")); // Exceeds the available amount of floating point registers
    add_type_to_cva(cva, new_type(TYPE_FLOAT)); // Ends up on the stack; the 8th floating point register is not used
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "         | 0123456  | 018 | 7 8", "C.3 in aarch64 ABI doc");

    // C.13 in aarch64 ABI doc
    // preceding clauses deal with structs that can fit in integer registers.
    // The implicit else is there are no more integer registers for the struct
    // The NGRN is set to 8.
    cva = init_call_value_allocaton("");
    for (int i = 0; i < 7; i++) add_type_to_cva(cva, new_type(TYPE_INT)); // Allocate 7 integer point registers
    add_type_to_cva(cva, parse_type_str("struct { int f[4]; }")); // Exceeds the available amount of integer point registers
    add_type_to_cva(cva, new_type(TYPE_INT)); // Ends up on the stack; the 8th integer point register is not used
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "0123456  |          | 018 | 7 8", "C.13 in aarch64 ABI doc");

    // A large struct that's passed in a pointer that is also on the stack
    cva = init_call_value_allocaton("");
    for (int i = 0; i < 9; i++) add_type_to_cva(cva, new_type(TYPE_INT)); // Allocate 9 integer point registers
    add_type_to_cva(cva, parse_type_str("struct { int f[9]; }")); // Exceeds the available amount of integer point registers
    finalize_call_value_allocation(cva);
    move_indirect_stack_args(cva);
    assert_string(cva_result_str(cva),  "01234567 |          | 040 | 8", "A pointer to a struct on the stack which is also on the stack");

    // Example from x86_64 ABI doc.
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
    assert_string(cva_result_str(cva), "01223489 | 567      | 008 | a", "Example from x86_64  ABI doc v0.98");
}

void test_int128() {
    // Just P1s
    test_param_allocation(P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00       |          | 000 | ");
    test_param_allocation(P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0011     |          | 000 | ");
    test_param_allocation(P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122   |          | 000 | ");
    test_param_allocation(P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00112233 |          | 000 | ");
    test_param_allocation(P1, P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00112233 |          | 010 | 4");

    // With some integers thrown in
    test_param_allocation(P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00       |          | 000 | ");
    test_param_allocation(PI, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0 11     |          | 000 | ");
    test_param_allocation(PI, P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0 112233 |          | 010 | 4");
    test_param_allocation(P1, P1, PI, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00112 33 |          | 000 | ");
    test_param_allocation(P1, P1, P1, P1, P1, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00112233 |          | 020 | 4 5");
    test_param_allocation(P1, P1, P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00112233 |          | 020 | 4 5");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PI, P1, PI, 0,  0,  0,  0,  0,  0,  0 , "0123456  |          | 020 | 7 8");

    // Test alignment of an __int128 in a struct
    test_multiple_struct_params(TYPE_INT, "struct { __int128 i; }",   1, "0 11     |          | 000 | ");
    test_multiple_struct_params(TYPE_INT, "struct { __int128 i; }",   4, "0 112233 |          | 010 | 4");
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
