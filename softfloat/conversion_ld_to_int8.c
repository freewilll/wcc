#ifdef DEBUG_STORE
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

LD_TO_INT_TEMPLATE(int8_t, __wcc_sf_convert_ld_to_int8, INT8_MIN, INT8_MAX, 7)
