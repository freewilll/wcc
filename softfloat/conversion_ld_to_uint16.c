#ifdef DEBUG_STORE
#include <inttypes.h>
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

LD_TO_UINT_TEMPLATE(uint16_t, __wcc_sf_convert_ld_to_uint16, UINT16_MAX)
