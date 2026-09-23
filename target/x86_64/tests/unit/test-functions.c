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
    test_param_allocation(PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0        |          | 000 | ");
    test_param_allocation(PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "01       |          | 000 | ");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, "012345   |          | 050 | 6789abcdef");
    test_param_allocation(PF, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 0        | 000 | ");
    test_param_allocation(PF, PF, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 01       | 000 | ");
    test_param_allocation(PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, "         | 01234567 | 040 | 89abcdef");
    test_param_allocation(PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         |          | 010 | 0");
    test_param_allocation(PL, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         |          | 020 | 0 1");
    test_param_allocation(PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, "         |          | 100 | 0 1 2 3 4 5 6 7 8 9 a b c d e f");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PF, PF, PF, PF, PF, PF, PF, PF, PI, PI, "012345   | 6789abcd | 010 | ef");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PF, PF, PF, PF, PF, PF, PF, PF, PI, PF, "012345   | 6789abcd | 010 | ef");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, "012345   | 6789abcd | 010 | ef");
    test_param_allocation(PI, PL, 0 , 0 , 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0        |          | 010 | 1");
    test_param_allocation(PI, PL, PI, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "02       |          | 020 | 1 3");
    test_param_allocation(PI, PL, PI, PL, PI, PL, PI, PL, 0,  0,  0,  0,  0,  0,  0,  0,  "0246     |          | 040 | 1 3 5 7");
    test_param_allocation(PL, PI, PL, PI, PL, PI, PL, PI, 0,  0,  0,  0,  0,  0,  0,  0,  "1357     |          | 040 | 0 2 4 6");
    test_param_allocation(PL, PL, PL, PL, PL, PL, PI, PI, PI, PI, PI, PI, 0,  0,  0,  0,  "6789ab   |          | 060 | 0 1 2 3 4 5");
    test_param_allocation(PI, PF, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0        | 1        | 010 | 2");
    test_param_allocation(PI, PF, PL, PI, PF, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "03       | 14       | 020 | 2 5");
    test_param_allocation(PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, "0369cf   | 147ad    | 050 | 2 5 8 b e");
}

void test_struct_params() {
    test_single_struct_param("struct { int a, b, c; }",                   "00       |          | 000 | ");
    test_single_struct_param("struct { int a, b; double d; }",            "0        | 0        | 000 | ");
    test_single_struct_param("struct { int a; float d; }",                "0        |          | 000 | ");
    test_single_struct_param("struct { int a, b; struct {int c, d; }; }", "00       |          | 000 | ");
    test_single_struct_param("struct { long double ld; }",                "         |          | 010 | 0");

    // Test defaulting to memory
    test_single_struct_param("struct { int i; long double ld; }", "         |          | 020 | 0");
    test_single_struct_param("struct { long i, j, k; }",          "         |          | 018 | 0");

    // Test running out of registers for struct/union
    test_multiple_struct_params(0,          "struct { int i[4]; }",   4, "001122   |          | 010 | 3");
    test_multiple_struct_params(0,          "struct { int i[4]; }",   5, "001122   |          | 020 | 3 4");
    test_multiple_struct_params(TYPE_INT,   "struct { int i[4]; }",   3, "01122    |          | 010 | 3");
    test_multiple_struct_params(TYPE_INT,   "struct { int i[4]; }",   4, "01122    |          | 020 | 3 4");
    test_multiple_struct_params(0,          "struct { float f[4]; }", 5, "         | 00112233 | 010 | 4");
    test_multiple_struct_params(0,          "struct { float f[4]; }", 6, "         | 00112233 | 020 | 4 5");
    test_multiple_struct_params(TYPE_FLOAT, "struct { float f[4]; }", 4, "         | 0112233  | 010 | 4");
    test_multiple_struct_params(TYPE_FLOAT, "struct { float f[4]; }", 5, "         | 0112233  | 020 | 4 5");

    // Unions
    test_single_struct_param("union { int i1, i2; }",     "0        |          | 000 | ");
    test_single_struct_param("union { int i; float f; }", "0        |          | 000 | ");
    test_single_struct_param("union { float f1, f2; }",   "         | 0        | 000 | ");

    // Example from x86_64 ABI doc
    CallValueAllocation *cva = init_call_value_allocaton("");
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
    assert_string(cva_result_str(cva), "012348   | 267      | 020 | 5 9a", "Example from x86_64 ABI doc v0.98");
}

void test_int128() {
    // Just P1s
    test_param_allocation(P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00       |          | 000 | ");
    test_param_allocation(P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0011     |          | 000 | ");
    test_param_allocation(P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122   |          | 000 | ");
    test_param_allocation(P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122   |          | 010 | 3");
    test_param_allocation(P1, P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122   |          | 020 | 3 4");

    // With some integers thrown in
    test_param_allocation(P1, P1, PI, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00112    |          | 010 | 3");
    test_param_allocation(P1, P1, P1, P1, P1, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122   |          | 028 | 3 4 5");

    // Test an __int128 in a struct
    test_multiple_struct_params(0,        "struct { __int128 i; }",   1, "00       |          | 000 | ");
    test_multiple_struct_params(0,        "struct { __int128 i; }",   4, "001122   |          | 010 | 3");
    test_multiple_struct_params(TYPE_INT, "struct { __int128 i; }",   1, "011      |          | 000 | ");
    test_multiple_struct_params(TYPE_INT, "struct { __int128 i; }",   4, "01122    |          | 020 | 3 4");
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
