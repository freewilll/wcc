__FILE__ __LINE__
before include
#include "include1.h"
after include
__FILE__ __LINE__

__FILE__ __LINE__
before include again
#include "include1.h"
after include again
__FILE__ __LINE__

// Test conditional include with a sub-include
#ifndef FOO
#define FOO
#include "include1.h"
#endif
