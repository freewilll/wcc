#include "softfloat.h"
#include "testlib.h"

#ifdef DEBUG_ARITHMETIC
#include <stdio.h>
#endif

static FpValue nan_fpv = {0, 0, (__uint128_t) 1 << (SIGNIFICAND_BITS - 1), TYPE_NAN};

// Negate a value. It is modified in place.
static void negate(FpValue *fpv) {
    fpv->sign = !fpv->sign;
}

// Add two FpValues. The contents of b gets potentially destroyed.
// If the signs differ then a subtraction is done.
static FpValue add(FpValue *a, FpValue *b) {
    #ifdef DEBUG_ARITHMETIC
    printf("Adding:\n");
    printf("a:                          ");
    print_fpv(a);
    printf("b:                          ");
    print_fpv(b);
    #endif

    FpValue result = *a;

    int is_addition = a->sign == b->sign;

    // zero +/- zero
    if (a->type == TYPE_ZERO && b->type == TYPE_ZERO) {
        // If both a re zero, the result is negative if both a and b are negative.
        result.sign =
            is_addition
                ? a->sign && b->sign    // -0.0 + -0.0
                : !a->sign && !b->sign; // -0.0 + -0.0 or -0.0 - +0.0
        return result;
    }

    // infinity - infinity and -infinity - -infinity
    if (!is_addition && a->type == TYPE_INF && b->type == TYPE_INF) {
        return nan_fpv;
    }

    // One of the values is zero; return the non-zero one
    if (a->type == TYPE_ZERO || b->type == TYPE_ZERO) {
        if (a->type == TYPE_ZERO) result = *b;
        return result;
    }

    // Zero the unused upper bits.
    a->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;
    b->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;

    // Ensure a >= b
    if (a->exponent < b->exponent || (a->exponent == b->exponent && a->significand < b->significand)) {
        result = *b;

        FpValue *tmp = a;
        a = b;
        b = tmp;
    }

    // At this point b will be added/subtracted to result and b's exponent is <= a's exponent
    int guard = 0;
    int round = 0;  // The round bit is used by subtraction
    int sticky = 0;

    // Set the implicit leading ones on both operands
    result.significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;
    b->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;

    int shift_amount = a->exponent - b->exponent;
    if (shift_amount >= SIGNIFICAND_BITS + 3) {
        // All the bits are truely shifted out of oblivion. All that is needed is the sticky bit.
        sticky = b->significand != 0;
        b->exponent = 0;
        b->significand = 0;
        b->type = TYPE_ZERO;
    }
    else if (shift_amount > 0) {
        // Shift the bits to the right so that the exponents match
        if (is_addition)
            shift_right(&b->significand, shift_amount, &guard, &sticky);
        else
            shift_right_with_GRS(&b->significand, shift_amount, &guard, &round, &sticky);

        #ifdef DEBUG_ARITHMETIC
        // Modify b so that debugging output is correct.
        // This is just for display purposes.
        b->exponent += shift_amount;
        b->type = TYPE_SUBNORMAL;

        printf("shifted b:                  ");
        print_fpv(b);
        #endif
    }

    // The exponents are now the same.

    if (is_addition) {
        // Addition

        result.significand += b->significand;

        // Normalize
        if (result.significand & ((__uint128_t) 1) << (SIGNIFICAND_BITS + 1)) {
            shift_right(&result.significand, 1, &guard, &sticky);
            result.exponent += 1;
        }
    }
    else {
        // Subtraction of a - b.
        // The GRS bits are a fraction f beyond the significand bits.
        // If f is non zero, then 1 must be borrowed from the significand.
        // The subtraction can be rewritten as:
        // a − b − f  =  (a − b − 1) + (1 − f)

        int borrow = (guard || round || sticky);

        if (sticky) {
            // 1 − f = 1 − (½G + ¼R + s) where 0 < s < ¼
            //       = −½G - ¼R -s, where -s = s since flipping all bits in s still lead to at least one bit staying 1
            // => flip G and R
            guard = !guard;
            round = !round;
        }
        else {
            // Negate the 2-bit combination of G and R. Together, then can be treated as a 2-bit number.
            int gr = (guard << 1) | round; // Make the 2-bit number
            gr = (-gr) & 3;                // Negate it and strip off the upper bits
            guard = gr > 1;                // Get the G & R bits out of the negated value
            round = gr & 1;
        }

        result.significand -= b->significand + borrow;

        // Normalize
        if (!(result.significand & ((__uint128_t) 1) << (SIGNIFICAND_BITS))) {
            int leading_zeros = count_leading_zeros(result.significand) - (128 - SIGNIFICAND_BITS);
            leading_zeros += 1; // One extra for the implicit 1 before the decimal point

            // Shift the G and R bits in
            __uint128_t significand_with_gr_bits = (result.significand << 2) | (guard << 1) | round;

            // if all the bits, guard and round are zero, then the result is zero
            if (significand_with_gr_bits == 0) {
                result.type = TYPE_ZERO;
                result.sign = 0;
                return result;
            }

            significand_with_gr_bits <<= leading_zeros;
            result.exponent -= leading_zeros;

            result.significand = significand_with_gr_bits >> 2;
            guard = (significand_with_gr_bits >> 1) & 1;
            round = 0; // No longer needed, it's merged into the sticky bit
            sticky |= significand_with_gr_bits & 1;
        }
    }

    // Round
    round_to_nearest_even(binary128_encoding, &result, binary128_encoding.significand_bits, guard, sticky);

    #ifdef DEBUG_ARITHMETIC
    printf("Addition result:            ");
    print_fpv(&result);
    #endif

    return result;
}

