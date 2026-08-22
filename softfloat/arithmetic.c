#include "softfloat.h"
#include "testlib.h"

#ifdef DEBUG_ARITHMETIC
#include <stdio.h>
#endif

// Negate a value. It is modified in place.
static void negate(FpValue *fpv) {
    fpv->sign = !fpv->sign;
}

// Add two FpValues. The contents of b gets potentially destroyed.
static FpValue add(FpValue *a, FpValue *b) {
    #ifdef DEBUG_ARITHMETIC
    printf("Adding:\n");
    printf("a:                          ");
    print_fpv(a);
    printf("b:                          ");
    print_fpv(b);
    #endif

    FpValue result = *a;

    // Addition of zero + zero
    if (a->type == TYPE_ZERO && b->type == TYPE_ZERO) {
        // If both a re zero, the result is negative if both a and b are negative.
        result.sign = a->sign && b->sign;
        return result;
    }

    // One of the values is zero; return the non-zero one
    if (a->type == TYPE_ZERO || b->type == TYPE_ZERO) {
        if (a->type == TYPE_ZERO) result = *b;
        return result;
    }

    // Zero the unused upper bits.
    a->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;
    b->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;

    // Ensure a's exponent is >= b's exponent
    if (a->exponent < b->exponent) {
        result = *b;

        FpValue *tmp = a;
        a = b;
        b = tmp;
    }

    // At this point b will be added to result and b's exponent is <= a's exponent
    int guard = 0;
    int sticky = 0;

    // Set the implicit leading ones on both operands
    result.significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;
    b->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;

    int shift_amount = a->exponent - b->exponent;
    if (shift_amount >= SIGNIFICAND_BITS + 2) {
        // All the bits are truely shifted out of oblivion. All that is needed is the sticky bit.
        sticky = b->significand != 0;
        b->exponent = 0;
        b->significand = 0;
        b->type = TYPE_ZERO;
    }
    else if (shift_amount > 0) {
        // Shift the bits to the right so that the exponents match
        shift_right(&b->significand, shift_amount, &guard, &sticky);

        #ifdef DEBUG_ARITHMETIC
        // Modify b so that the debugging output is correct.
        // This is just for display purposes.
        b->exponent += shift_amount;
        b->type = TYPE_SUBNORMAL;

        printf("shifted b:                  ");
        print_fpv(b);
        #endif
    }

    // The exponents are now the same.

    if (a->sign != b->sign) {
        // TODO subtraction
        return result;
    }

    // Implicit else: we're doing addition

    // Add the significands.
    result.significand += b->significand;

    // Normalize
    if (result.significand & ((__uint128_t) 1) << (SIGNIFICAND_BITS + 1)) {
        shift_right(&result.significand, 1, &guard, &sticky);
        result.exponent += 1;
    }

    // Round
    round_to_nearest_even(binary128_encoding, &result, binary128_encoding.significand_bits, guard, sticky);

    #ifdef DEBUG_ARITHMETIC
    printf("Addition result:            ");
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
    FpValue result = add(&fpv_a, &fpv_b);

    return store_ld(&result);
}

long double subtract_ld(long double a, long double b) {
    FpValue fpv_a = load_ld(a);
    FpValue fpv_b = load_ld(b);
    negate(&fpv_b);
    FpValue result = add(&fpv_a, &fpv_b);

    return store_ld(&result);
}