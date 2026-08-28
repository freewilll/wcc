#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#include "softfloat.h"
#include "testlib.h"

typedef union {
    __uint128_t i;
    long double ld;
} LongDoubleUnion;

// Print a long double in binary and float format
#define PRINT_LD(v) { \
    LongDoubleUnion _u; \
    _u.ld = v; \
    printf("%016lx %016lx %Lg\n", (uint64_t) (_u.i >> 64), (uint64_t) _u.i, _u.ld); \
}

// Make a long double from a significand and an exponent
#define BUILD_LD(significand, exponent) \
    ({ \
        LongDoubleUnion _u; \
        _u.i = (__uint128_t)(significand) | ((__uint128_t)(EXPONENT_MIDWAY + (exponent)) << SIGNIFICAND_BITS); \
        _u.ld; \
    })

// Assert an addition done by the compiled code and lib is bitwise identical
#define ASSERT_LD_ADD_BINEQ_ONE_DIRECTION(a, b, m) { \
    LongDoubleUnion u1; \
    LongDoubleUnion u2; \
    u1.ld = (a) + (b); \
    u2.ld = add_ld(a, b); \
    if (u1.i != u2.i) { \
        printf("%016lx %016lx %Lg\n", (uint64_t) (u1.i >> 64), (uint64_t) u1.i, u1.ld); \
        printf("%016lx %016lx %Lg\n", (uint64_t) (u2.i >> 64), (uint64_t) u2.i, u2.ld); \
    } \
    assert_int(1, u1.i == u2.i, m); \
}

// Check a + b and b + a
#define ASSERT_LD_ADD_BINEQ(a, b, m) { \
    ASSERT_LD_ADD_BINEQ_ONE_DIRECTION(a, b, m); \
    ASSERT_LD_ADD_BINEQ_ONE_DIRECTION(b, a, m " reverse"); \
}

// Assert a subtraction done by the compiled code and lib is bitwise identical
#define ASSERT_LD_SUB_BINEQ_ONE_DIRECTION(a, b, m) { \
    LongDoubleUnion u1; \
    LongDoubleUnion u2; \
    u1.ld = (a) - (b); \
    u2.ld = subtract_ld(a, b); \
    if (u1.i != u2.i) { \
        printf("%016lx %016lx %Lg\n", (uint64_t) (u1.i >> 64), (uint64_t) u1.i, u1.ld); \
        printf("%016lx %016lx %Lg\n", (uint64_t) (u2.i >> 64), (uint64_t) u2.i, u2.ld); \
    } \
    assert_int(1, u1.i == u2.i, m); \
}

// Check a - b and b - a
#define ASSERT_LD_SUB_BINEQ(a, b, m) { \
    ASSERT_LD_SUB_BINEQ_ONE_DIRECTION(a, b, m); \
    ASSERT_LD_SUB_BINEQ_ONE_DIRECTION(b, a, m " reverse"); \
}

// Assert a multiplication done by the compiled code and lib is bitwise identical
#define ASSERT_LD_MUL_BINEQ_ONE_DIRECTION(a, b, m) { \
    LongDoubleUnion u1; \
    LongDoubleUnion u2; \
    u1.ld = (a) * (b); \
    u2.ld = multiply_ld(a, b); \
    if (u1.i != u2.i) { \
        printf("%016lx %016lx %Lg\n", (uint64_t) (u1.i >> 64), (uint64_t) u1.i, u1.ld); \
        printf("%016lx %016lx %Lg\n", (uint64_t) (u2.i >> 64), (uint64_t) u2.i, u2.ld); \
    } \
    assert_int(1, u1.i == u2.i, m); \
}

// Check a * b and b * a
#define ASSERT_LD_MUL_BINEQ(a, b, m) { \
    ASSERT_LD_MUL_BINEQ_ONE_DIRECTION(a, b, m); \
    ASSERT_LD_MUL_BINEQ_ONE_DIRECTION(b, a, m " reverse"); \
}

// Assert a division done by the compiled code and lib is bitwise identical
#define ASSERT_LD_DIV_BINEQ_ONE_DIRECTION(a, b, m) { \
    LongDoubleUnion u1; \
    LongDoubleUnion u2; \
    u1.ld = (a) / (b); \
    u2.ld = divide_ld(a, b); \
    if (u1.i != u2.i) { \
        printf("%016lx %016lx %Lg\n", (uint64_t) (u1.i >> 64), (uint64_t) u1.i, u1.ld); \
        printf("%016lx %016lx %Lg\n", (uint64_t) (u2.i >> 64), (uint64_t) u2.i, u2.ld); \
    } \
    assert_int(1, u1.i == u2.i, m); \
}

// Check a / b and b / a
#define ASSERT_LD_DIV_BINEQ(a, b, m) { \
    ASSERT_LD_DIV_BINEQ_ONE_DIRECTION(a, b, m); \
    ASSERT_LD_DIV_BINEQ_ONE_DIRECTION(b, a, m " reverse"); \
}

void test_shift_right() {
    __uint128_t v = 0;
    int guard = -42;
    int sticky = -43;
    shift_right(&v, 0, &guard, &sticky);
    assert_int(-42, guard, "shift_right >> 0 a");
    assert_int(-43, sticky, "shift_right >> 0 b");

    // 01010101
    v = 0x55;
    guard = 0;
    sticky = 0;
    shift_right(&v, 1, &guard, &sticky);
    assert_int(0x2a, v, "shift_right >> 1 a"); // 0101010
    assert_int(1, guard, "shift_right >> 1 b");
    assert_int(0, sticky, "shift_right >> 1 c");

    shift_right(&v, 1, &guard, &sticky);
    assert_int(0x15, v, "shift_right >> 2 a"); // 010101
    assert_int(0, guard, "shift_right >> 2 b");
    assert_int(1, sticky, "shift_right >> 2 c");

    shift_right(&v, 1, &guard, &sticky);
    assert_int(0x0a, v, "shift_right >> 3 a"); // 01010
    assert_int(1, guard, "shift_right >> 3 b");
    assert_int(1, sticky, "shift_right >> 3 c");

    shift_right(&v, 1, &guard, &sticky);
    assert_int(0x05, v, "shift_right >> 4 a"); // 0101
    assert_int(0, guard, "shift_right >> 4 b");
    assert_int(1, sticky, "shift_right >> 4 c");

    // 01010101
    v = 0x55;
    guard = 0;
    sticky = 0;
    shift_right(&v, 2, &guard, &sticky);
    assert_int(0x15, v, "shift_right >> 2 a"); // 010101
    assert_int(0, guard, "shift_right >> 2 b");
    assert_int(1, sticky, "shift_right >> 2 c");

    shift_right(&v, 3, &guard, &sticky);
    assert_int(0x02, v, "shift_right >> 3 a"); // 010
    assert_int(1, guard, "shift_right >> 3 b");
    assert_int(1, sticky, "shift_right >> 3 c");

    // 1000
    v = 0x08;
    guard = 1;
    sticky = 0;
    shift_right(&v, 3, &guard, &sticky);
    assert_int(0x01, v, "shift_right >> 3 a"); // 1
    assert_int(0, guard, "shift_right >> 3 b");
    assert_int(1, sticky, "shift_right >> 3 c");
}

void run_update_GRS_bits(__uint128_t value, int shift_amount, int g1, int r1, int s1, int g2, int r2, int s2, char *message) {
    int guard = g1;
    int round = r1;
    int sticky = s1;
    update_GRS_bits(&value, shift_amount, &guard, &round, &sticky);
    int got_decimal = guard * 100 + round + 10 + sticky;
    int expected_decimal = g2 * 100 + r2 + 10 + s2;
    assert_int(expected_decimal, got_decimal, message);
}

