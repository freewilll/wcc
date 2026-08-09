#include <stdio.h>
#include <stdint.h>

#include "softfloat.h"

#if defined(DEBUG_LOAD) || defined(DEBUG_STORE)
#include "testlib.h"
#endif

FpEncoding binary32_encoding = { 8, 23 };
FpEncoding binary64_encoding = { 11, 52 };
FpEncoding binary128_encoding = { 15, 112 };

// Convert from a subnormal to normal encoding into normal when loading.
// Once loaded, values are never subnormal, since there the exponent
// is stored as an int. The only times subnormals need to be
// dealt with are during loading and storing.
static void convert_from_subnormal(FpEncoding encoding, FpValue *fpv) {
    if (fpv->type != TYPE_SUBNORMAL) return;

    #ifdef DEBUG_LOAD
    printf("Before SN conversion:       ");
    print_fpv(fpv);
    #endif

    fpv->type = TYPE_NORMAL;

    // Determine the amount of leading zeros by looking at the
    // two 64-bit halves of the 128-bit significand.
    int leading_bits;
    uint64_t high = fpv->significand >> 64;
    uint64_t low = fpv->significand;

    // Shouldn't happen; the IEEE encoding requires that at least one bit is 1
    if (high == 0 && low == 0) return;

    if (high == 0)
        leading_bits = 64 + __builtin_clzll(low);
    else
        leading_bits = __builtin_clzll(high);

    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    #ifdef DEBUG_LOAD
    printf("                            Loading subnormal with leading_bits=%d exponent_midway=%d\n", leading_bits, exponent_midway);
    #endif

    // Bump the exponent and shift the significand over
    fpv->exponent = -exponent_midway - leading_bits;
    fpv->significand = (fpv->significand << (leading_bits + 1));

    #ifdef DEBUG_LOAD
    printf("After SN conversion:        ");
    print_fpv(fpv);
    #endif
}

// When storing, convert a normal value to a subnormal, when possible.
// This could lead to a +0.0 or -0.0 in case all non-zero bits vanish.
static void convert_to_subnormal_if_needed(FpEncoding encoding, FpValue *fpv) {
    if (fpv->type != TYPE_NORMAL) return;

    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    // The shift_amount doesn't include the implicit leading 1 before the decimal point
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

        #ifdef DEBUG_STORE
        printf("                            No bits would be left after converting to subnormal, making a zero\n");
        print_fpv(fpv);
        #endif
    }

    // Make the guard and sticky bits from the shift amount.
    int guard;
    int sticky;
    make_guard_and_sticky_bits(fpv, encoding.significand_bits - shift_amount, &guard, &sticky);

    #ifdef DEBUG_STORE
    printf("                            Converting to subnormal since %d - %d < 1: shift by %d\n", -fpv->exponent, exponent_midway, shift_amount);
    printf("Before SN conversion:       ");
    print_fpv(fpv);
    #endif

    // The first shift is special, because the implicit 1 before the decimal point
    // from the normal encoding needs to be shifted over.
    fpv->significand >>= 1;
    fpv->significand |= (__uint128_t) 1 << (SIGNIFICAND_BITS - 1);

    // Shift the bits over
    fpv->significand >>= shift_amount;

    fpv->type = TYPE_SUBNORMAL;

    #ifdef DEBUG_STORE
    printf("After SN conversion:        ");
    print_fpv(fpv);
    #endif

    // Finally, round using the guard and sticky bits.
    // This may lead to the value becoming normal again.
    round_to_nearest_even(encoding, fpv, encoding.significand_bits, guard, sticky);
}

static void convert_to_inf_if_needed(FpEncoding encoding, FpValue *fpv) {
    if (fpv->type != TYPE_NORMAL) return;

    int exponent_max = GET_ENCODING_EXPONENT_MAX(encoding);
    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    if (fpv->type != TYPE_SUBNORMAL && fpv->exponent + exponent_midway > exponent_max) {
        fpv->type = TYPE_INF;
        fpv->exponent = 0;

        #ifdef DEBUG_STORE
        printf("Converted to infinite:      ");
        print_fpv(fpv);
        #endif
    }
}

