#include "softfloat.h"
#include "testlib.h"

#ifdef DEBUG_ROUNDING
#include <stdio.h>
#endif

void make_guard_and_sticky_bits(FpValue *fpv, int start_bit, int *guard, int *sticky) {
    *guard = 0;
    *sticky = 0;

    __uint128_t mask = SBITMASK(start_bit);
    *guard = (fpv->significand & mask) != 0;
    mask >>= 1;

    *sticky = 0;
    while (mask) {
        int b = (fpv->significand & mask) != 0;
        mask >>= 1;
        *sticky |= b;
    }
}

// Round to the nearest even bit at bit bits
// The sss are orrded into a single sticky bit
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
// 1|1|0 => 1   // Exact, T is odd, round up
// 1|1|1 => 1   // Above half, round up
void round_to_nearest_even(FpEncoding encoding, FpValue *fpv, int bits, int guard, int sticky) {
    #ifdef DEBUG_ROUNDING
    // printf("Before round: g=%d s=%d    ", guard, sticky);
    printf("Before round: b=%-3d g=%d s=%d ", bits, guard, sticky);
    print_fpv(fpv);
    #endif

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
    int guard;
    int sticky;
    make_guard_and_sticky_bits(fpv, bits, &guard, &sticky);
    round_to_nearest_even(encoding, fpv, bits, guard, sticky);
}
