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
    test_param_allocation(PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0        |          | 000 000 | ");
    test_param_allocation(PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "01       |          | 000 000 | ");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, "012345   |          | 050 000 | 6789abcdef");
    test_param_allocation(PF, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 0        | 000 000 | ");
    test_param_allocation(PF, PF, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 01       | 000 000 | ");
    test_param_allocation(PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, "         | 01234567 | 040 000 | 89abcdef");
    test_param_allocation(PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         |          | 010 000 | 0");
    test_param_allocation(PL, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         |          | 020 000 | 0 1");
    test_param_allocation(PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, PL, "         |          | 100 000 | 0 1 2 3 4 5 6 7 8 9 a b c d e f");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PF, PF, PF, PF, PF, PF, PF, PF, PI, PI, "012345   | 6789abcd | 010 000 | ef");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PF, PF, PF, PF, PF, PF, PF, PF, PI, PF, "012345   | 6789abcd | 010 000 | ef");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PF, PF, PF, PF, PF, PF, PF, PF, PF, PF, "012345   | 6789abcd | 010 000 | ef");
    test_param_allocation(PI, PL, 0 , 0 , 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0        |          | 010 000 | 1");
    test_param_allocation(PI, PL, PI, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "02       |          | 020 000 | 1 3");
    test_param_allocation(PI, PL, PI, PL, PI, PL, PI, PL, 0,  0,  0,  0,  0,  0,  0,  0,  "0246     |          | 040 000 | 1 3 5 7");
    test_param_allocation(PL, PI, PL, PI, PL, PI, PL, PI, 0,  0,  0,  0,  0,  0,  0,  0,  "1357     |          | 040 000 | 0 2 4 6");
    test_param_allocation(PL, PL, PL, PL, PL, PL, PI, PI, PI, PI, PI, PI, 0,  0,  0,  0,  "6789ab   |          | 060 000 | 0 1 2 3 4 5");
    test_param_allocation(PI, PF, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0        | 1        | 010 000 | 2");
    test_param_allocation(PI, PF, PL, PI, PF, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "03       | 14       | 020 000 | 2 5");
    test_param_allocation(PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, PF, PL, PI, "0369cf   | 147ad    | 050 000 | 2 5 8 b e");
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
    assert_string(cva_result_str(cva), "00       |          | 000 000 | ", sprint_type_in_english(type));

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int a, b; double d; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "0        | 0        | 000 000 | ", sprint_type_in_english(type));

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int a; float d; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "0        |          | 000 000 | ", sprint_type_in_english(type));

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int a, b; struct {int c; int d; } s; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "00       |          | 000 000 | ", sprint_type_in_english(type));

    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { long double ld; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "         |          | 010 000 | 0", sprint_type_in_english(type));
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
    assert_string(cva_result_str(cva), "012348   | 267      | 020 000 | 5 9a", "Example from ABI doc v0.98");
    assert_string("0:I2 1:S0", cvl_result_str(&(((CallValueLocations *) cva->locations->elements[2])->locations[0]), CVA_CVL(cva, 2).count), "Example from ABI doc v0.98 arg 2");

    // Test defaulting to memory
    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { int i; long double ld; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "         |          | 020 000 | 0", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 0).count, "Location counts are 1");

    // Test defaulting to memory
    cva = init_call_value_allocaton("");
    type = parse_type_str("struct { long i, j, k; }");
    add_type_to_cva(cva, type);
    finalize_call_value_allocation(cva);
    assert_string(cva_result_str(cva), "         |          | 018 000 | 0", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 0).count, "Location counts are 1");

    // Test running out of registers for struct/union
    cva = run_with_multiple_structs(0, "struct { int i[4]; }", 4);
    assert_string(cva_result_str(cva), "001122   |          | 010 000 | 3", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 3).count, "Location counts are 1");

    // Test running out of registers for struct/union
    cva = run_with_multiple_structs(0, "struct { int i[4]; }", 5);
    assert_string(cva_result_str(cva), "001122   |          | 020 000 | 3 4", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 3).count, "Location counts are 1");
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts are 1");

    cva = run_with_multiple_structs(TYPE_INT, "struct { int i[4]; }", 3);
    assert_string(cva_result_str(cva), "01122    |          | 010 000 | 3", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 3).count, "Location counts are 1");

    cva = run_with_multiple_structs(TYPE_INT, "struct { int i[4]; }", 4);
    assert_string(cva_result_str(cva), "01122    |          | 020 000 | 3 4", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 3).count, "Location counts 1");
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts 1");

    cva = run_with_multiple_structs(0, "struct { float i[4]; }", 5);
    assert_string(cva_result_str(cva), "         | 00112233 | 010 000 | 4", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts 1");

    cva = run_with_multiple_structs(0, "struct { float i[4]; }", 6);
    assert_string(cva_result_str(cva), "         | 00112233 | 020 000 | 4 5", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts 1");
    assert_int(1, CVA_CVL(cva, 5).count, "Location counts 1");

    cva = run_with_multiple_structs(TYPE_FLOAT, "struct { float i[4]; }", 4);
    assert_string(cva_result_str(cva), "         | 0112233  | 010 000 | 4", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts are 1");

    cva = run_with_multiple_structs(TYPE_FLOAT, "struct { float i[4]; }", 5);
    assert_string(cva_result_str(cva), "         | 0112233  | 020 000 | 4 5", sprint_type_in_english(type));
    assert_int(1, CVA_CVL(cva, 4).count, "Location counts 1");
    assert_int(1, CVA_CVL(cva, 5).count, "Location counts 1");
}

void test_int128() {
    test_param_allocation(P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00       |          | 000 000 | ");
    test_param_allocation(P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0011     |          | 000 000 | ");
    test_param_allocation(P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "0011     |          | 000 000 | ");
    test_param_allocation(P1, P1, PI, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "00112    |          | 010 000 | 3");
    test_param_allocation(P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122   |          | 010 000 | 3");
    test_param_allocation(P1, P1, P1, P1, P1, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122   |          | 020 000 | 3 4");
    test_param_allocation(P1, P1, P1, P1, P1, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "001122   |          | 030 008 | 3 4 5");
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
