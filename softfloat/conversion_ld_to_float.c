#include "softfloat.h"

float __wcc_sf_convert_ld_to_float(long double ld) {
    FpValue fpv = __wcc_sf_load_ld(ld);
    return __wcc_sf_store_float(&fpv);
}
