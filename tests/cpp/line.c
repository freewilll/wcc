__LINE__ __FILE__
#line 10
__LINE__ __FILE__
#include "include.h"
__LINE__ __FILE__

#line 10 "line-renamed.c"
__LINE__ __FILE__
#include "include.h"
__LINE__ __FILE__

// Ensure skipped directive doesn't change the line
#if 0
#line 10000
#endif
__LINE__

#define FOO 42
#line FOO
__FILE__ __LINE__

#undef FOO
#define FOO 420 "renamed"
#line FOO
__FILE__ __LINE__

#undef line
#define line 1000
line
#line line
#if 1000 != __LINE__
"  # line line" not working
#endif
