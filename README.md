# WCC Toy C Compiler for x86_64 Linux

This project is an implementation of a subset of C for x86_64 linux as an exercise in how to write an optimizing compiler. The implementation is mostly based on [Engineering a Compiler 2nd Edition](https://www.amazon.com/Engineering-Compiler-Keith-Cooper/dp/012088478X). The compiler is self hosting. It's just a hobby, it won't be big and professional like gcc.

# Usage
```
$ make wcc
$ ./wcc test.c -o test
```

# C Features
- All variables must fit in a register
- Pointers
- Structs
- Simple typedefs, only used for structs
- Single level nested header includes
- Most integer arithmetic and pointer operations
- Some commonly used functions are available, e.g. `malloc`, `memset`, `open`, `close`, ... , see `add_builtins` in [parser.c](parser.c)

# Limitations
- System headers aren't supported
- No nested headers
- No preprocessor support
- All variable declarations must be on top

# Implementation
The compiler goes through the following phases:

- Hand rolled lexer
- Precedence climbing parser
- `optimize_arithmetic_operations` for simple arithmetic transformations
- Transformation into [SSA](https://en.wikipedia.org/wiki/Static_single_assignment_form)
- Data flow analysis and replacement of variables with live ranges
- Live range coalescing
- Some more arithmetic optimizations while converting out of a linear IR into a tree IR
- Instruction selection using tree pattern matching
- Live range coalescing (again)
- Register allocation using top-down graph coloring
- Code generation using hand rolled functions

# Running the tests
Run all tests
```
$ make test
```

Test 3 stage self compilation
```
$ make test-self-compilation
```

Run benchmarks
```
$ make run-benchmark
```

# Example compilation
```
#include <stdio.h>

typedef struct s1 {
    int i, j;
} S1;

typedef struct s2 {
    S1 *s1;
} S2;

void main() {
    S2 *s2;

    s2 = malloc(sizeof(S2));
    s2->s1 = malloc(sizeof(S1));
    s2->s1->j = 1;
    printf("%d\n", s2->s1->j);
}
```

```
main:
    push    %rbp                # Function prologue
    movq    %rsp, %rbp
    pushq   %rbx
    subq    $8, %rsp
    movq    $8, %rdi            # s2 = malloc(sizeof(S2));
    callq   malloc@PLT
    movq    %rax, %rbx          # rbx = s2
    addq    $8, %rsp            # FIXME remove identical addq/subq combinations
    subq    $8, %rsp
    movq    $8, %rdi            # s2->s1 = malloc(sizeof(S1));
    callq   malloc@PLT
    addq    $8, %rsp
    movq    %rax, (%rbx)
    movq    (%rbx), %rax        # s2->s1->j = 1;
    addq    $4, %rax
    movl    $1, (%rax)
    subq    $8, %rsp
    movq    (%rbx), %rax        # printf("%d\n", s2->s1->j);
    movl    4(%rax), %esi
    leaq    .SL0(%rip), %rax
    movq    %rax, %rdi
    movslq  %esi, %rsi
    movb    $0, %al
    callq   printf@PLT
    addq    $8, %rsp
    movq    $0, %rax            # Function exit code zero
    popq    %rbx                # Function epilogue
    leaveq
    retq
```

# Improvements
This is a short summary of I have in mind to improve

- More tree tiling rules, e.g. the `inc` instruction and `mov` instructions with better addressing
- More optimizations using the SSA representation
- Scalar optimizations
- Redundancy elimination
- Common subexpresion elimination
- Code layout
- Better spill code generation for specific cases
- Consider using peephole optimization, esp useful for spill code
- Reconsider if SSA interference constraint for rsub `dst == src1` can be avoided. The IR doesn't allow `dst == src1` nor `dst == src2`, yet x86_64 is a two-operand arch. Both approaches lead to a lot of e.g. mov/add ops. Consider if this can't be improved.

So far, I've been ad libbing through [Engineering a Compiler 2nd Edition](https://www.amazon.com/Engineering-Compiler-Keith-Cooper/dp/012088478X). I'd like to focus on the areas I have not yet touched.

# History
The project started out as a clone of [c4](https://github.com/rswier/c4) to teach myself to write it from scratch. I then went down a route based on [TCC](https://bellard.org/tcc/) where I wrote a code generator that outputted an object (`.o`) file. It then quickly became clear that generating object code without using an assembler is a waste of time, so I adapted it to produce `.s` files and compiled them with gcc. I then proceeded implemeting [Sebastian Falbesoner's](https://www.complang.tuwien.ac.at/Diplomarbeiten/falbesoner14.pdf) approach to register allocation. At this point I went academic and started reading [Engineering a Compiler 2nd Edition](https://www.amazon.com/Engineering-Compiler-Keith-Cooper/dp/012088478X). First SSA transformation, then graph coloring register allocation, then finally instruction selection using tree pattern matching.