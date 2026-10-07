#include "softfloat.h"

long double __wcc_sf_convert_int64_to_ld(int64_t i) {
    FpValue fpv = __wcc_sf_convert_common_int64_to_fpv(i, 1);
    return __wcc_sf_store_ld(&fpv);
}
