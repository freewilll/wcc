all: wcc wcc3 benchmark

SOURCES = \
  main.c \
  wcc.c \
  lexer.c \
  parser.c \
  types.c \
  ir.c \
  ssa.c \
  regalloc.c \
  instrsel.c \
  instrutil.c \
  instrrules.c \
  codegen.c \
  utils.c \
  set.c \
  stack.c \
  graph.c \
  externals.c

ASSEMBLIES := ${SOURCES:c=s}
OBJECTS := ${SOURCES:c=o}

build:
	@mkdir -p build/wcc2
	@mkdir -p build/wcc3

externals.c: externals.h
	echo "// Auto generated, don't edit" > externals.c
	echo "" >> externals.c
	echo "char *externals() {" >> externals.c
	cat externals.h | sed ':a;N;$$!ba;s/\n/\\n/g;s/^/    return "/;s/$$/";/' >> externals.c
	echo "}" >> externals.c

%.o: %.c wcc.h build
	gcc ${GCC_OPTS} -c $< -o $@ -g -Wno-return-type -Wunused -D _GNU_SOURCE

wcc: ${OBJECTS} wcc.h
	gcc ${GCC_OPTS} ${OBJECTS} -o wcc -g -Wno-return-type -D _GNU_SOURCE

# wcc2
WCC2_SOURCES := ${SOURCES:%=build/wcc2/%}
WCC2_ASSEMBLIES := ${WCC2_SOURCES:.c=.s}

build/wcc2/%.s: %.c wcc
	./wcc ${WCC_OPTS} --rule-coverage-file wcc2.rulecov -c $< -S -o $@

wcc2: ${WCC2_ASSEMBLIES}
	gcc ${GCC_OPTS} ${WCC2_ASSEMBLIES} -o wcc2

# wcc3
WCC3_SOURCES := ${SOURCES:%=build/wcc3/%}
WCC3_ASSEMBLIES := ${WCC3_SOURCES:.c=.s}

build/wcc3/%.s: %.c wcc2
	./wcc2 ${WCC_OPTS} -c $< -S -o $@

wcc3: ${WCC3_ASSEMBLIES}
	gcc ${GCC_OPTS} ${WCC3_ASSEMBLIES} -o wcc3

# tests
WCC_TESTS=\
	expr \
	unsigned-integers \
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
	regressions \
	reg-move-torture

stack-check.o: stack-check.c
	gcc ${GCC_OPTS} stack-check.c -c

test-lib.o: test-lib.c
	gcc ${GCC_OPTS} test-lib.c -c

test-wcc.s: test-main.c wcc
	./wcc ${WCC_OPTS} -c -S test-main.c

test-wcc-%.s: test-wcc-%.c wcc
	./wcc ${WCC_OPTS} --rule-coverage-file wcc-tests.rulecov --Wno-integer-constant-too-large -c -S $<

test-wcc-%-wcc: test-wcc-%.s stack-check.o test-lib.o
	gcc ${GCC_OPTS} $< stack-check.o test-lib.o -o $@

run-test-wcc-%-wcc: test-wcc-%-wcc
	./$<
	touch run-$<

gen-test-wcc-reg-move-torture: gen-test-wcc-reg-move-torture.c
	gcc ${GCC_OPTS} -Wunused -D _GNU_SOURCE gen-test-wcc-reg-move-torture.c -o gen-test-wcc-reg-move-torture

test-wcc-reg-move-torture.c: gen-test-wcc-reg-move-torture
	./gen-test-wcc-reg-move-torture

.PHONY: run-test-wcc
run-test-wcc: ${WCC_TESTS:%=run-test-wcc-%-wcc}
	@echo wcc tests passed

test-wcc-%-gcc: test-wcc-%.c stack-check.o test-lib.o
	gcc ${GCC_OPTS} $< stack-check.o test-lib.o -o $@ -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-overflow -Wno-format -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -D _GNU_SOURCE

run-test-wcc-%-gcc: test-wcc-%-gcc
	./$<
	touch run-$<

.PHONY: run-test-wcc-gcc
run-test-wcc-gcc: ${WCC_TESTS:%=run-test-wcc-%-gcc}
	@echo gcc tests passed

