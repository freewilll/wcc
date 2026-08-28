#ifndef __SOFTFLOAT_H
#define __SOFTFLOAT_H

/*
    Basic implementation of a software floating point library.

    The purpose is to add software support for long doubles in aarch64 for wcc.
    Internally, a floating point value is represented with the same signficand precision
    as a long double (112 bits).

    Although everything should work for both floats and doubles, the focus is on
    long doubles, so floats and doubles haven't been thoroughly tested.
*/

#include <stdint.h>

#define SIGNIFICAND_BITS 112 // The same as the amount of bits in the binary128 encoding
#define EMPTY_HIGH_BITS (128 - SIGNIFICAND_BITS) // 128-bit integer - SIGNIFICAND_BITS
#define EXPONENT_MIDWAY 16383

#define PRINT_INT128(i) printf("%016lx %016lx", (long) ((i) >> 64), (long) (i));

#define GET_ENCODING_EXPONENT_MAX(encoding) ((1 << (encoding).exponent_bits) - 1)
#define GET_ENCODING_EXPONENT_MIDWAY(encoding) ((1 << (encoding).exponent_bits - 1) - 1)

// Make a bitmask for a single bit in the significand, where 0 is the MSB
#define SBITMASK(p) ((__uint128_t) 1 << (SIGNIFICAND_BITS - (p) - 1))

// Get a significant bit, where 0 is the MSB
#define GET_SBIT(s, p) (((s) >> (SIGNIFICAND_BITS - (p) - 1)) & 1)

// Set a significant bit, where 0 is the MSB
#define SET_SBIT(s, p) (s) |= SBITMASK(p)

typedef struct {
    __uint128_t low;
    __uint128_t high;
} uint256_t;

typedef struct fp_encoding {
    int exponent_bits;
    int significand_bits;
} FpEncoding;

typedef enum fp_value_type {
    TYPE_NORMAL,
    TYPE_SUBNORMAL,
    TYPE_INF,
    TYPE_NAN,
    TYPE_ZERO,
} FPValueType;

typedef struct fp_value {
    unsigned int sign;          // 0 is positive, 1 is negative
    int exponent;
    __uint128_t significand;
    FPValueType type;
} FpValue;

extern FpEncoding binary32_encoding;
extern FpEncoding binary64_encoding;
extern FpEncoding binary128_encoding;

FpValue load_float(float f);
FpValue load_double(double d);
FpValue load_ld(long double ld);
float store_float(FpValue *fpv);
double store_double(FpValue *fpv);
long double store_ld(FpValue *fpv);

// conversions.c
double      convert_float_to_double (float f);
long double convert_float_to_ld     (float f);
float       convert_double_to_float (double d);
long double convert_double_to_ld    (double d);
float       convert_ld_to_float     (long double ld);
double      convert_ld_to_double    (long double ld);
int64_t     convert_ld_to_int64     (long double ld);
uint64_t    convert_ld_to_uint64    (long double ld);
long double convert_int64_to_ld     (int64_t  i);
long double convert_uint64_to_ld    (uint64_t i);

// rounding.c
int count_leading_zeros(__uint128_t v);
void update_guard_and_sticky_bits(__uint128_t *value, int shift_amount, int *guard, int *sticky);
void shift_right(__uint128_t *value, int shift_amount, int *guard, int *sticky);
void update_GRS_bits(__uint128_t *value, int shift_amount, int *guard, int *round, int *sticky);
void shift_right_with_GRS(__uint128_t *value, int shift_amount, int *guard, int *round, int *sticky);
void round_to_nearest_even(FpEncoding encoding, FpValue *fpv, int bits, int guard, int sticky);
void round_to_nearest_even_at_bits(FpEncoding encoding, FpValue *fpv, int bits);

// integers.c
int64_t convert_fpv_to_int64(FpValue *fpv);
uint64_t convert_fpv_to_uint64(FpValue *fpv);
FpValue convert_int64_to_fpv(int64_t i);
FpValue convert_uint64_to_fpv(uint64_t i);

// arithmetic.c
uint256_t multiply_256_bit(__uint128_t a, __uint128_t b);
long double negate_ld(long double ld);
long double add_ld(long double a, long double b);
long double subtract_ld(long double a, long double b);
long double multiply_ld(long double a, long double b);

#endif

