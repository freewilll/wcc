#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#include "softfloat.h"
#include "testlib.h"

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
        f = store_float(fpv); \
        after = *((__uint32_t *) &f); \
        assert_int(1, before == after, m); \

    TEST_FLOAT_ROUNDTRIP(+0.0,            "Float roundtrip: +0.0");
    TEST_FLOAT_ROUNDTRIP(-0.0,            "Float roundtrip: -0.0");
    TEST_FLOAT_ROUNDTRIP(+INFINITY,       "Float roundtrip: +inf");
    TEST_FLOAT_ROUNDTRIP(-INFINITY,       "Float roundtrip: -inf");
    TEST_FLOAT_ROUNDTRIP(NAN,             "Float roundtrip: nan");
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
        d = store_double(fpv); \
        after = *((uint64_t *) &d); \
        assert_long(1, before == after, m); \

    TEST_DOUBLE_ROUNDTRIP(+0.0,                    "Double roundtrip: +0.0");
    TEST_DOUBLE_ROUNDTRIP(-0.0,                    "Double roundtrip: -0.0");
    TEST_DOUBLE_ROUNDTRIP(+INFINITY,               "Double roundtrip: +inf");
    TEST_DOUBLE_ROUNDTRIP(-INFINITY,               "Double roundtrip: -inf");
    TEST_DOUBLE_ROUNDTRIP(NAN,                     "Double roundtrip: nan");
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
        fpv = load_double(ld); \
        ld = store_double(fpv); \
        after = *((__uint128_t *) &ld); \
        assert_long(1, before == after, m); \

    TEST_LD_ROUNDTRIP(+0.0,                                        "Long double roundtrip: +0.0");
    TEST_LD_ROUNDTRIP(-0.0,                                        "Long double roundtrip: -0.0");
    TEST_LD_ROUNDTRIP(+INFINITY,                                   "Long double roundtrip: +inf");
    TEST_LD_ROUNDTRIP(-INFINITY,                                   "Long double roundtrip: -inf");
    TEST_LD_ROUNDTRIP(NAN,                                         "Long double roundtrip: nan");
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

    TEST_LD_TO_FLOAT(+0.0,           "0.00000000e+00",  "Conversion of LD to float: above max normal +0.0");
    TEST_LD_TO_FLOAT(-0.0,           "-0.00000000e+00", "Conversion of LD to float: above max normal -0.0");
    TEST_LD_TO_FLOAT(1e+39,          "inf",             "Conversion of LD to float: above max normal +inf");
    TEST_LD_TO_FLOAT(-1e+39,         "-inf",            "Conversion of LD to float: above max normal -inf");
    TEST_LD_TO_FLOAT(NAN,            "nan",             "Conversion of LD to float: above max normal nan");
    TEST_LD_TO_FLOAT(-1.12345,       "-1.12345004e+00", "Conversion of LD to float: -1.2345");
    TEST_LD_TO_FLOAT(3.40282347e+38, "3.40282347e+38",  "Conversion of LD to float: max normal");
    TEST_LD_TO_FLOAT(1.17549500e-38, "1.17549505e-38",  "Conversion of LD to float: just above min normal");
    TEST_LD_TO_FLOAT(1.17549435e-38, "1.17549435e-38",  "Conversion of LD to float: min normal");
    TEST_LD_TO_FLOAT(1.17549421e-38,  "1.17549421e-38", "Conversion of LD to float: max subnormal");
    TEST_LD_TO_FLOAT(1.40129846e-45, "1.40129846e-45",  "Conversion of LD to float: min subnormal");
    TEST_LD_TO_FLOAT(1e-40,          "1.00000862e-40",  "Conversion of LD to float: somewhere halfway subnormal");
    TEST_LD_TO_FLOAT(1e-46,          "0.00000000e+00",  "Conversion of LD to float: below min subnormal +0.0");
    TEST_LD_TO_FLOAT(-1e-46,         "-0.00000000e+00", "Conversion of LD to float: below min subnormal -0.0");
}

void test_convert_ld_to_double() {
    #define TEST_LD_TO_DOUBLE(v, e, m) d = convert_ld_to_double(v); assert_ld_string(d, e, m)

    double d;

    TEST_LD_TO_DOUBLE(+0.0,                     "0.00000000e+00",   "Conversion of LD to double: above max normal +0.0");
    TEST_LD_TO_DOUBLE(-0.0,                     "-0.00000000e+00",  "Conversion of LD to double: above max normal -0.0");
    TEST_LD_TO_DOUBLE(1e+310L,                  "inf",              "Conversion of LD to double: above max normal +inf");
    TEST_LD_TO_DOUBLE(-1e+310L,                 "-inf",             "Conversion of LD to double: above max normal -inf");
    TEST_LD_TO_DOUBLE(NAN,                      "nan",              "Conversion of LD to double: above max normal nan");
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

    // Edge case of a round up with the significant consisting of all ones
    fpv.significand = -1; // This is 1.111111....11111 * 2^0, which is almost 2.00000
    float f = store_float(fpv);
    assert_int(0, fpv.exponent, "Rounding up with overflow 1");
    assert_float(2.0, f, "Rounding up with overflow 2");
    round_to_nearest_even_at_bits(binary32_encoding, &fpv, binary32_encoding.significand_bits);
    f = store_float(fpv);
    assert_float(2.0, f, "Rounding up with overflow 3");
    assert_int(1, fpv.exponent, "Rounding up with overflow 4");
    assert_int(0, fpv.significand, "Rounding up with overflow 5");
}

void test_convert_ld_to_int64() {
    assert_long(0,              convert_ld_to_int64(0.0L),                    "ld to int64: +0.0");
    assert_long(0,              convert_ld_to_int64(-0.0L),                   "ld to int64: -0.0");
    assert_long(INT64_MAX,      convert_ld_to_int64(INFINITY),                "ld to int64: +inf");
    assert_long(INT64_MIN,      convert_ld_to_int64(-INFINITY),               "ld to int64: -inf");
    assert_long(INT64_MAX,      convert_ld_to_int64(NAN),                     "ld to int64: nan");
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

int main() {
    #ifdef __x86_64
    printf("Softfloat is not supported on x86_64\n");
    exit(1);
    #endif

    test_float_roundtrip();
    test_double_roundtrip();
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

    finish_tests();
}
