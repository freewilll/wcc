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
} FpValueType;

typedef struct fp_value {
    unsigned int sign;          // 0 is positive, 1 is negative
    int exponent;
    __uint128_t significand;
    FpValueType type;
} FpValue;

extern const FpEncoding _wcc_sf_binary32_encoding;
extern const FpEncoding _wcc_sf_binary64_encoding;
extern const FpEncoding _wcc_sf_binary128_encoding;

// load*.c and store*.c
FpValue __wcc_sf_load(FpEncoding encoding, __uint128_t raw);
void __wcc_sf_convert_to_inf_if_needed(FpEncoding encoding, FpValue *fpv);
void __wcc_sf_convert_to_subnormal_if_needed(FpEncoding encoding, FpValue *fpv);
__uint128_t __wcc_sf_store(FpEncoding encoding, FpValue *fpv);
FpValue     __wcc_sf_load_float(float f);
FpValue     __wcc_sf_load_double(double d);
FpValue     __wcc_sf_load_ld(long double ld);
float       __wcc_sf_store_float(FpValue *fpv);
double      __wcc_sf_store_double(FpValue *fpv);
long double __wcc_sf_store_ld(FpValue *fpv);

// conversions*.c
double      __wcc_sf_convert_float_to_double (float f);
long double __wcc_sf_convert_float_to_ld     (float f);
float       __wcc_sf_convert_double_to_float (double d);
long double __wcc_sf_convert_double_to_ld    (double d);
float       __wcc_sf_convert_ld_to_float     (long double ld);
double      __wcc_sf_convert_ld_to_double    (long double ld);

int8_t      __wcc_sf_convert_ld_to_int8      (long double ld);
uint8_t     __wcc_sf_convert_ld_to_uint8     (long double ld);
long double __wcc_sf_convert_int8_to_ld      (int8_t  i);
long double __wcc_sf_convert_uint8_to_ld     (uint8_t i);
int16_t     __wcc_sf_convert_ld_to_int16     (long double ld);
uint16_t    __wcc_sf_convert_ld_to_uint16    (long double ld);
long double __wcc_sf_convert_int16_to_ld     (int16_t  i);
long double __wcc_sf_convert_uint16_to_ld    (uint16_t i);
int32_t     __wcc_sf_convert_ld_to_int32     (long double ld);
uint32_t    __wcc_sf_convert_ld_to_uint32    (long double ld);
long double __wcc_sf_convert_int32_to_ld     (int32_t  i);
long double __wcc_sf_convert_uint32_to_ld    (uint32_t i);
int64_t     __wcc_sf_convert_ld_to_int64     (long double ld);
uint64_t    __wcc_sf_convert_ld_to_uint64    (long double ld);
long double __wcc_sf_convert_int64_to_ld     (int64_t  i);
long double __wcc_sf_convert_uint64_to_ld    (uint64_t i);

// rounding.c
int __wcc_sf_count_leading_zeros(__uint128_t v);
void __wcc_sf_update_guard_and_sticky_bits(__uint128_t *value, int shift_amount, int *guard, int *sticky);
void __wcc_sf_shift_right(__uint128_t *value, int shift_amount, int *guard, int *sticky);
void __wcc_sf_update_GRS_bits(__uint128_t *value, int shift_amount, int *guard, int *round, int *sticky);
void __wcc_sf_shift_right_with_GRS(__uint128_t *value, int shift_amount, int *guard, int *round, int *sticky);
void __wcc_sf_round_to_nearest_even(FpEncoding encoding, FpValue *fpv, int bits, int guard, int sticky);
void __wcc_sf_round_to_nearest_even_at_bits(FpEncoding encoding, FpValue *fpv, int bits);

// integers.c
FpValue __wcc_sf_convert_common_int64_to_fpv(uint64_t i, int is_signed);

// arithmetic*.c
extern const FpValue zero_fpv;
extern const FpValue nan_fpv;
extern const FpValue inf_fpv;

FpValue __wcc_sf_add(FpValue *a, FpValue *b);
uint256_t __wcc_sf_multiply_256_bit(__uint128_t a, __uint128_t b);
void __wcc_sf_negate(FpValue *fpv);
long double __wcc_sf_negate_ld(long double ld);
long double __wcc_sf_add_ld(long double a, long double b);
long double __wcc_sf_subtract_ld(long double a, long double b);
long double __wcc_sf_multiply_ld(long double a, long double b);
long double __wcc_sf_divide_ld(long double a, long double b);

// comparisons*.c
int __wcc_sf_compare(FpValue *a, FpValue *b);

#define COMPARE_FUNC(cmp_op, nan_result) { \
    FpValue fpv_a = __wcc_sf_load_ld(a); \
    FpValue fpv_b = __wcc_sf_load_ld(b); \
    if (fpv_a.type == TYPE_NAN || fpv_b.type == TYPE_NAN) return nan_result; \
    int result = __wcc_sf_compare(&fpv_a, &fpv_b); \
    return result cmp_op 0; \
}

int __wcc_sf_ld_eq(long double a, long double b);
int __wcc_sf_ld_ne(long double a, long double b);
int __wcc_sf_ld_lt(long double a, long double b);
int __wcc_sf_ld_gt(long double a, long double b);
int __wcc_sf_ld_le(long double a, long double b);
int __wcc_sf_ld_ge(long double a, long double b);

// Common LD to int code template
#define LD_TO_INT_TEMPLATE(int_type, func_name, int_min, int_max, max_exponent) \
int_type func_name(long double ld) { \
    FpValue fpv = __wcc_sf_load_ld(ld); \
\
    if (fpv.type == TYPE_ZERO || fpv.exponent < 0) \
        return 0; \
    else if (fpv.type == TYPE_INF) \
        return fpv.sign ? int_min : int_max; \
    else if (fpv.type == TYPE_NAN) \
        return fpv.sign ? 0 : int_max; \
    else if (fpv.exponent >= max_exponent) \
        return fpv.sign ? int_min : int_max; \
\
    __uint128_t result = fpv.significand >> 1; \
    SET_SBIT(result, 0); \
    int shift_amount = SIGNIFICAND_BITS - fpv.exponent - 1; \
    if (shift_amount >= SIGNIFICAND_BITS) return 0; \
    result >>=  shift_amount; \
\
    if (fpv.sign) result = -result; \
\
\
    return result; \
}

#endif

// Common LD to uint code template
#define LD_TO_UINT_TEMPLATE(uint_type, func_name, uint_max) \
uint_type func_name(long double ld) { \
    FpValue fpv = __wcc_sf_load_ld(ld); \
\
    if (fpv.type == TYPE_ZERO || fpv.exponent < 0) \
        return 0; \
    else if (fpv.type == TYPE_INF) \
        return fpv.sign ? 0 : uint_max; \
    else if (fpv.type == TYPE_NAN) \
        return fpv.sign ? 0 : uint_max; \
    else if (fpv.sign) \
        return 0; \
    else if (fpv.exponent >= 64) \
        return uint_max; \
\
    __uint128_t result = fpv.significand >> 1; \
    SET_SBIT(result, 0); \
    int shift_amount = SIGNIFICAND_BITS - fpv.exponent - 1; \
    if (shift_amount >= SIGNIFICAND_BITS) return 0; \
    result >>=  shift_amount; \
\
\
    return result; \
}
