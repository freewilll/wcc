// Shortcuts for parameters
enum {
    PI = 1, // integer
    PF = 2, // float/double
    PL = 3, // long double
    P1 = 4, // int128
};

void test_param_allocation(
        int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7,
        int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15, char *expected);
char *cva_result_str(CallValueAllocation *cva);
Type *parse_type_str(char *type_str);
void test_multiple_struct_params(int initial_var, char *type_str, int count, char *expected_cva_result_str);
void test_single_struct_param(char *type_str, char *expected_cva_result_str);
