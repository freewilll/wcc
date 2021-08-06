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