static FpValue load(FpEncoding encoding, __uint128_t raw) {
    // Load sign
    int non_sign_bits = (encoding.exponent_bits + encoding.significand_bits);
    unsigned int sign = (raw >> non_sign_bits) & 1;

    // Load exponent
    int based_exponent = ((raw >> encoding.significand_bits) & ((1 << encoding.exponent_bits) - 1));

    // Load significand
    __int128 significand_mask = (((__uint128_t) 1) << encoding.significand_bits) - 1;
    __uint128_t significand = raw & significand_mask;
    significand <<= (SIGNIFICAND_BITS - encoding.significand_bits);

    // Classification
    int exponent_max = GET_ENCODING_EXPONENT_MAX(encoding);
    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    // Deal with the encoding cases for inf, subnormal and nan
    int type = TYPE_NORMAL;
    if (based_exponent == 0            && significand == 0) type = TYPE_ZERO;
    if (based_exponent == 0            && significand != 0) type = TYPE_SUBNORMAL;
    if (based_exponent == exponent_max && significand == 0) type = TYPE_INF;
    if (based_exponent == exponent_max && significand != 0) type = TYPE_NAN;

    int exponent = type == TYPE_ZERO ? 0 : based_exponent - exponent_midway;

    FpValue fpv = {sign, exponent, significand, type};

    if (type == TYPE_SUBNORMAL) convert_from_subnormal(encoding, &fpv);

    #ifdef DEBUG_LOAD
    printf("Loaded                      ");
    print_fpv(&fpv);
    #endif

    return fpv;
}

static __uint128_t store(FpEncoding encoding, FpValue fpv) {
    #ifdef DEBUG_STORE
    printf("Storing                     ");
    print_fpv(&fpv);
    #endif

    __uint128_t raw = 0;

    // Store sign
    int non_sign_bits = (encoding.exponent_bits + encoding.significand_bits);
    raw |= ((__uint128_t) fpv.sign << non_sign_bits);

    // Store exponent
    int exponent_max = GET_ENCODING_EXPONENT_MAX(encoding);
    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    if (fpv.type == TYPE_INF || fpv.type == TYPE_NAN)
        raw |= ((__uint128_t) exponent_max << encoding.significand_bits);
    else if (fpv.type != TYPE_SUBNORMAL && fpv.type != TYPE_ZERO)
        raw |= ((__uint128_t) (fpv.exponent + exponent_midway) << encoding.significand_bits);

    // Store significand
    if (fpv.type != TYPE_INF && fpv.type != TYPE_ZERO) {
        __int128 significand_mask = (((__uint128_t) 1) << encoding.significand_bits) - 1;
        raw |= ((fpv.significand >> (SIGNIFICAND_BITS - encoding.significand_bits)) & significand_mask);
    }

    return raw;
}

FpValue load_float(float f) {
    #ifdef DEBUG_LOAD
    printf("Loading %15g\n", f);
    #endif

    __uint128_t raw = 0;
    raw = *((uint32_t *) &f);
    return load(binary32_encoding, raw);
}

FpValue load_double(double d) {
    #ifdef DEBUG_LOAD
    printf("Loading %15g\n", d);
    #endif

    __uint128_t raw = 0;
    raw = *((uint64_t *) &d);
    return load(binary64_encoding, raw);
}

FpValue load_ld(long double ld) {
    #ifdef DEBUG_LOAD
    printf("Loading %15Lg\n", ld);
    #endif

    __uint128_t raw = *((__uint128_t *) &ld);
    return load(binary128_encoding, raw);
}

float store_float(FpValue fpv) {
    #ifdef DEBUG_STORE
    printf("Storing                     ");
    print_fpv(&fpv);
    #endif

    convert_to_inf_if_needed(binary32_encoding, &fpv);
    convert_to_subnormal_if_needed(binary32_encoding, &fpv);

    // Subnormals have already been rounded and don't need rounding done again.
    if (fpv.type != TYPE_SUBNORMAL)
        round_to_nearest_even_at_bits(binary32_encoding, &fpv, binary32_encoding.significand_bits);

    __uint128_t raw = store(binary32_encoding, fpv);

    float f = *((float *) &raw);

    #ifdef DEBUG_STORE
    printf("Stored  %15g\n", f);
    #endif

    return f;
}

double store_double(FpValue fpv) {
    #ifdef DEBUG_STORE
    printf("Storing                     ");
    print_fpv(&fpv);
    #endif

    convert_to_inf_if_needed(binary64_encoding, &fpv);
    convert_to_subnormal_if_needed(binary64_encoding, &fpv);

    // Subnormals have already been rounded and don't need rounding done again.
    if (fpv.type != TYPE_SUBNORMAL)
        round_to_nearest_even_at_bits(binary64_encoding, &fpv, binary64_encoding.significand_bits);

    __uint128_t raw = store(binary64_encoding, fpv);

    double d = *((double *) &raw);

    #ifdef DEBUG_STORE
    printf("Stored  %15g\n", d);
    #endif

    return d;
}

long double store_ld(FpValue fpv) {
    #ifdef DEBUG_STORE
    printf("Storing                     ");
    print_fpv(&fpv);
    #endif

    __uint128_t raw = store(binary128_encoding, fpv);
    long double ld = *((long double *) &raw);

    #ifdef DEBUG_STORE
    printf("Stored  %15Lg\n", ld);
    #endif

    return ld;
}
