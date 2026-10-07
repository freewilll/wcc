#include "softfloat.h"

double __wcc_sf_convert_ld_to_double(long double ld) {
    FpValue fpv = __wcc_sf_load_ld(ld);
    return __wcc_sf_store_double(&fpv);
}
