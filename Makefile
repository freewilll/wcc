all: wc4 wc43 benchmark

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
OBJECTS := ${SOURCES:c=o}

WC4_OPTS ?= "-fno-coalesce-live-range"

build:
	@mkdir -p build/wc42
	@mkdir -p build/wc43

%.o: %.c wc4.h build
	gcc -c $< -o $@ -g -Wno-return-type -D _GNU_SOURCE

wc4: ${OBJECTS} wc4.h
	gcc ${OBJECTS} -o wc4 -g -Wno-return-type -D _GNU_SOURCE

# wc42
WC42_SOURCES := ${SOURCES:%=build/wc42/%}
WC42_ASSEMBLIES := ${WC42_SOURCES:.c=.s}

build/wc42/%.s: %.c wc4
	./wc4 ${WC4_OPTS} -c $< -S -o $@

wc42: ${WC42_ASSEMBLIES}
	gcc ${WC42_ASSEMBLIES} -o wc42

# wc43
WC43_SOURCES := ${SOURCES:%=build/wc43/%}
WC43_ASSEMBLIES := ${WC43_SOURCES:.c=.s}

build/wc43/%.s: %.c wc42
	./wc42 ${WC4_OPTS} -c $< -S -o $@

wc43: ${WC43_ASSEMBLIES}
	gcc ${WC43_ASSEMBLIES} -o wc43

# tests
WC4_TESTS=\
	expr \
	function-call-args \
	func-calls \
	pointers \
	memory-functions \
	inc-dec \
	conditionals \
	structs \
	typedef \
	pointer-arithmetic \
	loops \
	enums \
	regressions

stack-check.o: stack-check.c
	gcc stack-check.c -c

test-lib.o: test-lib.c
	gcc test-lib.c -c

test-wc4.s: test-wc4.c wc4
	./wc4 ${WC4_OPTS} -c -S test-wc4.c

test-wc4-%.s: test-wc4-%.c wc4
	./wc4 ${WC4_OPTS} -c -S $<

test-wc4-%-wc4: test-wc4-%.s stack-check.o test-lib.o
	gcc $< stack-check.o test-lib.o -o $@

run-test-wc4-%-wc4: test-wc4-%-wc4
	./$<
	touch run-$<

.PHONY: run-test-wc4
run-test-wc4: ${WC4_TESTS:%=run-test-wc4-%-wc4}
	@echo wc4 tests passed

test-wc4-%-gcc: test-wc4-%.c stack-check.o test-lib.o
	gcc $< stack-check.o test-lib.o -o $@ -Wno-int-conversion -Wno-incompatible-pointer-types -D _GNU_SOURCE

run-test-wc4-%-gcc: test-wc4-%-gcc
	./$<
	touch run-$<

.PHONY: run-test-wc4-gcc
run-test-wc4-gcc: ${WC4_TESTS:%=run-test-wc4-%-gcc}
	@echo gcc tests passed

benchmark: wc4 wc42 benchmark.c
	gcc benchmark.c -o benchmark

run-benchmark: benchmark
	./benchmark

test-self-compilation: ${WC42_ASSEMBLIES} ${WC43_ASSEMBLIES}
	cat build/wc42/*.s > build/wc42/all-s
	cat build/wc43/*.s > build/wc43/all-s
	diff build/wc42/all-s build/wc43/all-s
	@echo self compilation test passed

test-include/test-include: wc4 test-include/include.h test-include/main.c test-include/foo.c
	cd test-include && ../wc4 ${WC4_OPTS} main.c foo.c -o test-include

run-test-include: test-include/test-include
	test-include/test-include

test-set: wc4 set.c utils.c test-set.c
	./wc4 ${WC4_OPTS} set.c utils.c test-set.c -o test-set

run-test-set: test-set
	 ./test-set

test-ssa.s: wc4 test-ssa.c
	./wc4 ${WC4_OPTS} -c -S test-ssa.c -o test-ssa.s

test-utils.s: wc4 test-utils.c
	./wc4 ${WC4_OPTS} -c -S test-utils.c -o test-utils.s

test-ssa: wc4 test-ssa.s test-utils.s build/wc42/lexer.s build/wc42/parser.s build/wc42/ir.s build/wc42/ssa.s build/wc42/instrsel.s build/wc42/instrrules.s build/wc42/codegen.s build/wc42/utils.s build/wc42/set.s build/wc42/stack.s build/wc42/graph.s
	gcc -o test-ssa test-ssa.s test-utils.s build/wc42/lexer.s build/wc42/parser.s build/wc42/ir.s build/wc42/ssa.s build/wc42/instrsel.s build/wc42/instrrules.s build/wc42/codegen.s build/wc42/utils.s build/wc42/set.s build/wc42/stack.s build/wc42/graph.s

run-test-ssa: test-ssa
	 ./test-ssa

test-instrsel.s: wc4 test-instrsel.c
	./wc4 ${WC4_OPTS} -c -S test-instrsel.c -o test-instrsel.s

test-instrsel: wc4 test-instrsel.s test-utils.s build/wc42/lexer.s build/wc42/parser.s build/wc42/ir.s build/wc42/ssa.s build/wc42/instrsel.s build/wc42/instrrules.s build/wc42/codegen.s build/wc42/utils.s build/wc42/set.s build/wc42/stack.s build/wc42/graph.s
	gcc -o test-instrsel test-instrsel.s test-utils.s build/wc42/lexer.s build/wc42/parser.s build/wc42/ir.s build/wc42/ssa.s build/wc42/instrsel.s build/wc42/instrrules.s build/wc42/codegen.s build/wc42/utils.s build/wc42/set.s build/wc42/stack.s build/wc42/graph.s

run-test-instrsel: test-instrsel
	 ./test-instrsel

test-instrsel-gcc: wc4.h test-instrsel.c test-utils.c codegen.c lexer.c parser.c ir.c ssa.c instrsel.c instrrules.c utils.c set.c stack.c graph.c
	gcc -o test-instrsel-gcc test-instrsel.c test-utils.c codegen.c lexer.c parser.c ir.c ssa.c instrsel.c instrrules.c utils.c set.c stack.c graph.c -D _GNU_SOURCE

run-test-instrsel-gcc: test-instrsel-gcc
	./test-instrsel-gcc

test-graph: wc4 test-graph.c graph.c set.c stack.c ir.c utils.c
	./wc4 ${WC4_OPTS} test-graph.c graph.c utils.c -o test-graph

run-test-graph: test-graph
	 ./test-graph

.PHONY: test
test: run-test-wc4 run-test-include run-test-set run-test-ssa run-test-instrsel run-test-graph run-test-wc4-gcc test-self-compilation test-self-compilation

clean:
	@rm -f wc4
	@rm -f wc42
	@rm -f wc43
	@rm -f test-wc4
	@rm -f test-wc4-*-wc4
	@rm -f run-test-wc4-*
	@rm -f test-wc4-gcc
	@rm -f test-wc4-*-gcc
	@rm -f test-set
	@rm -f test-instrsel
	@rm -f test-instrsel-gcc
	@rm -f test-graph
	@rm -f test-ssa
	@rm -f benchmark
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f a.out
	@rm -f test-include/test-include
	@rm -Rf build
