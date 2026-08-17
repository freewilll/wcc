#include "softfloat.h"

double      convert_float_to_double (float f)        { FpValue fpv = load_float              (f);  return store_double         (&fpv); };
long double convert_float_to_ld     (float f)        { FpValue fpv = load_float              (f);  return store_ld             (&fpv); };
float       convert_double_to_float (double d)       { FpValue fpv = load_double             (d);  return store_float          (&fpv); };
long double convert_double_to_ld    (double d)       { FpValue fpv = load_double             (d);  return store_ld             (&fpv); };
float       convert_ld_to_float     (long double ld) { FpValue fpv = load_ld                 (ld); return store_float          (&fpv); };
double      convert_ld_to_double    (long double ld) { FpValue fpv = load_ld                 (ld); return store_double         (&fpv); };
int64_t     convert_ld_to_int64     (long double ld) { FpValue fpv = load_ld                 (ld); return convert_fpv_to_int64 (&fpv); };
uint64_t    convert_ld_to_uint64    (long double ld) { FpValue fpv = load_ld                 (ld); return convert_fpv_to_uint64(&fpv); };
long double convert_int64_to_ld     (int64_t  i)     { FpValue fpv = convert_int64_to_fpv    (i);  return store_ld             (&fpv); };
long double convert_uint64_to_ld    (uint64_t i)     { FpValue fpv = convert_uint64_to_fpv   (i);  return store_ld             (&fpv); };
