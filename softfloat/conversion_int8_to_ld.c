#include "softfloat.h"

long double __wcc_sf_convert_int8_to_ld(int8_t i) {
    FpValue fpv = __wcc_sf_convert_common_int64_to_fpv((int64_t) i, 1);
    return __wcc_sf_store_ld(&fpv);
}
