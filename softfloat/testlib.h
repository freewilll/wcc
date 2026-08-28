#include "softfloat.h"

#define ASSERT_INT128(expected_high, expected_low, got, message) \
    assert_long(expected_high, (got) >> 64,  message " high"); \
    assert_long(expected_low, (long) (got),   message " low")

#define ASSERT_INT256(e3, e2, e1, e0, got, message) \
    assert_long(e3,        (got.high) >> 64, message " r3"); \
    assert_long(e2, (long) (got.high),       message " r2"); \
    assert_long(e1,        (got.low) >> 64,  message " r1"); \
    assert_long(e0, (long) (got.low),        message " r0");

void assert_int(int expected, int actual, char *message);
void assert_long(long expected, long actual, char *message);
void assert_float(float expected, float actual, char *message);
void assert_double(double expected, double actual, char *message);
void assert_long_double(long double expected, long double actual, char *message);
void assert_ld_string(long double ld, char *expected, char *message);

void print_fpv(FpValue *fpv);
void finish_tests();