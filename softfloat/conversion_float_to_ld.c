#include "softfloat.h"

long double __wcc_sf_convert_float_to_ld(float f) {
    FpValue fpv = __wcc_sf_load_float(f);
    return __wcc_sf_store_ld(&fpv);
}