void test_update_GRS_bits() {
    run_update_GRS_bits(0, 0, 3, 4, 5, 3, 4, 5, "update_GRS_bits: 0 >> 0, no change");
    run_update_GRS_bits(0, 1, 1, 0, 0, 0, 1, 0, "update_GRS_bits: 0 >> 1, shift over R = G");
    run_update_GRS_bits(1, 1, 1, 0, 0, 1, 1, 0, "update_GRS_bits: 0 >> 1, shift over G = LSB");
    run_update_GRS_bits(0, 1, 0, 1, 0, 0, 0, 1, "update_GRS_bits: 0 >> 1, shift over S |= R");
    run_update_GRS_bits(1, 1, 0, 1, 0, 1, 0, 1, "update_GRS_bits: 0 >> 1, shift over G=LSB, S |= R");
    run_update_GRS_bits(1, 1, 1, 1, 0, 1, 1, 1, "update_GRS_bits: 0 >> 1, shift over G=LSB, R=G, S |= R");

    run_update_GRS_bits(0, 2, 1, 0, 0, 0, 0, 1, "update_GRS_bits: 0 >> 2, shift over S |= G");
    run_update_GRS_bits(0, 2, 0, 1, 0, 0, 0, 1, "update_GRS_bits: 0 >> 2, shift over S |= R");
    run_update_GRS_bits(1, 2, 0, 0, 0, 0, 1, 0, "update_GRS_bits: 0 >> 2, shift over R = LSB");
    run_update_GRS_bits(2, 2, 0, 0, 0, 1, 0, 0, "update_GRS_bits: 0 >> 2, shift over G = LSB1"); // LSB1 is 1 bit left of LSB
    run_update_GRS_bits(3, 2, 0, 0, 0, 1, 1, 0, "update_GRS_bits: 0 >> 2, shift over G = LSB1, r = LSB");
    run_update_GRS_bits(0, 2, 1, 0, 0, 0, 0, 1, "update_GRS_bits: 0 >> 2, shift over S |= G");
    run_update_GRS_bits(0, 2, 0, 1, 0, 0, 0, 1, "update_GRS_bits: 0 >> 2, shift over S |= R");

    run_update_GRS_bits(0, 3, 0, 0, 0, 0, 0, 0, "update_GRS_bits: 0 >> 3, no change");
    run_update_GRS_bits(1, 3, 0, 0, 0, 0, 0, 1, "update_GRS_bits: 0 >> 3, S |= bit");
    run_update_GRS_bits(1, 4, 0, 0, 0, 0, 0, 1, "update_GRS_bits: 0 >> 4, S |= bit");
    run_update_GRS_bits(2, 4, 0, 0, 0, 0, 0, 1, "update_GRS_bits: 0 >> 4, S |= bit");
    run_update_GRS_bits(3, 4, 0, 0, 0, 0, 0, 1, "update_GRS_bits: 0 >> 4, S |= bit");
}

void test_multiply_256_bit() {
    __uint128_t a, b;

    a = 3;
    b = 4;
    ASSERT_INT256(
        0x0000000000000000,
        0x0000000000000000,
        0x0000000000000000,
        0x000000000000000c,
        multiply_256_bit(a, b),
        "256 bit multiply 3 * 4"
    );

    a = (((__uint128_t) 3) << 64) | 4;
    b = (((__uint128_t) 5) << 64) | 6;
    ASSERT_INT256(
        0x0000000000000000,
        0x000000000000000f,
        0x0000000000000026,
        0x0000000000000018,
        multiply_256_bit(a, b),
        "256 bit multiply 3|4 * 5|6"
    );

    a = 0xffffffffffffffff;
    b = 0xffffffffffffffff;
    ASSERT_INT256(
        0x0000000000000000,
        0x0000000000000000,
        0xfffffffffffffffe,
        0x0000000000000001,
        multiply_256_bit(a, b),
        "256 bit multiply 0|f..f * 0|f..f"
    );

    a = -1;
    b = -1;
    ASSERT_INT256(
        0xffffffffffffffff,
        0xfffffffffffffffe,
        0x0000000000000000,
        0x0000000000000001,
        multiply_256_bit(a, b),
        "256 bit multiply f..f * f..f"
    );

    a = ((__uint128_t) 0x0123456789abcdefL << 64) | 0x0123456789abcdefL;
    b = ((__uint128_t) 0xfedcba9876543210L << 64) | 0xfedcba9876543210L;
    ASSERT_INT256(
        0x0121fa00ad77d742,
        0x247acc9140513b74,
        0x458fab20783af122,
        0x2236d88fe5618cf0,
        multiply_256_bit(a, b),
        "256 bit multiply seq * seq"
    );
}

// Test that loading and storing a float produces the exact same binary result.
void test_float_roundtrip() {
    float f;
    FpValue fpv;
    uint32_t before, after;

    // Run the conversion and assert the binary representation is identical
    #define TEST_FLOAT_ROUNDTRIP(v, m) \
        f = v; \
        before = *((__uint32_t *) &f); \
        fpv = load_float(f); \
        f = store_float(&fpv); \
        after = *((__uint32_t *) &f); \
        assert_int(1, before == after, m); \

    TEST_FLOAT_ROUNDTRIP(+0.0,            "Float roundtrip: +0.0");
    TEST_FLOAT_ROUNDTRIP(-0.0,            "Float roundtrip: -0.0");
    TEST_FLOAT_ROUNDTRIP(+INFINITY,       "Float roundtrip: +inf");
    TEST_FLOAT_ROUNDTRIP(-INFINITY,       "Float roundtrip: -inf");
    TEST_FLOAT_ROUNDTRIP(NAN,             "Float roundtrip: +nan");
    TEST_FLOAT_ROUNDTRIP(-NAN,            "Float roundtrip: -nan");
    TEST_FLOAT_ROUNDTRIP(-1.12345f,       "Float roundtrip: -1.12345");
    TEST_FLOAT_ROUNDTRIP(3.40282347e+38f, "Float roundtrip: max normal");
    TEST_FLOAT_ROUNDTRIP(1.17549500e-38f, "Float roundtrip: just above min normal");
    TEST_FLOAT_ROUNDTRIP(1.17549435e-38f, "Float roundtrip: min normal");
    TEST_FLOAT_ROUNDTRIP(1.17549421e-38f, "Float roundtrip: max subnormal");
    TEST_FLOAT_ROUNDTRIP(1e-40f,          "Float roundtrip: somewhere halfway subnormal");
    TEST_FLOAT_ROUNDTRIP(1.40129846e-45f, "Float roundtrip: min subnormal");
}

// Test that loading and storing a double produces the exact same binary result.
void test_double_roundtrip() {
    double d;
    FpValue fpv;
    uint64_t before, after;

    // Run the conversion and assert the binary representation is identical
    #define TEST_DOUBLE_ROUNDTRIP(v, m) \
        d = v; \
        before = *((uint64_t *) &d); \
        fpv = load_double(d); \
        d = store_double(&fpv); \
        after = *((uint64_t *) &d); \
        assert_long(1, before == after, m); \

    TEST_DOUBLE_ROUNDTRIP(+0.0,                    "Double roundtrip: +0.0");
    TEST_DOUBLE_ROUNDTRIP(-0.0,                    "Double roundtrip: -0.0");
    TEST_DOUBLE_ROUNDTRIP(+INFINITY,               "Double roundtrip: +inf");
    TEST_DOUBLE_ROUNDTRIP(-INFINITY,               "Double roundtrip: -inf");
    TEST_DOUBLE_ROUNDTRIP(NAN,                     "Double roundtrip: +nan");
    TEST_DOUBLE_ROUNDTRIP(-NAN,                    "Double roundtrip: -nan");
    TEST_DOUBLE_ROUNDTRIP(-1.12345f,               "Double roundtrip: -1.12345");
    TEST_DOUBLE_ROUNDTRIP(1.7976931348623157e+308, "Double roundtrip: max normal");
    TEST_DOUBLE_ROUNDTRIP(2.2250738585073014e-308, "Double roundtrip: just above min normal");
    TEST_DOUBLE_ROUNDTRIP(2.2250738585072014e-308, "Double roundtrip: min normal");
    TEST_DOUBLE_ROUNDTRIP(2.2250738585072009e-308, "Double roundtrip: max subnormal");
    TEST_DOUBLE_ROUNDTRIP(1e-312,                  "Double roundtrip: somewhere halfway subnormal");
    TEST_DOUBLE_ROUNDTRIP(4.9406564584124654e-324, "Double roundtrip: min subnormal");
}

// Test that loading and storing a long double produces the exact same binary result.
void test_ld_roundtrip() {
    long double ld;
    FpValue fpv;
    __uint128_t before, after;

    // Run the conversion and assert the binary representation is identical
    #define TEST_LD_ROUNDTRIP(v, m) \
        ld = v; \
        before = *((__uint128_t *) &ld); \
        fpv = load_ld(ld); \
        ld = store_ld(&fpv); \
        after = *((__uint128_t *) &ld); \
        assert_long(1, before == after, m); \

    TEST_LD_ROUNDTRIP(+0.0,                                        "Long double roundtrip: +0.0");
    TEST_LD_ROUNDTRIP(-0.0,                                        "Long double roundtrip: -0.0");
    TEST_LD_ROUNDTRIP(+INFINITY,                                   "Long double roundtrip: +inf");
    TEST_LD_ROUNDTRIP(-INFINITY,                                   "Long double roundtrip: -inf");
    TEST_LD_ROUNDTRIP(NAN,                                         "Long double roundtrip: +nan");
    TEST_LD_ROUNDTRIP(-NAN,                                        "Long double roundtrip: -nan");
    TEST_LD_ROUNDTRIP(-1.12345f,                                   "Long double roundtrip: -1.12345");
    TEST_LD_ROUNDTRIP(1.1897314953572317650857593266280070e+4932L, "Long double roundtrip: max normal");
    TEST_LD_ROUNDTRIP(3.3621031431120935062626778173218526e-4932L, "Long double roundtrip: just above min normal");
    TEST_LD_ROUNDTRIP(3.3621031431120935062626778173217526e-4932L, "Long double roundtrip: min normal");
    TEST_LD_ROUNDTRIP(3.3621031431120935062626778173217520e-4932L, "Long double roundtrip: max subnormal");
    TEST_LD_ROUNDTRIP(1e-4940L,                                    "Long double roundtrip: somewhere halfway subnormal");
    TEST_LD_ROUNDTRIP(6.4751751194380251109244389582276466e-4966L, "Long double roundtrip: min subnormal");
}

