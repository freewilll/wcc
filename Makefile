all: wcc

SOURCES = \
  wcc.c \
  lexer.c \
  parser.c \
  constexpr.c \
  scopes.c \
  types.c \
  ir.c \
  functions.c \
  ssa.c \
  regalloc.c \
  instrsel.c \
  instrutil.c \
  instrrules-generated.c \
  codegen.c \
  utils.c \
  memory.c \
  error.c \
  set.c \
  stack.c \
  strmap.c \
  strset.c \
  longset.c \
  longmap.c \
  list.c \
  graph.c \
  internals.c \
  cpp.c

ASSEMBLIES := ${SOURCES:c=s}
OBJECTS := ${SOURCES:c=o}

BUILD_DIR = "$(shell pwd)"

build:
	@mkdir -p build/wcc2
	@mkdir -p build/wcc3

make-internals: make-internals.c
	gcc make-internals.c -o make-internals

internals.c: internals.h make-internals
	./make-internals > internals.c

instrgen: instrgen.c instrgen.c instrrules.c instrutil.c utils.c memory.c longmap.c types.c scopes.c list.c set.c strmap.c error.c cpp.c strset.c lexer.c constexpr.c parser.c functions.c ir.c codegen.c longset.c instrsel.c graph.c ssa.c stack.c config.h wcc.h
	gcc -o instrgen instrgen.c instrrules.c instrutil.c utils.c memory.c longmap.c types.c scopes.c list.c set.c strmap.c error.c cpp.c strset.c lexer.c constexpr.c parser.c functions.c ir.c codegen.c longset.c instrsel.c graph.c ssa.c stack.c  -Wno-return-type -D BUILD_DIR='${BUILD_DIR}'

instrrules-generated.c: instrgen
	./instrgen > instrrules-generated.c

%.o: %.c config.h wcc.h build
	gcc -g ${GCC_OPTS} -Wunused -c $< -o $@ -D BUILD_DIR='${BUILD_DIR}'

libwcc.a: ${OBJECTS}
	ar rcs libwcc.a ${OBJECTS}

wcc: libwcc.a main.c config.h wcc.h
	gcc ${GCC_OPTS} -Wunused  main.c libwcc.a -o wcc -g -Wno-return-type

# wcc2
WCC2_SOURCES := ${SOURCES:%=build/wcc2/%}
WCC2_ASSEMBLIES := ${WCC2_SOURCES:.c=.s}

build/wcc2/%.s: %.c wcc
	./wcc ${WCC_OPTS} --fail-on-leaked-memory --rule-coverage-file wcc2.rulecov -c $< -S -o $@ -D BUILD_DIR='${BUILD_DIR}'

wcc2: ${WCC2_ASSEMBLIES} build/wcc2/main.s wcc
	./wcc ${GCC_OPTS} ${WCC2_ASSEMBLIES} build/wcc2/main.s -o wcc2

# wcc3
WCC3_SOURCES := ${SOURCES:%=build/wcc3/%}
WCC3_ASSEMBLIES := ${WCC3_SOURCES:.c=.s}

build/wcc3/%.s: %.c wcc2
	./wcc2 ${WCC_OPTS} --fail-on-leaked-memory -c $< -S -o $@ -D BUILD_DIR='${BUILD_DIR}'

wcc3: ${WCC3_ASSEMBLIES} build/wcc3/main.s wcc2
	./wcc2 ${GCC_OPTS} ${WCC3_ASSEMBLIES} build/wcc3/main.s -o wcc3

.PHONY: test-self-compilation
test-self-compilation: ${WCC2_ASSEMBLIES} build/wcc2/main.s ${WCC3_ASSEMBLIES} build/wcc3/main.s
	cat build/wcc2/*.s > build/wcc2/all-s
	cat build/wcc3/*.s > build/wcc3/all-s
	diff build/wcc2/all-s build/wcc3/all-s
	@echo self compilation test passed

.PHONY: test-all
test-all: wcc internals.c utils.c memory.c include/stdarg.h
	cd tests && ${MAKE} all

.PHONY: test
test: test-self-compilation test-all
	@echo All tests passed

.PHONY: test-unit
test-unit: libwcc.a strmap.c longmap.c
	cd tests && ${MAKE} test-unit

.PHONY: test-unit-parser
test-unit-parser: libwcc.a
	cd tests && ${MAKE} test-unit-parser

.PHONY: test-unit-ssa
test-unit-ssa: libwcc.a
	cd tests && ${MAKE} test-unit-ssa

.PHONY: test-unit-cpp
test-unit-cpp: libwcc.a
	cd tests && ${MAKE} test-unit-cpp

.PHONY: regen-cpp-tests-output
regen-cpp-tests-output: wcc
	cd tests && ${MAKE} regen-cpp-tests-output

.PHONY: test-unit-types
test-unit-types: libwcc.a
	cd tests && ${MAKE} test-unit-types

.PHONY: test-integration
test-integration: libwcc.a wcc
	cd tests && ${MAKE} test-integration

.PHONY: test-e2e
test-e2e: wcc
	cd tests && ${MAKE} test-e2e

.PHONY: test-cpp
test-cpp: wcc
	cd tests && ${MAKE} test-cpp

.PHONY: run-benchmark
run-benchmark: wcc wcc2
	cd tools && ${MAKE} run-benchmark

.PHONY: rule-coverage-report
rule-coverage-report: wcc wcc2 test
	cd tools && ${MAKE} rule-coverage-report

clean:
	cd tests && ${MAKE} clean
	cd tools && ${MAKE} clean

	@rm -f make-internals
	@rm -f internals.c
	@rm -f instrgen
	@rm -f instrrules-generated.c
	@rm -f libwcc.a
	@rm -f wcc
	@rm -f wcc2
	@rm -f wcc3
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f a.out
	@rm -Rf build
	@rm -f wcc-tests.rulecov
	@rm -f wcc2.rulecov
	@rm -f gmon.out
	@rm -f *.gcda
	@rm -f *.gcno
	@rm -f *.gcov
	@rm -f main_coverage.info
	@rm -f prof_output

distclean: clean
	@rm -f config.h
