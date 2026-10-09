#ifdef DEBUG_STORE
#include <inttypes.h>
#include <stdio.h>
#include "testlib.h"
#endif

#include "softfloat.h"

LD_TO_UINT_TEMPLATE(uint32_t, __wcc_sf_convert_ld_to_uint32, UINT32_MAX)
