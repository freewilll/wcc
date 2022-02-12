#define FOO(x) # x
FOO(1 )
FOO(1 2)
FOO(1 "foo"  'c')
FOO(1 'c')

#define EMPTY
#define str(x) # x
str(EMPTY)
str(foo)
str(foo bar)
str(foo  bar)
str(foo bar)
str( foo    bar )
str("foo")
str("foo\"")
str()
str(||)
str(!&)
str(*^+)
str(* ^ +)
str( * ^ +)
str(* ^ + )
str( * ^ + )
str(\n)
str(\t)
str(\p)
str(\\)
str(\~)
str(\*)
str(\\*)
str('\n')
str("\n")
str("\\n")
str("'\\n'")
str("\"\\n\"")
str("\\n")
str("'\\n'")
str("\"\\n\"")
str("\"\\\\n\"")
str("\"'\\\\n'\"")
str("\"\\\"\\\\n\\\"\"")

#define stre(x) EMPTY # x EMPTY
stre(foo)

#define str2(x, y) # x # y
str2(EMPTY,)

// From https://msdn.microsoft.com/en-us/library/7e3a913x.aspx
#define stringer( x ) printf_s( #x "\n" )
stringer( In quotes in the printf function call );
stringer( "In quotes when printed to the screen" );
stringer( "This: \"  prints an escaped double quote" );

#define F abc
#define B def
#define FB(arg) #arg
#define FB1(arg) FB(arg)
FB(F B)
FB1(F B)

#define str(x) # x
str(x
y)
x

#define str(x) # x
str(x
    y)
 x

#define stringify(x) #x
stringify(1
)
