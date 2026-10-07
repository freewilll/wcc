#ifdef DEBUG_LOAD
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

// Convert from a subnormal to normal encoding into normal when loading.
// Once loaded, values are never subnormal, since there the exponent
// is stored as an int. The only times subnormals need to be
// dealt with are during loading and storing.
void __wcc_sf_convert_from_subnormal(FpEncoding encoding, FpValue *fpv) {
    if (fpv->type != TYPE_SUBNORMAL) return;

    #ifdef DEBUG_LOAD
    printf("Before SN conversion:       ");
    print_fpv(fpv);
    #endif

    fpv->type = TYPE_NORMAL;

    // Determine the amount of leading zeros by looking at the
    // two 64-bit halves of the 128-bit significand.
    int leading_zeroes = __wcc_sf_count_leading_zeros(fpv->significand);

    // The upper bits are unused.
    leading_zeroes -= EMPTY_HIGH_BITS;

    int exponent_midway = GET_ENCODING_EXPONENT_MIDWAY(encoding);

    #ifdef DEBUG_LOAD
    printf("                            Loading subnormal with leading_zeroes=%d exponent_midway=%d\n", leading_zeroes, exponent_midway);
    #endif

    // Bump the exponent and shift the significand over
    fpv->exponent = -exponent_midway - leading_zeroes;
    fpv->significand = (fpv->significand << (leading_zeroes + 1));

    #ifdef DEBUG_LOAD
    printf("After SN conversion:        ");
    print_fpv(fpv);
    #endif
}

FpValue __wcc_sf_load(FpEncoding encoding, __uint128_t raw) {
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

    if (type == TYPE_SUBNORMAL) __wcc_sf_convert_from_subnormal(encoding, &fpv);

    #ifdef DEBUG_LOAD
    printf("Loaded                      ");
    print_fpv(&fpv);
    #endif

    return fpv;
}
