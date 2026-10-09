#include "softfloat.h"

long double __wcc_sf_convert_uint8_to_ld(uint8_t i) {
    FpValue fpv = __wcc_sf_convert_common_int64_to_fpv((uint64_t) i, 0);
    return __wcc_sf_store_ld(&fpv);
}
