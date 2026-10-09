#ifdef DEBUG_STORE
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

LD_TO_INT_TEMPLATE(int32_t, __wcc_sf_convert_ld_to_int32, INT32_MIN, INT32_MAX, 31)