// Multiply two 128-bit integers into a 256 integer.
// Any bits beyond 256 are discared.
// This needs doing with a series of 128-bit multipies of the
// four 64-bit components, followed by a bunch of additions
// including carries.
uint256_t multiply_256_bit(__uint128_t a, __uint128_t b) {
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

// Multiply two FpValues. a and b get destroyed.
static FpValue multiply(FpValue *a, FpValue *b) {
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

    // Shortcut multiplying by 1
    if (a->significand == 0 && a->exponent == 0) { FpValue result = *b; result.sign = a->sign ^ b->sign; return result; }
    if (b->significand == 0 && b->exponent == 0) { FpValue result = *a; result.sign = a->sign ^ b->sign; return result; }

    // Set the implicit leading ones on both operands
    a->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;
    b->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;

    FpValue result = *a;

    result.sign = a->sign ^ b->sign;
    result.exponent += b->exponent;

    uint256_t significand = multiply_256_bit(a->significand, b->significand);

    int guard = 0;
    int sticky = 0;

    // The result needs to be shifted SIGNIFICAND_BITS bits to the right
    // | 128   | 128    |    <- multiplied  value
    // | 32 96 | 16 112 |    <- split up into parts for the 112 bit shift
    result.significand = significand.low;
    shift_right(&result.significand, 112, &guard, &sticky);
    result.significand |= significand.high << 16;

    // Normalize
    if (result.significand & ((__uint128_t) 1) << (SIGNIFICAND_BITS + 1)) {
        shift_right(&result.significand, 1, &guard, &sticky);
        result.exponent += 1;
    }

    // Round
    round_to_nearest_even(binary128_encoding, &result, binary128_encoding.significand_bits, guard, sticky);

    #ifdef DEBUG_ARITHMETIC
    printf("Multiplication result:      ");
    print_fpv(&result);
    #endif

    return result;
}

long double negate_ld(long double ld) {
    FpValue fpv = load_ld(ld);
    negate(&fpv);

    return store_ld(&fpv);
}

long double add_ld(long double a, long double b) {
    FpValue fpv_a = load_ld(a);
    FpValue fpv_b = load_ld(b);

    // Return the first NAN if there is one.
    if (fpv_a.type == TYPE_NAN) return store_ld(&fpv_a);
    if (fpv_b.type == TYPE_NAN) return store_ld(&fpv_b);

    FpValue result = add(&fpv_a, &fpv_b);

    return store_ld(&result);
}

long double subtract_ld(long double a, long double b) {
    FpValue fpv_a = load_ld(a);
    FpValue fpv_b = load_ld(b);

    // Return the first NAN if there is one.
    if (fpv_a.type == TYPE_NAN) return store_ld(&fpv_a);
    if (fpv_b.type == TYPE_NAN) return store_ld(&fpv_b);

    negate(&fpv_b);
    FpValue result = add(&fpv_a, &fpv_b);

    return store_ld(&result);
}

long double multiply_ld(long double a, long double b) {
    FpValue fpv_a = load_ld(a);
    FpValue fpv_b = load_ld(b);

    // Return the first NAN if there is one.
    if (fpv_a.type == TYPE_NAN) return store_ld(&fpv_a);
    if (fpv_b.type == TYPE_NAN) return store_ld(&fpv_b);

    FpValue result = multiply(&fpv_a, &fpv_b);

    return store_ld(&result);
}
