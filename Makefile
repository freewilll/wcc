all: wc4 wc42 wc43 benchmark

SOURCES = \
  wc4.c \
  lexer.c \
  parser.c \
  ir.c \
  ssa.c \
  instrsel.c \
  instrrules.c \
  codegen.c \
  utils.c \
  set.c \
  stack.c \
  graph.c \

ASSEMBLIES := ${SOURCES:c=s}

build:
	@mkdir -p build/wc42
	@mkdir -p build/wc43

wc4: ${SOURCES} wc4.h build
	gcc ${SOURCES} -o wc4 -g -Wno-return-type -D _GNU_SOURCE

# wc42
WC42_SOURCES := ${SOURCES:%=build/wc42/%}
WC42_ASSEMBLIES := ${WC42_SOURCES:.c=.s}

build/wc42/%.s: %.c wc4
	./wc4 -c $< -S -o $@

wc42: ${WC42_ASSEMBLIES}
	gcc ${WC42_ASSEMBLIES} -o wc42

# wc43
WC43_SOURCES := ${SOURCES:%=build/wc43/%}
WC43_ASSEMBLIES := ${WC43_SOURCES:.c=.s}

build/wc43/%.s: %.c wc42
	./wc42 -c $< -S -o $@

wc43: ${WC43_ASSEMBLIES}
	gcc ${WC43_ASSEMBLIES} -o wc43

# tests
stack-check.o: stack-check.c
	gcc stack-check.c -c

test-wc4.s: wc4 test-wc4.c
	./wc4 -c -S test-wc4.c

test-wc4: test-wc4.s stack-check.o
	gcc test-wc4.s stack-check.o -o test-wc4

benchmark: wc4 wc42 benchmark.c
	gcc benchmark.c -o benchmark

run-benchmark: benchmark
	./benchmark

.PHONY: run-test-wc4
run-test-wc4: test-wc4
	./test-wc4
	@echo wc4 tests passed

test-wc4-gcc: test-wc4.c
	gcc test-wc4.c stack-check.o -o test-wc4-gcc -Wno-int-conversion -Wno-incompatible-pointer-types

.PHONY: run-test-wc4-gcc
run-test-wc4-gcc: test-wc4-gcc
	./test-wc4-gcc
	@echo gcc tests passed

test-self-compilation: ${WC42_ASSEMBLIES} ${WC43_ASSEMBLIES}
	cat build/wc42/*.s > build/wc42/all-s
	cat build/wc43/*.s > build/wc43/all-s
	diff build/wc42/all-s build/wc43/all-s
	@echo self compilation test passed

test-include/test-include: wc4 test-include/include.h test-include/main.c test-include/foo.c
	cd test-include && ../wc4 main.c foo.c -o test-include

run-test-include: test-include/test-include
	test-include/test-include

test-set: wc4 set.c utils.c test-set.c
	./wc4 set.c utils.c test-set.c -o test-set

run-test-set: test-set
	 ./test-set

test-ssa: wc4 test-ssa.c test-utils.c ssa.c set.c stack.c graph.c ir.c utils.c
	./wc4 test-ssa.c test-utils.c ssa.c set.c stack.c graph.c ir.c utils.c -o test-ssa

run-test-ssa: test-ssa
	 ./test-ssa

test-instrsel: wc4 test-instrsel.c test-utils.c instrsel.c instrrules.c ir.c graph.c set.c ssa.c stack.c utils.c
	./wc4 test-instrsel.c test-utils.c instrsel.c instrrules.c ir.c graph.c set.c ssa.c stack.c utils.c -o test-instrsel

run-test-instrsel: test-instrsel
	 ./test-instrsel

test-graph: wc4 test-graph.c graph.c set.c stack.c ir.c utils.c
	./wc4 test-graph.c graph.c utils.c -o test-graph

run-test-graph: test-graph
	 ./test-graph

test-codegen: wc4 test-codegen.c instrsel.c instrrules.c codegen.c utils.c ir.c ssa.c set.c stack.c graph.c parser.c lexer.c
	./wc4 test-codegen.c instrsel.c instrrules.c codegen.c utils.c ir.c ssa.c set.c stack.c graph.c parser.c lexer.c -o test-codegen

run-test-codegen: test-codegen
	 ./test-codegen

.PHONY: test
test: run-test-wc4 run-test-include run-test-set run-test-ssa run-test-instrsel run-test-graph run-test-codegen run-test-wc4-gcc test-self-compilation

clean:
	@rm -f wc4
	@rm -f wc42
	@rm -f wc43
	@rm -f test-wc4
	@rm -f test-wc4-gcc
	@rm -f test-set
	@rm -f test-instrsel
	@rm -f test-graph
	@rm -f test-ssa
	@rm -f test-codegen
	@rm -f benchmark
	@rm -f *.s
	@rm -f *.o
	@rm -f test.c test test1.c test1 test2.c test2
	@rm -f core
	@rm -f a.out
	@rm -f test-include/test-include
	@rm -Rf build
