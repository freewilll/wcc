#include "softfloat.h"

float __wcc_sf_convert_double_to_float(double d) {
    FpValue fpv = __wcc_sf_load_double(d);
    return __wcc_sf_store_float(&fpv);
}
