#ifdef DEBUG_ARITHMETIC
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

// Multiply two FpValues. a and b get destroyed.
static FpValue __wcc_sf_multiply(FpValue *a, FpValue *b) {
    #ifdef DEBUG_ARITHMETIC
    printf("Multiplying:\n");
    printf("a:                          ");
    print_fpv(a);
    printf("b:                          ");
    print_fpv(b);
    #endif

    // Zero the unused upper bits.
    a->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;
    b->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;

    // Infinity * zero
    if ((a->type == TYPE_INF && b->type == TYPE_ZERO) || (b->type == TYPE_INF && a->type == TYPE_ZERO)) {
        return nan_fpv;
    }

    // Zero and infinity
    if (a->type == TYPE_ZERO || a->type == TYPE_INF) { FpValue result = *a; result.sign = a->sign ^ b->sign; return result; }
    if (b->type == TYPE_ZERO || b->type == TYPE_INF) { FpValue result = *b; result.sign = a->sign ^ b->sign; return result; }

    // Implicit else, a and b are normal values

    // Set the implicit leading ones on both operands
    a->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;
    b->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;

    FpValue result = *a;

    result.sign = a->sign ^ b->sign;
    result.exponent += b->exponent;

    int guard = 0;
    int sticky = 0;

    if (b->significand) {
        uint256_t significand = __wcc_sf_multiply_256_bit(a->significand, b->significand);

        // The result needs to be shifted SIGNIFICAND_BITS bits to the right
        // | 128   | 128    |    <- multiplied  value
        // | 32 96 | 16 112 |    <- split up into parts for the 112 bit shift
        result.significand = significand.low;
        __wcc_sf_shift_right(&result.significand, 112, &guard, &sticky);
        result.significand |= significand.high << 16;
    }
    else {
        result.significand = a->significand; // Don't bother multiplying by 1
    }

    // Normalize
    if (result.significand & ((__uint128_t) 1) << (SIGNIFICAND_BITS + 1)) {
        __wcc_sf_shift_right(&result.significand, 1, &guard, &sticky);
        result.exponent += 1;
    }

    // Round
    __wcc_sf_round_to_nearest_even(_wcc_sf_binary128_encoding, &result, _wcc_sf_binary128_encoding.significand_bits, guard, sticky);

    #ifdef DEBUG_ARITHMETIC
    printf("Multiplication result:      ");
    print_fpv(&result);
    #endif

    return result;
}

// Multiply two 128-bit integers into a 256 integer.
// Any bits beyond 256 are discared.
// This needs doing with a series of 128-bit multipies of the
// four 64-bit components, followed by a bunch of additions
// including carries.
uint256_t __wcc_sf_multiply_256_bit(__uint128_t a, __uint128_t b) {
    uint256_t r = {0};

    // Split up a and b into 64-bit values
    uint64_t al = a;
    uint64_t ah = a >> 64;
    uint64_t bl = b;
    uint64_t bh = b >> 64;

    // Calculate all cross terms
    __uint128_t pll = (__uint128_t) al * bl;
    __uint128_t phl = (__uint128_t) ah * bl;
    __uint128_t plh = (__uint128_t) al * bh;
    __uint128_t phh = (__uint128_t) ah * bh;

    // Split up the cross terms into 64-bit values
    uint64_t plll = pll;
    uint64_t pllh = pll >> 64;
    uint64_t phll = phl;
    uint64_t phlh = phl >> 64;
    uint64_t plhl = plh;
    uint64_t plhh = plh >> 64;
    uint64_t phhl = phh;
    uint64_t phhh = phh >> 64;

    // Calculate the 4 64-bit components
    uint64_t r0 = pll;
    __uint128_t r1 = (__uint128_t) phll + plhl + pllh;
    __uint128_t r2 = (__uint128_t) phhl + phlh + plhh + (r1 >> 64);
    uint64_t r3 = phhh + (r2 >> 64);

    // Put them together in the final 256-bit struct
    r.low = (r1 << 64) | r0;
    r.high = ((__uint128_t) r3 << 64) | (uint64_t) r2;

    return r;
}

long double __wcc_sf_multiply_ld(long double a, long double b) {
    FpValue fpv_a = __wcc_sf_load_ld(a);
    FpValue fpv_b = __wcc_sf_load_ld(b);

    // Return the first NAN if there is one.
    if (fpv_a.type == TYPE_NAN) return __wcc_sf_store_ld(&fpv_a);
    if (fpv_b.type == TYPE_NAN) return __wcc_sf_store_ld(&fpv_b);

    FpValue result = __wcc_sf_multiply(&fpv_a, &fpv_b);

    return __wcc_sf_store_ld(&result);
}

