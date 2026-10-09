#ifdef DEBUG_STORE
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

LD_TO_INT_TEMPLATE(int16_t, __wcc_sf_convert_ld_to_int16, INT16_MIN, INT16_MAX, 15)
