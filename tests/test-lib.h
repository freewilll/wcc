void assert_int(int expected, int actual, char *message);
void assert_long(long expected, long actual, char *message);
#ifdef FLOATS
void assert_float(float expected, float got, char *message);
void assert_double(double expected, double got, char *message);
#endif
void assert_long_double(long double expected, long double got, char *message);
void assert_string(char *expected, char *actual, char *message);
void finalize();
void parse_args(int argc, char **argv, int *verbose);

void test_floats_function_call(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9); // fwip
void test_doubles_function_call(double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8, double d9); // fwip
