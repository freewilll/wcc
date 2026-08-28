#include "softfloat.h"

#ifdef DEBUG_ROUNDING
#include <stdio.h>
#include <stdlib.h>

#include "testlib.h"
#endif

// Determine the amount of leading zeros by looking at the
// two 64-bit halves of a  128-bit significand.
int count_leading_zeros(__uint128_t v) {
    int leading_zeros;

    uint64_t high = v >> 64;
    uint64_t low = v;

    if (high == 0 && low == 0) return 128;

    if (high == 0)
        leading_zeros =__builtin_clzll(low) + 64;
    else
        leading_zeros = __builtin_clzll(high);

    return leading_zeros;
}

// Update guard and sticky bits as if the value were to be shifted right by shift_amount
void update_guard_and_sticky_bits(__uint128_t *value, int shift_amount, int *guard, int *sticky) {
    if (!shift_amount) return;

    *sticky |= *guard;

    if (shift_amount == 1) {
        *guard = *value & 1;
    }
    else {
        *guard = (*value >> (shift_amount - 1)) & 1;
        __uint128_t mask = (((__uint128_t) 1 << (shift_amount - 1)) - 1);
        *sticky |= (*value & mask) != 0;
    }
}

// Shift right and update guard and sticky bits
void shift_right(__uint128_t *value, int shift_amount, int *guard, int *sticky) {
    update_guard_and_sticky_bits(value, shift_amount, guard, sticky);
    if (shift_amount) *value >>= shift_amount;
}

// Update guard, round and sticky bits as if the value were to be shifted right by shift_amount
void update_GRS_bits(__uint128_t *value, int shift_amount, int *guard, int *round, int *sticky) {
    if (!shift_amount) return;

    if (shift_amount >= 128) {
        // Should not normally happen.
        #ifdef DEBUG_ROUNDING
        printf("Illegal shift amount %d >= 128 in update_GRS_bits", shift_amount);
        exit(1);
        #endif
        return;
    }

    if (shift_amount == 1) {
        *sticky |= *round;
        *round = *guard;
        *guard = *value & 1;
        return;
    }

    // Implicit else, shift_amount >= 2
    *sticky |= *guard | *round;
    *guard = (*value >> (shift_amount - 1)) & 1;
    *round = (*value >> (shift_amount - 2)) & 1;

    __uint128_t mask = (((__uint128_t) 1 << (shift_amount - 2)) - 1);
    *sticky |= (*value & mask) != 0;
}

// Shift right and update guard, round and sticky bits
void shift_right_with_GRS(__uint128_t *value, int shift_amount, int *guard, int *round, int *sticky) {
    update_GRS_bits(value, shift_amount, guard, round, sticky);
    if (shift_amount) *value >>= shift_amount;
}

// Round to the nearest even
// T is the top bit
// g is the guard bit (first discarded bit)
// s is the orrded sticky bits (second and further discarded bits, they are the tie breaker)
// T|g|s    how much to add
// 0|0|0 => 0   // Exact, no change
// 0|0|1 => 0   // Inexact, below half
// 0|1|0 => 0   // Exact, T is already even
// 0|1|1 => 1   // Inexact, above half, round up
// 1|0|0 => 0   // Exact, no change
// 1|0|1 => 0   // Inexact, below half, round down
// 1|1|0 => 1   // Exact, T is odd, round up to nearest even
// 1|1|1 => 1   // Above half, round up
void round_to_nearest_even(FpEncoding encoding, FpValue *fpv, int bits, int guard, int sticky) {
    #ifdef DEBUG_ROUNDING
    printf("Before round: b=%-3d g=%d s=%d ", bits, guard, sticky);
    print_fpv(fpv);
    #endif

    // Zero the unused upper bits. The rounding code assumes one bit beyond
    // SIGNIFICAND_BITS is zero.
    fpv->significand &= (((__uint128_t) 1) << SIGNIFICAND_BITS) - 1;

    // Shift the significand over so that result contains `bits` bits
    __uint128_t result = fpv->significand >> (SIGNIFICAND_BITS - bits);

    if (guard && sticky) {
        result += 1; // Above half, Round up
    }
    else if (guard) {
        if (result & 1) result++; // Exactly halfway, round to even
    }

    // Check for overflow, this happens if the significand consists of all ones
    // and gets rounded up.
    if (result & ((__uint128_t) 1 << bits)) {
        result = 0;

        if (fpv->type == TYPE_SUBNORMAL) {
            // The implicit leading 0. becomes 1. The value has now become normal.
            // The exponent becomes one higher than the lowest exponent.
            fpv->type = TYPE_NORMAL;
            fpv->exponent = 1 - GET_ENCODING_EXPONENT_MIDWAY(encoding);
        }
        else {
            // The implicit leading 01. becomes 10. Bump the exponent.
            fpv->exponent++;
        }
    }

    // Shift the significand back so that it occupies the full SIGNIFICAND_BITS bits
    fpv->significand = result << (SIGNIFICAND_BITS - bits);

    #ifdef DEBUG_ROUNDING
    printf("After rounding:             ");
    print_fpv(fpv);
    #endif
}

// Using a significand of bits size, look a the bits beyond and round.
void round_to_nearest_even_at_bits(FpEncoding encoding, FpValue *fpv, int bits) {
    int guard = 0;
    int sticky = 0;
    update_guard_and_sticky_bits(&fpv->significand, SIGNIFICAND_BITS - bits, &guard, &sticky);
    round_to_nearest_even(encoding, fpv, bits, guard, sticky);
}
