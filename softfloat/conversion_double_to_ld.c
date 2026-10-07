#include "softfloat.h"

long double __wcc_sf_convert_double_to_ld(double d) {
    FpValue fpv = __wcc_sf_load_double(d);
    return __wcc_sf_store_ld(&fpv);
}
