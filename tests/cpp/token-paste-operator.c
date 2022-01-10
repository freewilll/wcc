// Object-like
#define obj_paste a b ## c d
obj_paste
e obj_paste
obj_paste f
e obj_paste f

#define obj_paste2 a b ## c d ## e f
obj_paste2
g obj_paste2
obj_paste2 h
g obj_paste2 h

// Evil examples from cwcc. I disabled them since the output doesn't get reprocessed
// idempotently. No sane person would do these things anyways since the output is
// interpreted as directives.
// #define foo # x
// foo

// #define bar #
// bar

#define foo2 f # x
foo2

#define bar2 f #
bar2

foo bar foo2 bar2

#define EMPTY
EMPTY foo
EMPTY bar
EMPTY foo2
EMPTY bar2

// Function-like
#define func_paste(x, y) x ## y
func_paste(,)
func_paste(a,)
func_paste(,b)
func_paste(a,b)

c func_paste(,)
c func_paste(a,)
c func_paste(,b)
c func_paste(a,b)

func_paste(,) d
func_paste(a,) d
func_paste(,b) d
func_paste(a,b) d

#define func_paste2(x, y, z) x ## y ## z
func_paste2(,,)
func_paste2(a,,)
func_paste2(,b,)
func_paste2(,,c)
func_paste2(a,b,)
func_paste2(a,,c)
func_paste2(,b,c)
func_paste2(a,b,c)

#define func_paste3(x, y, z) EMPTY func_paste2(x, y, z)
func_paste3(,,)
func_paste3(a,,)
func_paste3(,b,)
func_paste3(,,c)
func_paste3(a,b,)
func_paste3(a,,c)
func_paste3(,b,c)
func_paste3(a,b,c)

#define func_paste4a(x) x ## 1
func_paste4a(a)
func_paste4a()
#define func_paste4b(x) 1 ## x
func_paste4b(a)
func_paste4b()
#define func_paste4c(x, y) 1 ## x ## 2
func_paste4c(a,b)

// Ensure multi character tokens are lexed correctly
#define OPS && || == != <= >= += -= *= /= %= &= |= ^= -> >>= <<= ... ++ -- << >>
OPS

// Bug where replacement list was turned into a circular linked list
// love you baby Licious
#define A(X) X ## X;
A(a)
