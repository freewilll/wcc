#include "softfloat.h"

double      convert_float_to_double (float f)        { return store_double         (load_float (f));  }
long double convert_float_to_ld     (float f)        { return store_ld             (load_float (f));  }
float       convert_double_to_float (double d)       { return store_float          (load_double(d));  }
long double convert_double_to_ld    (double d)       { return store_ld             (load_double(d));  }
float       convert_ld_to_float     (long double ld) { return store_float          (load_ld    (ld)); }
double      convert_ld_to_double    (long double ld) { return store_double         (load_ld    (ld)); }
int64_t     convert_ld_to_int64     (long double ld) { return convert_fpv_to_int64 (load_ld    (ld)); }
uint64_t    convert_ld_to_uint64    (long double ld) { return convert_fpv_to_uint64(load_ld    (ld)); }
long double convert_int64_to_ld     (int64_t  i)     { return store_ld(convert_int64_to_fpv    (i));  }
long double convert_uint64_to_ld    (uint64_t i)     { return store_ld(convert_uint64_to_fpv   (i));  }
