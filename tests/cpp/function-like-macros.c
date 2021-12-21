#define noparams() bar
noparams()

#define param1(x) x + 1
param1(1)
param1 (1)
param1  (1)
param1  (1)
param1()

#define param2a(x,y) x + y
param2a(1, 2)
param2a(,1)
param2a(1,)
param2a(,)
param2a((,),)

#define param2b( x,y) x + y
param2b(1, 2)

#define param2c(x,y ) x + y
param2c(1, 2)

#define param2d(x ,y ) x + y
param2d(1, 2)

#define param2e(x, y) x + y
param2e(1, 2)

#define Z z
#define param2f(x, y) x + y + Z
param2f(1, 2)

#define param2g(x, y) x + y + param1(x * y)
param2g(1, 2)

// Test parentheses and command handling in the replacement list
param2a((1, 2), 3)
param2a((1, 2), ((3)))
param2a(((1, 2)), ((3)))
param2a(((1, (2))), ((3)))

// Test just using the macro without an opening parenthesis leads to no expansion
param2a
param2a+
param2a +

#define rec(x) x + rec(x)
rec(1)

// Test undefs
afunc(x)
#define afunc(x) x + 1
afunc(1)
#undef afunc
afunc(1)
#define afunc(x) x + 2
afunc(1)
#undef afunc
afunc(1)

// Empty replacement lists
#define erlfoo()
x erlfoo() x

#define erlfoox(x) x
x erlfoox() x
x erlfoox(x) x

#define erlfooxy(x, y) x y
x erlfooxy(x, y) x

// Same formal parameter used twice, combined with an actual parameter which is a macro
#define param_twice(x) x * x
param_twice(Z)

// Using a CPP keyword as function parameter name
#define foo(line) line + line
foo(1)
