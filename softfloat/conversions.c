#include "softfloat.h"

double      __wcc_sf_convert_float_to_double (float f)        { FpValue fpv = __wcc_sf_load_float              (f);  return __wcc_sf_store_double         (&fpv); };
long double __wcc_sf_convert_float_to_ld     (float f)        { FpValue fpv = __wcc_sf_load_float              (f);  return __wcc_sf_store_ld             (&fpv); };
float       __wcc_sf_convert_double_to_float (double d)       { FpValue fpv = __wcc_sf_load_double             (d);  return __wcc_sf_store_float          (&fpv); };
long double __wcc_sf_convert_double_to_ld    (double d)       { FpValue fpv = __wcc_sf_load_double             (d);  return __wcc_sf_store_ld             (&fpv); };
float       __wcc_sf_convert_ld_to_float     (long double ld) { FpValue fpv = __wcc_sf_load_ld                 (ld); return __wcc_sf_store_float          (&fpv); };
double      __wcc_sf_convert_ld_to_double    (long double ld) { FpValue fpv = __wcc_sf_load_ld                 (ld); return __wcc_sf_store_double         (&fpv); };
int64_t     __wcc_sf_convert_ld_to_int64     (long double ld) { FpValue fpv = __wcc_sf_load_ld                 (ld); return __wcc_sf_convert_fpv_to_int64 (&fpv); };
uint64_t    __wcc_sf_convert_ld_to_uint64    (long double ld) { FpValue fpv = __wcc_sf_load_ld                 (ld); return __wcc_sf_convert_fpv_to_uint64(&fpv); };
long double __wcc_sf_convert_int64_to_ld     (int64_t  i)     { FpValue fpv = __wcc_sf_convert_int64_to_fpv    (i);  return __wcc_sf_store_ld             (&fpv); };
long double __wcc_sf_convert_uint64_to_ld    (uint64_t i)     { FpValue fpv = __wcc_sf_convert_uint64_to_fpv   (i);  return __wcc_sf_store_ld             (&fpv); };
