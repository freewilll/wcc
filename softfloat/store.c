#ifdef DEBUG_STORE
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

void __wcc_sf_convert_to_inf_if_needed(FpEncoding encoding, FpValue *fpv) {
    if (fpv->type != TYPE_NORMAL) return;

    int exponent_max = GET_ENCODING_EXPONENT_MAX(encoding);
    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    if (fpv->type != TYPE_SUBNORMAL && fpv->exponent + exponent_midway >= exponent_max) {
        fpv->type = TYPE_INF;
        fpv->exponent = 0;

        #ifdef DEBUG_STORE
        printf("Converted to infinite:      ");
        print_fpv(fpv);
        #endif
    }
}

// When storing, convert a normal value to a subnormal, when possible.
// This could lead to a +0.0 or -0.0 in case all non-zero bits vanish.
void __wcc_sf_convert_to_subnormal_if_needed(FpEncoding encoding, FpValue *fpv) {
    if (fpv->type != TYPE_NORMAL) return;

    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    // The shift_amount doesn't include the implicit leading 1 before the decimal point.
    // shift_amount may be zero.
    int shift_amount = -exponent_midway - fpv->exponent;

    // The -1 in the comparison is because shift_amount doesn't include the implicit 1,
    // which is also shifted over.
    if (shift_amount <= -1) {
        #ifdef DEBUG_STORE
        printf("                            Not to converting subnormal since exponent=%d and midway = %d\n", fpv->exponent, exponent_midway);
        #endif

        return; // The normal representation can be used in the encoding
    }

    // Check if all bits would disappear
    if (shift_amount > encoding.significand_bits) {
        fpv->exponent = 0;
        fpv->significand = 0;
        fpv->type = TYPE_ZERO;

        #ifdef DEBUG_STORE
        printf("                            No bits would be left after converting to subnormal, making a zero\n");
        print_fpv(fpv);
        #endif

        return;
    }

    // Make the guard and sticky bits from the shift amount.
    int guard = 0;
    int sticky = 0;

    // Set the implicit 1 before the dot
    fpv->significand |= ((__uint128_t) 1) << SIGNIFICAND_BITS;

    // Set the guard and sticky bits based on a total shift of shift_amount + 1
    __wcc_sf_update_guard_and_sticky_bits(&fpv->significand, SIGNIFICAND_BITS - encoding.significand_bits + shift_amount + 1, &guard, &sticky);

    #ifdef DEBUG_STORE
    printf("                            Converting to subnormal since %d - %d < 1: shift by 1 + %d\n", -fpv->exponent, exponent_midway, shift_amount);
    printf("Before SN conversion:       ");
    print_fpv(fpv);
    #endif

    // The first shift is special, because the implicit 1 before the decimal point
    // from the normal encoding needs to be shifted over.
    fpv->significand >>= 1;

    // Shift the bits over
    if (shift_amount) fpv->significand >>= shift_amount;

    fpv->type = TYPE_SUBNORMAL;

    #ifdef DEBUG_STORE
    printf("After SN conversion:        ");
    print_fpv(fpv);
    #endif

    // Finally, round using the guard and sticky bits.
    // This may lead to the value becoming normal again.
    __wcc_sf_round_to_nearest_even(encoding, fpv, encoding.significand_bits, guard, sticky);
}

__uint128_t __wcc_sf_store(FpEncoding encoding, FpValue *fpv) {
    #ifdef DEBUG_STORE
    printf("Storing                     ");
    print_fpv(fpv);
    #endif

    __uint128_t raw = 0;

    // Store sign
    int non_sign_bits = (encoding.exponent_bits + encoding.significand_bits);
    raw |= ((__uint128_t) fpv->sign << non_sign_bits);

    // Store exponent
    int exponent_max = GET_ENCODING_EXPONENT_MAX(encoding);
    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    if (fpv->type == TYPE_INF || fpv->type == TYPE_NAN)
        raw |= ((__uint128_t) exponent_max << encoding.significand_bits);
    else if (fpv->type != TYPE_SUBNORMAL && fpv->type != TYPE_ZERO)
        raw |= ((__uint128_t) (fpv->exponent + exponent_midway) << encoding.significand_bits);

    // Store significand
    if (fpv->type != TYPE_INF && fpv->type != TYPE_ZERO) {
        __int128 significand_mask = (((__uint128_t) 1) << encoding.significand_bits) - 1;
        raw |= ((fpv->significand >> (SIGNIFICAND_BITS - encoding.significand_bits)) & significand_mask);
    }

    return raw;
}

