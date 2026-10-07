#include "softfloat.h"

long double __wcc_sf_convert_uint64_to_ld(uint64_t i) {
    FpValue fpv = __wcc_sf_convert_common_int64_to_fpv(i, 0);
    return __wcc_sf_store_ld(&fpv);
}