void test_convert_float_to_double() {
    float f = -1.12345;
    double d = convert_float_to_double(f);
    assert_double(f, d, "Conversion: float to double");
}

void test_convert_float_to_ld() {
    float f = -1.12345;
    long double ld = convert_float_to_ld(f);
    assert_long_double(f, ld, "Conversion: float to LD");
}

void test_convert_double_to_float() {
    double d = -1.12345;
    float f = convert_double_to_float(d);
    assert_long_double(d, f, "Conversion: double to float");
}

void test_convert_double_to_ld() {
    double d = -1.12345;
    long double ld = convert_double_to_ld(d);
    assert_long_double(d, ld, "Conversion: double to LD");
}

void test_convert_ld_to_float() {
    #define TEST_LD_TO_FLOAT(v, e, m) f = convert_ld_to_float(v); assert_ld_string(f, e, m)

    float f;

    TEST_LD_TO_FLOAT(+0.0,           "0.00000000e+00",  "Conversion of LD to float: +0.0");
    TEST_LD_TO_FLOAT(-0.0,           "-0.00000000e+00", "Conversion of LD to float: -0.0");
    TEST_LD_TO_FLOAT(1e+39,          "inf",             "Conversion of LD to float: +inf");
    TEST_LD_TO_FLOAT(-1e+39,         "-inf",            "Conversion of LD to float: -inf");
    TEST_LD_TO_FLOAT(NAN,            "nan",             "Conversion of LD to float: +nan");
    TEST_LD_TO_FLOAT(-NAN,           "-nan",            "Conversion of LD to float: -nan");
    TEST_LD_TO_FLOAT(-1.12345,       "-1.12345004e+00", "Conversion of LD to float: -1.2345");
    TEST_LD_TO_FLOAT(3.40282347e+38, "3.40282347e+38",  "Conversion of LD to float: max normal");
    TEST_LD_TO_FLOAT(1.17549500e-38, "1.17549505e-38",  "Conversion of LD to float: just above min normal");
    TEST_LD_TO_FLOAT(1.17549435e-38, "1.17549435e-38",  "Conversion of LD to float: min normal");
    TEST_LD_TO_FLOAT(1.17549421e-38,  "1.17549421e-38", "Conversion of LD to float: max subnormal");
    TEST_LD_TO_FLOAT(1.40129846e-45, "1.40129846e-45",  "Conversion of LD to float: min subnormal");
    TEST_LD_TO_FLOAT(1e-40,          "9.99994610e-41",  "Conversion of LD to float: somewhere halfway subnormal");
    TEST_LD_TO_FLOAT(1e-46,          "0.00000000e+00",  "Conversion of LD to float: below min subnormal +0.0");
    TEST_LD_TO_FLOAT(-1e-46,         "-0.00000000e+00", "Conversion of LD to float: below min subnormal -0.0");
}

void test_convert_ld_to_double() {
    #define TEST_LD_TO_DOUBLE(v, e, m) d = convert_ld_to_double(v); assert_ld_string(d, e, m)

    double d;

    TEST_LD_TO_DOUBLE(+0.0,                     "0.00000000e+00",   "Conversion of LD to double: +0.0");
    TEST_LD_TO_DOUBLE(-0.0,                     "-0.00000000e+00",  "Conversion of LD to double: -0.0");
    TEST_LD_TO_DOUBLE(1e+310L,                  "inf",              "Conversion of LD to double: +inf");
    TEST_LD_TO_DOUBLE(-1e+310L,                 "-inf",             "Conversion of LD to double: -inf");
    TEST_LD_TO_DOUBLE(NAN,                      "nan",              "Conversion of LD to double: +nan");
    TEST_LD_TO_DOUBLE(-NAN,                     "-nan",             "Conversion of LD to double: -nan");
    TEST_LD_TO_DOUBLE(-1.12345,                 "-1.12345000e+00",  "Conversion of LD to double: -1.2345");
    TEST_LD_TO_DOUBLE(1.7976931348623157e+308L, "1.79769313e+308",  "Conversion of LD to double: max normal");
    TEST_LD_TO_DOUBLE(2.2250738585073014e-308L, "2.22507386e-308",  "Conversion of LD to double: just above min normal");
    TEST_LD_TO_DOUBLE(2.2250738585072014e-308L, "2.22507386e-308",  "Conversion of LD to double: min normal");
    TEST_LD_TO_DOUBLE(2.2250738585072009e-308L, "2.22507386e-308",  "Conversion of LD to double: max subnormal");
    TEST_LD_TO_DOUBLE(1e-312L,                  "1.00000000e-312",  "Conversion of LD to double: somewhere halfway subnormal");
    TEST_LD_TO_DOUBLE(4.9406564584124654e-324L, "4.94065646e-324",  "Conversion of LD to double: min subnormal");
    TEST_LD_TO_DOUBLE(1e-325L,                  "0.00000000e+00",  "Conversion of LD to double: below min subnormal +0.0");
    TEST_LD_TO_DOUBLE(-1e-325L,                 "-0.00000000e+00", "Conversion of LD to double: below min subnormal -0.0");
}

void test_convert_fp_to_fp_nan_and_inf() {
    // Infinity
    assert_double(INFINITY,       convert_float_to_double(INFINITY),   "Conversion: float to double +inf");
    assert_long_double(INFINITY,  convert_float_to_ld(INFINITY),       "Conversion: float to LD +inf");
    assert_float(INFINITY,        convert_double_to_float(INFINITY),   "Conversion: double to float +inf");
    assert_long_double(INFINITY,  convert_double_to_ld(INFINITY),      "Conversion: double to LD +inf");
    assert_float(INFINITY,        convert_ld_to_float(INFINITY),       "Conversion: LD to LD +inf");
    assert_double(INFINITY,       convert_ld_to_double(INFINITY),      "Conversion: LD to double +inf");

    assert_double(-INFINITY,      convert_float_to_double(-INFINITY),  "Conversion: float to double -inf");
    assert_long_double(-INFINITY, convert_float_to_ld(-INFINITY),      "Conversion: float to LD -inf");
    assert_float(-INFINITY,       convert_double_to_float(-INFINITY),  "Conversion: double to float -inf");
    assert_long_double(-INFINITY, convert_double_to_ld(-INFINITY),     "Conversion: double to LD -inf");
    assert_float(-INFINITY,       convert_ld_to_float(-INFINITY),      "Conversion: LD to LD -inf");
    assert_double(-INFINITY,      convert_ld_to_double(-INFINITY),     "Conversion: LD to  double -inf");

    // NAN
    assert_double(NAN,      convert_float_to_double(NAN), "Conversion: float to double NAN");
    assert_long_double(NAN, convert_float_to_ld(NAN),     "Conversion: float to LD NAN");
    assert_float(NAN,       convert_double_to_float(NAN), "Conversion: double to float NAN");
    assert_long_double(NAN, convert_double_to_ld(NAN),    "Conversion: double to LD NAN");
    assert_float(NAN,       convert_ld_to_float(NAN),     "Conversion: LD to LD NAN");
    assert_double(NAN,      convert_ld_to_double(NAN),    "Conversion: LD to double NAN");

    assert_double(-NAN,      convert_float_to_double(-NAN), "Conversion: float to double -NAN");
    assert_long_double(-NAN, convert_float_to_ld(-NAN),     "Conversion: float to LD -NAN");
    assert_float(-NAN,       convert_double_to_float(-NAN), "Conversion: double to float -NAN");
    assert_long_double(-NAN, convert_double_to_ld(-NAN),    "Conversion: double to LD -NAN");
    assert_float(-NAN,       convert_ld_to_float(-NAN),     "Conversion: LD to LD -NAN");
    assert_double(-NAN,      convert_ld_to_double(-NAN),    "Conversion: LD to double -NAN");
}

