// 6.10.3.3.4
#define hash_hash # ## #
#define mkstr(a) # a
#define in_between(a) mkstr(a)
#define join(c, d) in_between(c hash_hash d)
char p[] = join(x, y);

// 6.10.3.4.5
#define x       3
#define f(a)    f(x * (a))
#undef  x
#define x       2
#define g       f
#define z       z[0]
#define h       g(~
#define m(a)    a(w)
#define w       0,1
#define t(a)    a
#define p()     int
#define q(x)    x
#define r(x,y)  x ## y
#define str(x)  # x

f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);
g(x+(3,4)-w) | h 5) & m(f)^m(m);
p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };
char c[2][6] = { str(hello), str() };

// 6.10.3.5.6
#define xstr(s)         str(s)
#define debug(s, t)     printf("x" # s "= %d, x" # t "= %s", \
                               x ## s, x ## t)
#define INCFILE(n)      vers ## n
#define glue(a, b)      a ## b
#define xglue(a, b)     glue(a, b)
#define HIGHLOW         "hello"
#define LOW             LOW ", world"
debug(1, 2);
fputs(str(strncmp("abc\0d", "abc", '\4') // this goes away
          == 0) str(: @\n), s);
glue(HIGH, LOW);
xglue(HIGH, LOW)

// 6.10.3.4.7
#define t2(x,y,z) x ## y ## z
int j[] = { t2(1,2,3), t2(,4,5), t2(6,,7), t2(8,9,),
            t2(10,,), t2(,11,), t2(,,12), t2(,,) };
