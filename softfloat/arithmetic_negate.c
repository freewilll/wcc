#include "softfloat.h"

// Negate a value. It is modified in place.
void __wcc_sf_negate(FpValue *fpv) {
    fpv->sign = !fpv->sign;
}

long double __wcc_sf_negate_ld(long double ld) {
    FpValue fpv = __wcc_sf_load_ld(ld);
    __wcc_sf_negate(&fpv);

    return __wcc_sf_store_ld(&fpv);
}