#define TEST_ROUNDING(t1, t2, g, s1, s2, et1, et2) \
    fpv.significand = 0; \
    if (t1) SET_SBIT(fpv.significand, binary32_encoding.significand_bits - 2); \
    if (t2) SET_SBIT(fpv.significand, binary32_encoding.significand_bits - 1); \
    if (g) SET_SBIT(fpv.significand, binary32_encoding.significand_bits); \
    if (s1) SET_SBIT(fpv.significand, binary32_encoding.significand_bits + 1); \
    if (s2) SET_SBIT(fpv.significand, binary32_encoding.significand_bits + 2); \
    round_to_nearest_even_at_bits(binary32_encoding, &fpv, binary32_encoding.significand_bits); \
    assert_int(et1, GET_SBIT(fpv.significand, binary32_encoding.significand_bits - 2), "Rounding test " # t1 # t2 # g # s1 # s2); \
    assert_int(et2, GET_SBIT(fpv.significand, binary32_encoding.significand_bits - 1), "Rounding test " # t1 # t2 # g # s1 # s2);

// Test a couple of cases of round to even using the float digits boundary
void test_rounding() {
    FpValue fpv = {0};

    // Bits: top 1, top 2, guard, sticky1, sticky2, expected top 1, expected top 2
    //            t1 t2 g  s1 s2 t1 t2
    TEST_ROUNDING(0, 0, 0, 0, 0, 0, 0); // Exact, no change
    TEST_ROUNDING(0, 0, 1, 0, 0, 0, 0); // Exact, T is already even
    TEST_ROUNDING(0, 0, 1, 1, 0, 0, 1); // Inexact, above half, round up
    TEST_ROUNDING(0, 1, 0, 0, 0, 0, 1); // Exact, no change
    TEST_ROUNDING(0, 1, 0, 1, 0, 0, 1); // Inexact, below half, round down
    TEST_ROUNDING(0, 1, 1, 0, 0, 1, 0); // Exact, T is odd, round up
    TEST_ROUNDING(0, 1, 1, 1, 0, 1, 0); // Inexact, above half, round up

    // With the other sticky bit set
    //            t1 t2 g  s1 s2 t1 t2
    TEST_ROUNDING(0, 0, 0, 0, 0, 0, 0); // Exact, no change
    TEST_ROUNDING(0, 0, 1, 0, 0, 0, 0); // Exact, T is already even
    TEST_ROUNDING(0, 0, 1, 0, 1, 0, 1); // Inexact, above half, round up
    TEST_ROUNDING(0, 1, 0, 0, 0, 0, 1); // Exact, no change
    TEST_ROUNDING(0, 1, 0, 0, 1, 0, 1); // Inexact, below half, round down
    TEST_ROUNDING(0, 1, 1, 0, 0, 1, 0); // Exact, T is odd, round up
    TEST_ROUNDING(0, 1, 1, 0, 1, 1, 0); // Inexact, above half, round up

    // Edge case of a round up with the significand consisting of all ones
    fpv.significand = -1; // This is 1.111111....11111 * 2^0, which is almost 2.00000
    float f = store_float(&fpv);
    assert_int(1, fpv.exponent, "Rounding up with overflow 1");
    assert_int(1, fpv.significand == 0, "Rounding up with overflow 2");
    assert_float(2.0, f, "Rounding up with overflow 3");
    round_to_nearest_even_at_bits(binary32_encoding, &fpv, binary32_encoding.significand_bits);
    f = store_float(&fpv);
    assert_float(2.0, f, "Rounding up with overflow 4");
    assert_int(1, fpv.exponent, "Rounding up with overflow 5");
    assert_int(0, fpv.significand, "Rounding up with overflow 6");
}

void test_convert_ld_to_int64() {
    assert_long(0,              convert_ld_to_int64(0.0L),                    "ld to int64: +0.0");
    assert_long(0,              convert_ld_to_int64(-0.0L),                   "ld to int64: -0.0");
    assert_long(INT64_MAX,      convert_ld_to_int64(INFINITY),                "ld to int64: +inf");
    assert_long(INT64_MIN,      convert_ld_to_int64(-INFINITY),               "ld to int64: -inf");
    assert_long(INT64_MAX,      convert_ld_to_int64(NAN),                     "ld to int64: +nan");
    assert_long(0,              convert_ld_to_int64(-NAN),                    "ld to int64: -nan");
    assert_long(INT64_MIN,      convert_ld_to_int64(-9223372036854775809.0L), "ld to int64: INT64_MIN - 1");
    assert_long(INT64_MIN,      convert_ld_to_int64(-9223372036854775808.0L), "ld to int64: INT64_MIN");
    assert_long(INT64_MIN + 1,  convert_ld_to_int64(-9223372036854775807.0L), "ld to int64: INT64_MIN + 1");
    assert_long(-100000000,     convert_ld_to_int64(-100000000.1L),           "ld to int64: -100000000.1");
    assert_long(-100,           convert_ld_to_int64(-100.1L),                 "ld to int64: -100.1");
    assert_long(-10,            convert_ld_to_int64(-10.1L),                  "ld to int64: -10.1");
    assert_long(-1,             convert_ld_to_int64(-1.1L),                   "ld to int64: -1.1");
    assert_long(INT64_MIN,      convert_ld_to_int64(-1e100L),                 "ld to int64: -1e100");
    assert_long(0,              convert_ld_to_int64(-0.1L),                   "ld to int64: -0.1");
    assert_long(0,              convert_ld_to_int64(0.1L),                    "ld to int64: 0.1");
    assert_long(1,              convert_ld_to_int64(1.1L),                    "ld to int64: 1.1");
    assert_long(10,             convert_ld_to_int64(10.1L),                   "ld to int64: 10.1");
    assert_long(100,            convert_ld_to_int64(100.1L),                  "ld to int64: 100.1");
    assert_long(100000000,      convert_ld_to_int64(100000000.1L),            "ld to int64: 100000000.1");
    assert_long(INT64_MAX - 1,  convert_ld_to_int64(9223372036854775806.0L),  "ld to int64: INT64_MAX - 1");
    assert_long(INT64_MAX,      convert_ld_to_int64(9223372036854775807.0L),  "ld to int64: INT64_MAX");
}

void test_convert_ld_to_uint64() {
    assert_long(0,              convert_ld_to_uint64(0.0L),                    "ld to uint64: +0.0");
    assert_long(0,              convert_ld_to_uint64(-0.0L),                   "ld to uint64: -0.0");
    assert_long(UINT64_MAX,     convert_ld_to_uint64(INFINITY),                "ld to uint64: +inf");
    assert_long(0,              convert_ld_to_uint64(-INFINITY),               "ld to uint64: -inf");
    assert_long(UINT64_MAX,     convert_ld_to_uint64(NAN),                     "ld to uint64: nan");
    assert_long(0,              convert_ld_to_uint64(-NAN),                    "ld to uint64: nan");
    assert_long(0,              convert_ld_to_uint64(-1e100L),                 "ld to uint64: -1e100");
    assert_long(0,              convert_ld_to_uint64(-0.1L),                   "ld to uint64: -0.1");
    assert_long(0,              convert_ld_to_uint64(0.1L),                    "ld to uint64: 0.1");
    assert_long(1,              convert_ld_to_uint64(1.1L),                    "ld to uint64: 1.1");
    assert_long(10,             convert_ld_to_uint64(10.1L),                   "ld to uint64: 10.1");
    assert_long(100,            convert_ld_to_uint64(100.1L),                  "ld to uint64: 100.1");
    assert_long(100000000,      convert_ld_to_uint64(100000000.1L),            "ld to uint64: 100000000.1");
    assert_long(UINT64_MAX - 1, convert_ld_to_uint64(18446744073709551614.0L), "ld to uint64: UINT64_MAX - 1");
    assert_long(UINT64_MAX,     convert_ld_to_uint64(18446744073709551615.0L), "ld to uint64: UINT64_MAX");
    assert_long(UINT64_MAX,     convert_ld_to_uint64(18446744073709551616.0L), "ld to uint64: UINT64_MAX + 1");
}

void test_convert_int64_to_ld() {
    assert_long_double(-9223372036854775808.000000L,  convert_int64_to_ld(INT64_MIN),      "int64 to ld: INT64_MIN");
    assert_long_double(-9223372036854775807.000000L,  convert_int64_to_ld(INT64_MIN + 1),  "int64 to ld: INT64_MIN - 1");
    assert_long_double(-100000000.0,                  convert_int64_to_ld(-100000000),     "int64 to ld: -100000000.0");
    assert_long_double(-100.0,                        convert_int64_to_ld(-100),           "int64 to ld: -100.0");
    assert_long_double(-10.0,                         convert_int64_to_ld(-10),            "int64 to ld: -10.0");
    assert_long_double(-1.0,                          convert_int64_to_ld(-1),             "int64 to ld: -1.0");
    assert_long_double(0.0,                           convert_int64_to_ld(0),              "int64 to ld: 0.0");
    assert_long_double(1.0,                           convert_int64_to_ld(1),              "int64 to ld: 1.0");
    assert_long_double(10.0,                          convert_int64_to_ld(10),             "int64 to ld: 10.0");
    assert_long_double(100.0,                         convert_int64_to_ld(100),            "int64 to ld: 100.0");
    assert_long_double(100000000.0,                   convert_int64_to_ld(100000000),      "int64 to ld: 100000000.0");
    assert_long_double(9223372036854775806.000000L,   convert_int64_to_ld(INT64_MAX - 1),  "int64 to ld: INT64_MAX - 1");
    assert_long_double(9223372036854775807.000000L,   convert_int64_to_ld(INT64_MAX),      "int64 to ld: INT64_MAX");
    assert_long_double(-2.0,                          convert_int64_to_ld(UINT64_MAX - 1), "int64 to ld: UINT64_MAX - 1");
    assert_long_double(-1.0,                          convert_int64_to_ld(UINT64_MAX),     "int64 to ld: UINT64_MAX");
}

void test_convert_uint64_to_ld() {
    assert_long_double(0.0,                     convert_uint64_to_ld(0),              "uint64 to ld: 0.0");
    assert_long_double(1.0,                     convert_uint64_to_ld(1),              "uint64 to ld: 1.0");
    assert_long_double(10.0,                    convert_uint64_to_ld(10),             "uint64 to ld: 10.0");
    assert_long_double(100.0,                   convert_uint64_to_ld(100),            "uint64 to ld: 100.0");
    assert_long_double(100000000.0,             convert_uint64_to_ld(100000000),      "uint64 to ld: 100000000.0");
    assert_long_double(18446744073709551614.0L, convert_uint64_to_ld(UINT64_MAX - 1), "uint64 to ld: UINT64_MAX - 1");
    assert_long_double(18446744073709551615.0L, convert_uint64_to_ld(UINT64_MAX),     "uint64 to ld: UINT64_MAX");
}

void test_negate_ld() {
    assert_long_double(-0,        negate_ld(0),         "negate +0");
    assert_long_double(0,         negate_ld(-0),        "negate -0");
    assert_long_double(-INFINITY, negate_ld(INFINITY),  "negate +inf");
    assert_long_double(INFINITY,  negate_ld(-INFINITY), "negate -inf");
    assert_long_double(-NAN,      negate_ld(NAN),       "negate +nan");
    assert_long_double(NAN,       negate_ld(-NAN),      "negate -nan");
    assert_long_double(-1,        negate_ld(1),         "negate 1");
    assert_long_double(1,         negate_ld(-1),        "negate -1");
}

void test_add_ld() {
    #define TEST_ADD_LD(a, b, e, m) { long double ld = add_ld(a, b); assert_ld_string(ld, e, m); }

    // Zero, infinity, nan
    TEST_ADD_LD(0.0L,   0.0L,  "0.00000000e+00", "+0.0 + +0.0");
    TEST_ADD_LD(0.0L,  -0.0L,  "0.00000000e+00", "+0.0 + -0.0");
    TEST_ADD_LD(-0.0L,  0.0L,  "0.00000000e+00", "-0.0 + +0.0");
    TEST_ADD_LD(-0.0L, -0.0L, "-0.00000000e+00", "-0.0 + -0.0");

    TEST_ADD_LD( 0.0L,  INFINITY,  "inf", "+0.0 + +inf");
    TEST_ADD_LD( 0.0L, -INFINITY, "-inf", "+0.0 + -inf");
    TEST_ADD_LD(-0.0L,  INFINITY,  "inf", "-0.0 + +inf");
    TEST_ADD_LD(-0.0L, -INFINITY, "-inf", "-0.0 + -inf");

    TEST_ADD_LD( INFINITY,  0.0L,  "inf", "+inf + +0.0");
    TEST_ADD_LD(-INFINITY,  0.0L, "-inf", "-inf + +0.0");
    TEST_ADD_LD( INFINITY, -0.0L,  "inf", "+inf + -0.0");
    TEST_ADD_LD(-INFINITY, -0.0L, "-inf", "-inf + -0.0");

    TEST_ADD_LD( 0.0L,  NAN,  "nan", "+0.0 + +nan");
    TEST_ADD_LD( 0.0L, -NAN, "-nan", "+0.0 + -nan");
    TEST_ADD_LD(-0.0L,  NAN,  "nan", "-0.0 + +nan");
    TEST_ADD_LD(-0.0L, -NAN, "-nan", "-0.0 + -nan");

    TEST_ADD_LD( NAN,  0.0L,  "nan", "+nan + +0.0");
    TEST_ADD_LD(-NAN,  0.0L, "-nan", "-nan + +0.0");
    TEST_ADD_LD( NAN, -0.0L,  "nan", "+nan + -0.0");
    TEST_ADD_LD(-NAN, -0.0L, "-nan", "-nan + -0.0");

    TEST_ADD_LD( 1.0L,  1.5L,  "2.50000000e+00", " 1.0 +  1.5");
    TEST_ADD_LD( 1.1L,  1.5L,  "2.60000000e+00", " 1.1 +  1.5");
    TEST_ADD_LD( 1.0L,  2.0L,  "3.00000000e+00", " 1.0 +  2.0");
    TEST_ADD_LD( 2.0L,  1.0L,  "3.00000000e+00", " 2.0 +  1.0");
    TEST_ADD_LD(-2.0L, -1.0L, "-3.00000000e+00", "-2.0 + -1.0");

    TEST_ADD_LD(10.12345L, 100.11111L, "1.10234560e+02", "10.12345 + 100.11111");
    TEST_ADD_LD(1.0e1L,    1.0e9L,     "1.00000001e+09", "1.0e1 + 1.0e9");

    // Create a bunch of convenient constants
    long double TWO_EXP_MIN_112 = BUILD_LD(0, -112); // 2^-112
    long double TWO_EXP_MIN_113 = BUILD_LD(0, -113); // 2^-113
    long double TWO_EXP_MIN_114 = BUILD_LD(0, -114); // 2^-114
    long double TWO_EXP_MIN_225 = BUILD_LD(0, -225); // 2^-225
    long double TWO_EXP_MIN_226 = BUILD_LD(0, -226); // 2^-226
    long double SMALLEST_NORMAL = BUILD_LD(0 , 1 - EXPONENT_MIDWAY);
    long double LARGEST_NORMAL = BUILD_LD(((__uint128_t) 1 << 112) - 1, EXPONENT_MIDWAY);
    long double LARGEST_SUBNORMAL = BUILD_LD(((__uint128_t) 1 << 112) - 1, -EXPONENT_MIDWAY);
    long double SMALLEST_SUBNORMAL = BUILD_LD(1, -EXPONENT_MIDWAY);

    // Rounding
    ASSERT_LD_ADD_BINEQ(2 - TWO_EXP_MIN_112, TWO_EXP_MIN_112, "(2 - 2^-112) +  2^-112");
    ASSERT_LD_ADD_BINEQ(1, TWO_EXP_MIN_112, "1 + 2^-112");
    ASSERT_LD_ADD_BINEQ(1, TWO_EXP_MIN_113, "1 + 2^-113"); // T=0, g=1, s=0, rounds down
    ASSERT_LD_ADD_BINEQ(1 + TWO_EXP_MIN_112, TWO_EXP_MIN_113, "(1 + 2^-112) + 2^-113"); // T=1, g=1, s=0, rounds up
    ASSERT_LD_ADD_BINEQ(1, TWO_EXP_MIN_113 + TWO_EXP_MIN_114, "1 + (2^-113 + 2^-114)"); // T=0, g=1, s=1, rounds up
    ASSERT_LD_ADD_BINEQ(1, TWO_EXP_MIN_113 + TWO_EXP_MIN_225, "1 + (2^-113 + 2^-225)"); // T=0, g=1, s=1, rounds up
    ASSERT_LD_ADD_BINEQ(1, TWO_EXP_MIN_113 - TWO_EXP_MIN_226, "1 + (2^-113 - 2^-226)"); // T=0, g=0, s=1, no bits participate due to the large shift
    ASSERT_LD_ADD_BINEQ(2 - TWO_EXP_MIN_112, 2 - TWO_EXP_MIN_112, "(2 - 2^-112) + (2 - 2^-112)"); // Overflow and normalization

    // Subnormals
    ASSERT_LD_ADD_BINEQ(SMALLEST_NORMAL, SMALLEST_SUBNORMAL, "smallest normal + smallest subnormal");
    ASSERT_LD_ADD_BINEQ(SMALLEST_SUBNORMAL, SMALLEST_SUBNORMAL, "smallest subnormal + smallest subnormal");
    ASSERT_LD_ADD_BINEQ(LARGEST_SUBNORMAL, SMALLEST_SUBNORMAL, "largest subnormal + smallest subnormal");

    // Infinity
    ASSERT_LD_ADD_BINEQ(LARGEST_NORMAL, BUILD_LD(0, EXPONENT_MIDWAY - 112), "largest normal + just enough = infinity"); // Infinity
    ASSERT_LD_ADD_BINEQ(LARGEST_NORMAL, LARGEST_NORMAL, "largest normal + largest normal = infinity");
    ASSERT_LD_ADD_BINEQ(-LARGEST_NORMAL, -LARGEST_NORMAL, "-largest normal + -largest normal = -infinity");
}

void test_subtract_ld() {
    #define TEST_SUB_LD(a, b, e, m) { long double ld = subtract_ld(a, b); assert_ld_string(ld, e, m); }

    // Zero, infinity, nan
    TEST_SUB_LD(0.0L,   0.0L,  "0.00000000e+00", "+0.0 - +0.0");
    TEST_SUB_LD(0.0L,  -0.0L,  "0.00000000e+00", "+0.0 - -0.0");
    TEST_SUB_LD(-0.0L,  0.0L, "-0.00000000e+00", "-0.0 - +0.0");
    TEST_SUB_LD(-0.0L, -0.0L,  "0.00000000e+00", "-0.0 - -0.0");

    TEST_SUB_LD( 0.0L,  INFINITY, "-inf", "+0.0 - +inf");
    TEST_SUB_LD( 0.0L, -INFINITY,  "inf", "+0.0 - -inf");
    TEST_SUB_LD(-0.0L,  INFINITY, "-inf", "-0.0 - +inf");
    TEST_SUB_LD(-0.0L, -INFINITY,  "inf", "-0.0 - -inf");

    TEST_SUB_LD( INFINITY,  0.0L,  "inf", "+inf - +0.0");
    TEST_SUB_LD(-INFINITY,  0.0L, "-inf", "-inf - +0.0");
    TEST_SUB_LD( INFINITY, -0.0L,  "inf", "+inf - -0.0");
    TEST_SUB_LD(-INFINITY, -0.0L, "-inf", "-inf - -0.0");

    TEST_SUB_LD( 0.0L,  NAN,  "nan", "+0.0 - +nan");
    TEST_SUB_LD( 0.0L, -NAN, "-nan", "+0.0 - -nan");
    TEST_SUB_LD(-0.0L,  NAN,  "nan", "-0.0 - +nan");
    TEST_SUB_LD(-0.0L, -NAN, "-nan", "-0.0 - -nan");

    TEST_SUB_LD( NAN,  0.0L,  "nan", "+nan - +0.0");
    TEST_SUB_LD(-NAN,  0.0L, "-nan", "-nan - +0.0");
    TEST_SUB_LD( NAN, -0.0L,  "nan", "+nan - -0.0");
    TEST_SUB_LD(-NAN, -0.0L, "-nan", "-nan - -0.0");

    // Create a bunch of convenient constants
    long double TWO_EXP_MIN_112 = BUILD_LD(0, -112); // 2^-112
    long double TWO_EXP_MIN_113 = BUILD_LD(0, -113); // 2^-113
    long double TWO_EXP_MIN_114 = BUILD_LD(0, -114); // 2^-114
    long double TWO_EXP_MIN_115 = BUILD_LD(0, -115); // 2^-115
    long double TWO_EXP_MIN_225 = BUILD_LD(0, -225); // 2^-225
    long double TWO_EXP_MIN_226 = BUILD_LD(0, -226); // 2^-226
    long double SMALLEST_NORMAL = BUILD_LD(0 , 1 - EXPONENT_MIDWAY);
    long double LARGEST_NORMAL = BUILD_LD(((__uint128_t) 1 << 112) - 1, EXPONENT_MIDWAY);
    long double LARGEST_SUBNORMAL = BUILD_LD(((__uint128_t) 1 << 112) - 1, -EXPONENT_MIDWAY);
    long double SMALLEST_SUBNORMAL = BUILD_LD(1, -EXPONENT_MIDWAY);
    long double ONE_PLUS_ULP = BUILD_LD(1, 0); // (1 + 2^-112)
    long double ONE_MINUS_ULP =BUILD_LD(((__uint128_t) 1 << 112) - 1, -1);
    long double TWO_MINUS_ULP = BUILD_LD(((__uint128_t) 1 << 112) - 1, 0);

    TEST_SUB_LD( 1.5L,       1.5L,  "0.00000000e+00",  "1.5 -  1.5");
    TEST_SUB_LD( 1.5L,       0.1L,  "1.40000000e+00",  "1.5 -  0.1");
    TEST_SUB_LD( 0.1L,       1.5L, "-1.40000000e+00",  "0.1 -  1.5");
    TEST_SUB_LD( 1.5L,       0.5L,  "1.00000000e+00",  "1.5 -  0.5");
    TEST_SUB_LD( 0.5L,       1.5L, "-1.00000000e+00",  "0.5 -  1.5");
    TEST_SUB_LD( 1.5L,       1.0L,  "5.00000000e-01", " 1.5 -  1.0");
    TEST_SUB_LD( 1.0L,       1.5L, "-5.00000000e-01", " 1.0 -  1.5");
    TEST_SUB_LD( 1.0L,       2.0L, "-1.00000000e+00", " 1.0 -  2.0");
    TEST_SUB_LD( 2.0L,       1.0L,  "1.00000000e+00", " 2.0 -  1.0");
    TEST_SUB_LD(-2.0L,      -1.0L, "-1.00000000e+00", "-2.0 - -1.0");
    TEST_SUB_LD(-1.0L,      -2.0L,  "1.00000000e+00", "-1.0 - -2.0");
    TEST_SUB_LD(1.0L,        0.75L, "2.50000000e-01",  "1.0 - 0.75");
    TEST_SUB_LD(0.75L,       1.0L, "-2.50000000e-01",  "0.75 - 1.0");
    TEST_SUB_LD(1.0000001L,  1.0L,  "1.00000000e-07",  "1.0000001 - 1.0");

    TEST_SUB_LD(10.12345L,  100.11111L, "-8.99876600e+01", "10.12345 - 100.11111");
    TEST_SUB_LD(100.11111L, 10.12345L,   "8.99876600e+01", "100.11111 - 10.12345");
    TEST_SUB_LD(1.0e1L,     1.0e9L,     "-9.99999990e+08", "1.0e1 - 1.0e9");
    TEST_SUB_LD(1.0e9L,     1.0e1L,      "9.99999990e+08", "1.0e9 - 1.0e1");
    ASSERT_LD_SUB_BINEQ(LARGEST_NORMAL, 1.0L, "largest normal - 1");

    ASSERT_LD_SUB_BINEQ(ONE_PLUS_ULP, ONE_PLUS_ULP, "(1 + 2^-112) - (1 + 2^-112)");
    ASSERT_LD_SUB_BINEQ(ONE_PLUS_ULP, 1.0L, "(1 + 2^-112) - 1");
    ASSERT_LD_SUB_BINEQ(2.0L, TWO_MINUS_ULP, "2 - (2 - 2^-112)");
    ASSERT_LD_SUB_BINEQ(1.0L, ONE_PLUS_ULP, "1 - (1 + 2^-112)");
    ASSERT_LD_SUB_BINEQ(ONE_PLUS_ULP, 1.0L, "(1 + 2^-112) - 1");

    // Rounding
    ASSERT_LD_SUB_BINEQ(1.0L, TWO_EXP_MIN_113, "1 - 2^-113"); // T=0, g=1, r=0, s=0  Exact
    ASSERT_LD_SUB_BINEQ(1.0L, TWO_EXP_MIN_114, "1 - 2^-114"); // T=0, g=0, r=1, s=0  Halfway
    ASSERT_LD_SUB_BINEQ(1.0L, TWO_EXP_MIN_115, "1 - 2^-115"); // T=0, g=0, r=0, s=1  Less than halfway
    ASSERT_LD_SUB_BINEQ(1.0L, TWO_EXP_MIN_114 + TWO_EXP_MIN_115, "1 - (2^-114 + 2^-115)"); // T=0, g=0, r=1, s=1  More than halfway
    ASSERT_LD_SUB_BINEQ(1.0L, TWO_EXP_MIN_114 + TWO_EXP_MIN_225, "1 - (2^-114 + 2^-225)"); // T=0, g=0, r=1, s=1  Also more than halfway
    ASSERT_LD_SUB_BINEQ(1.0L, TWO_EXP_MIN_114 - TWO_EXP_MIN_226, "1 - (2^-114 - 2^-226)"); // T=0, g=0, r=0, s=1  Below halfway
    ASSERT_LD_SUB_BINEQ(ONE_MINUS_ULP, TWO_EXP_MIN_114, "(1 - 2^-113) - 2^-114");          // T=0, g=1, r=0, s=0  Halfway
    ASSERT_LD_SUB_BINEQ(1.5L, TWO_EXP_MIN_113, "1.5 - 2^-113");                            // T=1, g=1, r=0, s=0  Exact

    // Subnormals
    ASSERT_LD_SUB_BINEQ(SMALLEST_NORMAL, SMALLEST_SUBNORMAL, "smallest normal - smallest subnormal = largest subnormal");
    ASSERT_LD_SUB_BINEQ(SMALLEST_NORMAL, LARGEST_SUBNORMAL, "smallest normal - largest subnormal = smallest subnormal");
    ASSERT_LD_SUB_BINEQ(SMALLEST_SUBNORMAL + SMALLEST_SUBNORMAL, SMALLEST_SUBNORMAL, "2*smallest subnormal - smallest subnormal = smallest subnormal");
    ASSERT_LD_SUB_BINEQ(SMALLEST_SUBNORMAL, SMALLEST_SUBNORMAL + SMALLEST_SUBNORMAL, "smallest subnormal - 2*smallest subnormal = -smallest subnormal");

    // Infinity
    ASSERT_LD_SUB_BINEQ(INFINITY, 1.0L, "+inf - 1");
    ASSERT_LD_SUB_BINEQ(1.0L, INFINITY, "1 - +inf");
    ASSERT_LD_SUB_BINEQ(-INFINITY, 1.0L, "-inf - 1");
    ASSERT_LD_SUB_BINEQ(1.0L, -INFINITY, "1 - -inf");
    ASSERT_LD_SUB_BINEQ(INFINITY, -INFINITY, "+inf - -inf");
    ASSERT_LD_SUB_BINEQ(-INFINITY, INFINITY, "-inf - +inf");
    ASSERT_LD_SUB_BINEQ(INFINITY, INFINITY, "+inf - +inf");
    ASSERT_LD_SUB_BINEQ(-INFINITY, -INFINITY, "-inf - -inf");
}

void test_multiply_ld() {
    // Zero
    ASSERT_LD_MUL_BINEQ( 0.0,  1.0, " 0.0 *  1.0");
    ASSERT_LD_MUL_BINEQ(-0.0,  1.0, "-0.0 *  1.0");
    ASSERT_LD_MUL_BINEQ( 0.0, -1.0, " 0.0 * -1.0");
    ASSERT_LD_MUL_BINEQ(-0.0, -1.0, "-0.0 * -1.0");

    // One
    ASSERT_LD_MUL_BINEQ( 1.0,  2.0, " 1.0 *  2.0");
    ASSERT_LD_MUL_BINEQ( 1.0, -2.0, " 1.0 * -2.0");
    ASSERT_LD_MUL_BINEQ(-1.0,  2.0, "-1.0 *  2.0");
    ASSERT_LD_MUL_BINEQ(-1.0, -2.0, "-1.0 * -2.0");

    // Infinity
    ASSERT_LD_MUL_BINEQ( INFINITY,   2.0,      "+inf *  2.0");
    ASSERT_LD_MUL_BINEQ( INFINITY,  -2.0,      "+inf * -2.0");
    ASSERT_LD_MUL_BINEQ( INFINITY,   0.5,      "+inf *  0.5");
    ASSERT_LD_MUL_BINEQ( INFINITY,   INFINITY, "+inf * +inf");
    ASSERT_LD_MUL_BINEQ(-INFINITY,   INFINITY, "-inf * +inf");
    ASSERT_LD_MUL_BINEQ(-INFINITY,   INFINITY, "-inf * -inf");

    ASSERT_LD_MUL_BINEQ( INFINITY,  0.0, "+inf *  0.0");
    ASSERT_LD_MUL_BINEQ( INFINITY, -0.0, "+inf * -0.0");
    ASSERT_LD_MUL_BINEQ(-INFINITY,  0.0, "-inf *  0.0");
    ASSERT_LD_MUL_BINEQ(-INFINITY, -0.0, "-inf * -0.0");

    ASSERT_LD_MUL_BINEQ( INFINITY,  NAN, "+inf *  NAN");
    ASSERT_LD_MUL_BINEQ( INFINITY, -NAN, "+inf * -NAN");
    ASSERT_LD_MUL_BINEQ(-INFINITY,  NAN, "-inf *  NAN");
    ASSERT_LD_MUL_BINEQ(-INFINITY, -NAN, "-inf * -NAN");

    // Nan
    ASSERT_LD_MUL_BINEQ( 0.0L,  NAN, "+0.0 - +nan");
    ASSERT_LD_MUL_BINEQ( 0.0L, -NAN, "+0.0 - -nan");
    ASSERT_LD_MUL_BINEQ(-0.0L,  NAN, "-0.0 - +nan");
    ASSERT_LD_MUL_BINEQ(-0.0L, -NAN, "-0.0 - -nan");

    // Regular cases
    ASSERT_LD_MUL_BINEQ( 1.0,     1.0,    " 1.0 *  1.0");
    ASSERT_LD_MUL_BINEQ( 1.0,     2.0,    " 1.0 *  2.0");
    ASSERT_LD_MUL_BINEQ( 2.0,     3.0,    " 2.0 *  3.0");
    ASSERT_LD_MUL_BINEQ( 2.0,    -3.0,    " 2.0 * -3.0");
    ASSERT_LD_MUL_BINEQ(-2.0,     3.0,    "-2.0 *  3.0");
    ASSERT_LD_MUL_BINEQ(-2.0,    -3.0,    "-2.0 * -3.0");
    ASSERT_LD_MUL_BINEQ( 3.0,     3.0,    " 3.0 *  3.0");
    ASSERT_LD_MUL_BINEQ( 3.0,     0.5,    " 3.0 *  0.5");
    ASSERT_LD_MUL_BINEQ( 1e-100L, 1e100L, " 1e-100 * 1e100");
    ASSERT_LD_MUL_BINEQ( 1e-100L, 1e200L, " 1e-100 * 1e200");
    ASSERT_LD_MUL_BINEQ( 1e-200L, 1e100L, " 1e-200 * 1e100");

    // Create a bunch of convenient constants
    long double TWO_EXP_MIN_112 = BUILD_LD(0, -112); // 2^-112
    long double SMALLEST_NORMAL = BUILD_LD(0 , 1 - EXPONENT_MIDWAY);
    long double LARGEST_NORMAL = BUILD_LD(((__uint128_t) 1 << 112) - 1, EXPONENT_MIDWAY);
    long double LARGEST_SUBNORMAL = BUILD_LD(((__uint128_t) 1 << 112) - 1, -EXPONENT_MIDWAY);
    long double SMALLEST_SUBNORMAL = BUILD_LD(1, -EXPONENT_MIDWAY);
    long double ONE_PLUS_ULP = BUILD_LD(1, 0); // (1 + 2^-112)
    long double ONE_MINUS_ULP =BUILD_LD(((__uint128_t) 1 << 112) - 1, -1);
    long double TWO_MINUS_ULP = BUILD_LD(((__uint128_t) 1 << 112) - 1, 0);

    // Rounding and subnormals
    ASSERT_LD_MUL_BINEQ(SMALLEST_NORMAL, 0.5L, "smallest normal * 0.5");
    ASSERT_LD_MUL_BINEQ(SMALLEST_NORMAL, TWO_EXP_MIN_112, "smallest normal * 2^-112");
    ASSERT_LD_MUL_BINEQ(SMALLEST_SUBNORMAL, 0.5L, "smallest subnormal * 0.5 = 0");
    ASSERT_LD_MUL_BINEQ(-SMALLEST_SUBNORMAL, 0.5L, "-smallest subnormal * 0.5 = 0");
    ASSERT_LD_MUL_BINEQ(LARGEST_NORMAL, 2.0L, "largest normal * 2 = +inf");
    ASSERT_LD_MUL_BINEQ(-LARGEST_NORMAL, 2.0L, "-largest normal * 2 = -inf");
    ASSERT_LD_MUL_BINEQ(-LARGEST_NORMAL, -2.0L, "-largest normal * -2 = +inf");
    ASSERT_LD_MUL_BINEQ(ONE_PLUS_ULP, ONE_PLUS_ULP, "(1 + 2^-112)^2 rounding");
}

void test_divide_ld() {
    // Zero
    ASSERT_LD_DIV_BINEQ( 0.0,  1.0, " 0.0 /  1.0");
    ASSERT_LD_DIV_BINEQ(-0.0,  1.0, "-0.0 /  1.0");
    ASSERT_LD_DIV_BINEQ( 0.0, -1.0, " 0.0 / -1.0");
    ASSERT_LD_DIV_BINEQ(-0.0, -1.0, "-0.0 / -1.0");
    ASSERT_LD_DIV_BINEQ( 0.0,  0.0, " 0.0 /  0.0");
    ASSERT_LD_DIV_BINEQ(-0.0,  0.0, "-0.0 /  0.0");
    ASSERT_LD_DIV_BINEQ(-0.0, -0.0, "-0.0 / -0.0");

    // Infinity
    ASSERT_LD_MUL_BINEQ( 1.0,        INFINITY, "+1.0 / +inf");
    ASSERT_LD_MUL_BINEQ(-1.0,        INFINITY, "-1.0 / +inf");
    ASSERT_LD_MUL_BINEQ(-1.0,        INFINITY, "-1.0 / -inf");
    ASSERT_LD_MUL_BINEQ( INFINITY,   1.0,      "+inf / +1.0");
    ASSERT_LD_MUL_BINEQ(-INFINITY,   1.0,      "-inf / +1.0");
    ASSERT_LD_MUL_BINEQ(-INFINITY,   1.0,      "-inf / -1.0");
    ASSERT_LD_MUL_BINEQ( INFINITY,   INFINITY, "+inf / +inf");
    ASSERT_LD_MUL_BINEQ(-INFINITY,   INFINITY, "-inf / +inf");
    ASSERT_LD_MUL_BINEQ(-INFINITY,   INFINITY, "-inf / -inf");

    // Nan
    ASSERT_LD_MUL_BINEQ( NAN,   1.0, "_nan / +1.0");
    ASSERT_LD_MUL_BINEQ( NAN,  -1.0, "_nan / -1.0");
    ASSERT_LD_MUL_BINEQ(-NAN,   1.0, "-nan / +1.0");
    ASSERT_LD_MUL_BINEQ(-NAN,  -1.0, "-nan / -1.0");

    // Regular cases
    ASSERT_LD_DIV_BINEQ(3.0L,      3.0L,     "3.0 / 3.0");
    ASSERT_LD_DIV_BINEQ(3.0L,      2.0L,     "3.0 / 2.0");
    ASSERT_LD_DIV_BINEQ(3.0L,     -2.0L,     "3.0 / -2.0");
    ASSERT_LD_DIV_BINEQ(15.0L,     4.0L,     "15.0 / 4.0");
    ASSERT_LD_DIV_BINEQ(1.0L,      1.0001L,  "1.0 / 1.0001");
    ASSERT_LD_DIV_BINEQ(1.0L,      1.5L,     "1.0 / 1.5");
    ASSERT_LD_DIV_BINEQ(1.0L,      3.0L,     "1.0 / 3.0");
    ASSERT_LD_DIV_BINEQ(1.0L,      5.0L,     "1.0 / 5.0");
    ASSERT_LD_DIV_BINEQ(2.0L,      3.0L,     "2.0 / 3.0");
    ASSERT_LD_DIV_BINEQ(4.0L,      3.0L,     "4.0 / 3.0");
    ASSERT_LD_DIV_BINEQ(7.0L,      5.0L,     "7.0 / 5.0");
    ASSERT_LD_DIV_BINEQ(7.0L,      9.0L,     "7.0 / 9.0");
    ASSERT_LD_DIV_BINEQ(7.0L,      15.0L,    "7.0 / 15.0");
    ASSERT_LD_DIV_BINEQ(9.0L,      17.0L,    "9.0 / 17.0");
    ASSERT_LD_DIV_BINEQ(10.0L,     3.0L,     "10.0 / 3.0");
    ASSERT_LD_DIV_BINEQ(113.0L,    109.0L,   "113.0 / 109.0");
    ASSERT_LD_DIV_BINEQ(1.0L,      10.0L,    "1.0 / 10.0");
    ASSERT_LD_DIV_BINEQ(1.0L,      100.0L,   "1.0 / 100.0");
    ASSERT_LD_DIV_BINEQ(1.0L,      3e-100L,  "1.0 / 3e-100");
    ASSERT_LD_DIV_BINEQ(1.0e100L,  3.0L,     "1e100 / 3");
    ASSERT_LD_DIV_BINEQ(1.0L,      3.0e100L, "1 / 3e100");
    ASSERT_LD_DIV_BINEQ(1.0L,      3.0e-100L,"1 / 3e-100");
    ASSERT_LD_DIV_BINEQ(1.0e-100L, 3.0L,     "1e-100 / 3");

    // Create a bunch of convenient constants
    long double SMALLEST_NORMAL = BUILD_LD(0 , 1 - EXPONENT_MIDWAY);
    long double LARGEST_NORMAL = BUILD_LD(((__uint128_t) 1 << 112) - 1, EXPONENT_MIDWAY);
    long double VERY_LARGE_NORMAL = BUILD_LD(0, EXPONENT_MIDWAY);  // Just smaller than the largest normal
    long double SMALLEST_SUBNORMAL = BUILD_LD(1, -EXPONENT_MIDWAY);

    // Subnormals
    ASSERT_LD_DIV_BINEQ(VERY_LARGE_NORMAL,          SMALLEST_SUBNORMAL,  "very large normal / smallest subnormal");
    ASSERT_LD_DIV_BINEQ(SMALLEST_NORMAL,            SMALLEST_SUBNORMAL,  "normal / subnormal");
    ASSERT_LD_DIV_BINEQ(SMALLEST_NORMAL,            0.5L,                "smallest normal / 0.5");
    ASSERT_LD_DIV_BINEQ(SMALLEST_NORMAL,            2.0L,                "smallest normal / 2.0");
    ASSERT_LD_DIV_BINEQ(SMALLEST_NORMAL,            3.0L,                "smallest normal / 3.0");
    ASSERT_LD_DIV_BINEQ(SMALLEST_NORMAL,            4.0L,                "smallest normal / 4.0");
    ASSERT_LD_DIV_BINEQ(SMALLEST_SUBNORMAL,         1.0L,                "smallest subnormal / 1.0");
    ASSERT_LD_DIV_BINEQ(SMALLEST_SUBNORMAL,         2.0L,                "smallest subnormal / 2.0 = 0.0");
    ASSERT_LD_DIV_BINEQ(SMALLEST_SUBNORMAL,         2.0L,                "smallest subnormal / 2.0");
    ASSERT_LD_DIV_BINEQ(SMALLEST_SUBNORMAL,         3.0L,                "smallest subnormal / 3.0");
    ASSERT_LD_DIV_BINEQ(SMALLEST_SUBNORMAL * 3.0L,  2.0L,                "smallest subnormal * 3 / 2.0");
    ASSERT_LD_DIV_BINEQ(SMALLEST_SUBNORMAL,         SMALLEST_SUBNORMAL,  "smallest subnormal / smallest subnormal");
    ASSERT_LD_DIV_BINEQ(SMALLEST_SUBNORMAL * 2.0L,  SMALLEST_SUBNORMAL,  "smallest subnormal * 2 / smallest subnormal");

    // Overflow
    ASSERT_LD_DIV_BINEQ( LARGEST_NORMAL, 0.5L,  "largest normal / 0.5 = inf");
    ASSERT_LD_DIV_BINEQ(-LARGEST_NORMAL, 0.5L, "-largest normal / 0.5 = -inf");
}

int main() {
    #ifdef __x86_64
    printf("Softfloat is not supported on x86_64\n");
    exit(1);
    #endif

    test_shift_right();
    test_update_GRS_bits();
    test_multiply_256_bit();
    test_float_roundtrip();
    test_double_roundtrip();
    test_ld_roundtrip();
    test_convert_float_to_double();
    test_convert_float_to_ld();
    test_convert_double_to_float();
    test_convert_double_to_ld();
    test_convert_ld_to_float();
    test_convert_ld_to_double();
    test_convert_fp_to_fp_nan_and_inf();
    test_rounding();
    test_convert_ld_to_int64();
    test_convert_ld_to_uint64();
    test_convert_int64_to_ld();
    test_convert_uint64_to_ld();
    test_negate_ld();
    test_add_ld();
    test_subtract_ld();
    test_multiply_ld();
    test_divide_ld();

    finish_tests();
}
