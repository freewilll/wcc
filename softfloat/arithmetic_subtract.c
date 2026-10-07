#include "softfloat.h"

long double __wcc_sf_subtract_ld(long double a, long double b) {
    FpValue fpv_a = __wcc_sf_load_ld(a);
    FpValue fpv_b = __wcc_sf_load_ld(b);

    // Return the first NAN if there is one.
    if (fpv_a.type == TYPE_NAN) return __wcc_sf_store_ld(&fpv_a);
    if (fpv_b.type == TYPE_NAN) return __wcc_sf_store_ld(&fpv_b);

    __wcc_sf_negate(&fpv_b);
    FpValue result = __wcc_sf_add(&fpv_a, &fpv_b);

    return __wcc_sf_store_ld(&result);
}

