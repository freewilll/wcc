#define FOO foo bar
FOO
#undef FOO
FOO
#define FOO  foo2 bar2
FOO
#undef FOO with more tokens
FOO
#define EMPTY
a EMPTY b
a  EMPTY  b
a EMPTY EMPTY b
a  EMPTY EMPTY b
a  EMPTY  EMPTY b
a  EMPTY  EMPTY  b
EMPTY a
EMPTY EMPTY a
EMPTY EMPTY  a

A # in the middle of a line
