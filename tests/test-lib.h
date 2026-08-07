#define MAKE_SINT128(high, low) (((signed __int128) (high)) << 64 | (low))
#define MAKE_UINT128(high, low) (((unsigned __int128) (high)) << 64 | (low))

#define ASSERT_INT128(expected_high, expected_low, got, message) \
    assert_long(expected_high, (got) >> 64,  message " high"); \
    assert_long(expected_low, (long) (got),   message " low")

void assert_uchar(unsigned char expected, unsigned char actual, char *message);
void assert_int(int expected, int actual, char *message);
void assert_long(long expected, long actual, char *message);
int float_eq(float expected, float got);
void assert_float(float expected, float got, char *message);
void assert_double(double expected, double got, char *message);
void assert_long_double(long double expected, long double got, char *message);
void assert_string(char *expected, char *actual, char *message);
void assert_memory(char *expected, char *actual, int size, char *message);
char *write_temp_c_file(char *content);
void finalize();
void parse_args(int argc, char **argv);
int wasprintf(char **ret, const char *format, ...);
