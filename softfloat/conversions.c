#include "softfloat.h"

double      convert_float_to_double (float f)        { return store_double(load_float (f));  }
long double convert_float_to_ld     (float f)        { return store_ld    (load_float (f));  }
float       convert_double_to_float (double d)       { return store_float (load_double(d));  }
long double convert_double_to_ld    (double d)       { return store_ld    (load_double(d));  }
float       convert_ld_to_float     (long double ld) { return store_float (load_ld    (ld)); }
double      convert_ld_to_double    (long double ld) { return store_double(load_ld    (ld)); }