benchmark: wcc wcc2 benchmark.c
	gcc ${GCC_OPTS} benchmark.c -o benchmark

run-benchmark: benchmark
	./benchmark

test-self-compilation: ${WCC2_ASSEMBLIES} ${WCC3_ASSEMBLIES}
	cat build/wcc2/*.s > build/wcc2/all-s
	cat build/wcc3/*.s > build/wcc3/all-s
	diff build/wcc2/all-s build/wcc3/all-s
	@echo self compilation test passed

test-include/test-include: wcc test-include/include.h test-include/main.c test-include/foo.c
	cd test-include && ../wcc ${WCC_OPTS} main.c foo.c -o test-include

run-test-include: test-include/test-include
	test-include/test-include

test-lexer: wcc lexer.c parser.c ir.c codegen.c instrutil.c types.c utils.c test-lexer.c
	./wcc ${WCC_OPTS} lexer.c parser.c ir.c codegen.c instrutil.c types.c utils.c test-lexer.c -o test-lexer

test-lexer-gcc: lexer.c parser.c ir.c codegen.c instrutil.c types.c utils.c test-lexer.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -g -o test-lexer-gcc lexer.c parser.c ir.c codegen.c instrutil.c types.c utils.c test-lexer.c

run-test-lexer: test-lexer
	 ./test-lexer

run-test-lexer-gcc: test-lexer-gcc
	 ./test-lexer-gcc

test-set: wcc set.c utils.c test-set.c
	./wcc ${WCC_OPTS} set.c utils.c test-set.c -o test-set

test-set-gcc: set.c utils.c test-set.c
	gcc ${GCC_OPTS} -g -o test-set-gcc set.c utils.c test-set.c

run-test-set: test-set
	 ./test-set

run-test-set-gcc: test-set-gcc
	 ./test-set-gcc

test-utils.s: wcc test-utils.c
	./wcc ${WCC_OPTS} -c -S test-utils.c -o test-utils.s

test-ssa.s: wcc test-ssa.c
	./wcc ${WCC_OPTS} -c -S test-ssa.c -o test-ssa.s

test-ssa: wcc test-ssa.s test-utils.s build/wcc2/lexer.s build/wcc2/parser.s build/wcc2/types.s build/wcc2/ir.s build/wcc2/ssa.s build/wcc2/regalloc.s build/wcc2/instrsel.s build/wcc2/instrutil.s build/wcc2/instrrules.s build/wcc2/codegen.s build/wcc2/wcc.s build/wcc2/utils.s build/wcc2/set.s build/wcc2/stack.s build/wcc2/graph.s build/wcc2/externals.s
	gcc ${GCC_OPTS} -g -o test-ssa test-ssa.s test-utils.s build/wcc2/lexer.s build/wcc2/parser.s build/wcc2/types.s build/wcc2/ir.s build/wcc2/ssa.s build/wcc2/regalloc.s build/wcc2/instrsel.s build/wcc2/instrutil.s build/wcc2/instrrules.s build/wcc2/codegen.s build/wcc2/wcc.s build/wcc2/utils.s build/wcc2/set.s build/wcc2/stack.s build/wcc2/graph.s build/wcc2/externals.s

run-test-ssa: test-ssa
	 ./test-ssa

test-ssa-gcc: test-ssa.c test-utils.s lexer.c parser.c types.c ir.c ssa.c regalloc.c instrsel.c instrutil.c instrrules.c codegen.c wcc.c utils.c set.c stack.c graph.c externals.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-ssa-gcc test-ssa.c test-utils.c lexer.c parser.c types.c ir.c ssa.c regalloc.c instrsel.c instrutil.c instrrules.c codegen.c wcc.c utils.c set.c stack.c graph.c externals.c

run-test-ssa-gcc: test-ssa-gcc
	 ./test-ssa-gcc

test-instrsel.s: wcc test-instrsel.c
	./wcc ${WCC_OPTS} -c -S test-instrsel.c -o test-instrsel.s

test-instrsel: wcc test-instrsel.s test-utils.s build/wcc2/lexer.s build/wcc2/parser.s build/wcc2/types.s build/wcc2/ir.s build/wcc2/ssa.s build/wcc2/regalloc.s build/wcc2/instrsel.s build/wcc2/instrutil.s build/wcc2/instrrules.s build/wcc2/codegen.s build/wcc2/wcc.s build/wcc2/utils.s build/wcc2/set.s build/wcc2/stack.s build/wcc2/graph.s build/wcc2/externals.s
	gcc ${GCC_OPTS} -g -o test-instrsel test-instrsel.s test-utils.s build/wcc2/lexer.s build/wcc2/parser.s build/wcc2/types.s build/wcc2/ir.s build/wcc2/ssa.s build/wcc2/regalloc.s build/wcc2/instrsel.s build/wcc2/instrutil.s build/wcc2/instrrules.s build/wcc2/codegen.s build/wcc2/wcc.s build/wcc2/utils.s build/wcc2/set.s build/wcc2/stack.s build/wcc2/graph.s build/wcc2/externals.s

run-test-instrsel: test-instrsel
	 ./test-instrsel

test-instrsel-gcc: wcc.h test-instrsel.c test-utils.c wcc.c codegen.c lexer.c parser.c types.c ir.c ssa.c regalloc.c instrsel.c instrutil.c instrrules.c utils.c set.c stack.c graph.c externals.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -g -o test-instrsel-gcc test-instrsel.c test-utils.c wcc.c codegen.c lexer.c parser.c types.c ir.c ssa.c regalloc.c instrsel.c instrutil.c instrrules.c utils.c set.c stack.c graph.c externals.c

run-test-instrsel-gcc: test-instrsel-gcc
	./test-instrsel-gcc

test-graph: wcc test-graph.c graph.c set.c stack.c ir.c utils.c
	./wcc ${WCC_OPTS} test-graph.c graph.c utils.c -o test-graph

run-test-graph: test-graph
	 ./test-graph

test-parser-gcc: test-parser.c lexer.c parser.c types.c ir.c instrutil.c codegen.c utils.c set.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-parser-gcc test-parser.c lexer.c parser.c types.c ir.c instrutil.c codegen.c utils.c set.c

run-test-parser-gcc: test-parser-gcc
	 ./test-parser-gcc

.PHONY: test
test: run-test-wcc run-test-include run-test-lexer-gcc run-test-lexer run-test-set-gcc run-test-set run-test-ssa-gcc run-test-ssa run-test-instrsel-gcc run-test-instrsel run-test-parser-gcc run-test-graph run-test-wcc-gcc test-self-compilation test-self-compilation

print-rule-coverage-report.s: wcc print-rule-coverage-report.c
	./wcc ${WCC_OPTS} -c -S print-rule-coverage-report.c -c -S -o print-rule-coverage-report.s

print-rule-coverage-report: print-rule-coverage-report.s
	gcc ${GCC_OPTS} -D _GNU_SOURCE -g -o print-rule-coverage-report print-rule-coverage-report.s lexer.c parser.c types.c ir.c instrrules.c instrutil.c codegen.c utils.c set.c

make-rule-coverage-report: print-rule-coverage-report
	./print-rule-coverage-report

clean:
	@rm -f externals.c
	@rm -f wcc
	@rm -f wcc2
	@rm -f wcc3
	@rm -f test-wcc
	@rm -f test-wcc-*-wcc
	@rm -f test-wcc-reg-move-torture.c
	@rm -f gen-test-wcc-reg-move-torture
	@rm -f run-test-wcc-*
	@rm -f test-wcc-gcc
	@rm -f test-wcc-*-gcc
	@rm -f test-lexer
	@rm -f test-lexer-gcc
	@rm -f test-set
	@rm -f test-set-gcc
	@rm -f test-ssa-gcc
	@rm -f test-instrsel
	@rm -f test-instrsel-gcc
	@rm -f test-graph
	@rm -f test-ssa
	@rm -f test-parser-gcc
	@rm -f benchmark
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f a.out
	@rm -f test-include/test-include
	@rm -Rf build
	@rm -f wcc-tests.rulecov
	@rm -f instrsel-tests.rulecov
	@rm -f wcc2.rulecov
	@rm -f print-rule-coverage-report
	@rm -f rulecov.html
