#ifdef DEBUG_ARITHMETIC
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

// The calculated quotient bits are as follows:
// 1 implicit leading 1 before the dot
// 112 significand bits
// 3 GRS bits
__uint128_t __wcc_sf_divide_128_bit(__uint128_t a, __uint128_t b, __uint128_t *output_remainder) {
    __uint128_t quotient = 0;
    __uint128_t remainder = a;

    for (int i = 0; i < SIGNIFICAND_BITS + 4; i++) {
        quotient <<= 1;
        if (b <= remainder) {
            quotient |= 1;
            remainder -= b;
        }
        remainder <<= 1;
    }

    remainder >>= 1;

    *output_remainder = remainder;

    return quotient;
}

// Divide two FpValues. a and b get destroyed.
static FpValue __wcc_sf_divide(FpValue *a, FpValue *b) {
    #ifdef DEBUG_ARITHMETIC
    printf("Dividing:\n");
    printf("a:                          ");
    print_fpv(a);
    printf("b:                          ");
    print_fpv(b);
    #endif

    // Zero the unused upper bits.
    a->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;
    b->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;

    // 0 / 0
    if (a->type == TYPE_ZERO && b->type == TYPE_ZERO) {
        return nan_fpv;
    }

    // Zero dividend
    if (a->type == TYPE_ZERO) {
        FpValue result = zero_fpv;
        result.sign = a->sign ^ b->sign;
        return result;
    }

    // Zero divisor
    if (b->type == TYPE_ZERO) {
        FpValue result = *a;
        result.type = TYPE_INF;
        result.sign = a->sign ^ b->sign;
        return result;
    }

    // infinity / infinity
    if (a->type == TYPE_INF && b->type == TYPE_INF) {
        return nan_fpv;
    }

    // finite / infinity
    if (b->type == TYPE_INF) {
        FpValue result = zero_fpv;
        result.sign = a->sign ^ b->sign;
        return result;
    }

    // infinity / finite
    if (a->type == TYPE_INF) {
        FpValue result = inf_fpv;
        result.sign = a->sign ^ b->sign;
        return result;
    }

    // Set the implicit leading ones on both operands
    a->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;
    b->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;

    FpValue result = *a;

    result.sign = a->sign ^ b->sign;
    result.exponent -= b->exponent;

    __uint128_t remainder;

    __uint128_t significand;
    if (b->significand)
        significand = __wcc_sf_divide_128_bit(a->significand, b->significand, &remainder);
    else
        significand = a->significand; // Don't bother dividing by 1

    result.significand = significand;

    // Normalize
    uint8_t guard = 0;
    uint8_t round = 0;
    uint8_t sticky = (remainder != 0) ? 1 : 0;

    // There are 3 rounding bits at the end of the quotient
    if (result.significand & ((__uint128_t) 1) << (SIGNIFICAND_BITS + 3)) {
        // The quotient is like 1.xxxxxxxx.
        // All three bits at the end are rounding bits.
        guard = (result.significand >> 2) & 1;
        round = (result.significand >> 1) & 1;
        sticky |= result.significand & 1;
        result.significand >>= 3;
    }
    else if (result.significand & ((__uint128_t) 1) << (SIGNIFICAND_BITS + 2)) {
        // The quotient is like 0.xxxxxxxx and needs shifting left one.
        // The remanining 2 bits at the end are rounding bits.
        guard = (result.significand >> 1) & 1;
        sticky |= result.significand & 1;
        result.significand >>= 2;
        result.exponent--;
    }

    // Round
    sticky |= round; // Merge the round bit into the sticky bit.
    __wcc_sf_round_to_nearest_even(_wcc_sf_binary128_encoding, &result, _wcc_sf_binary128_encoding.significand_bits, guard, sticky);

    #ifdef DEBUG_ARITHMETIC
    printf("Division result:            ");
    print_fpv(&result);
    #endif

    return result;
}

long double __wcc_sf_divide_ld(long double a, long double b) {
    FpValue fpv_a = __wcc_sf_load_ld(a);
    FpValue fpv_b = __wcc_sf_load_ld(b);

    // Return the first NAN if there is one.
    if (fpv_a.type == TYPE_NAN) return __wcc_sf_store_ld(&fpv_a);
    if (fpv_b.type == TYPE_NAN) return __wcc_sf_store_ld(&fpv_b);

    FpValue result = __wcc_sf_divide(&fpv_a, &fpv_b);

    return __wcc_sf_store_ld(&result);
}
