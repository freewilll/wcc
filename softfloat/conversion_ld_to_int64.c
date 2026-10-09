#ifdef DEBUG_STORE
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

LD_TO_INT_TEMPLATE(int64_t, __wcc_sf_convert_ld_to_int64, INT64_MIN, INT64_MAX, 63)
