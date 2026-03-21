export SRC_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
export BUILD_DIR := $(CURDIR)
CONFIG ?= $(BUILD_DIR)/config.mk

-include $(CONFIG)

VERSION = 0
INSTALL_BIN_DIR = ${PREFIX}/bin
INSTALL_LIB_DIR = ${PREFIX}/lib/wcc/${VERSION}
INSTALL_LIB_INCLUDE_DIR = ${INSTALL_LIB_DIR}/include

WCC_BUILD_FLAGS := -D INSTALL_LIB_DIR='"${INSTALL_LIB_DIR}"'
WCC_SRC_INCLUDE := -I ${SRC_DIR}/include
WCC_SELFHOST_FLAGS := ${WCC_OPTS} --fail-on-leaked-memory
WCC_RULE_COVERAGE_FLAGS := --rule-coverage-file wcc2.rulecov

all: wcc

GCC ?= gcc

SOURCES := \
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
	cpp.c

MISC_SOURCES := instrrules-generated.c internals.c wcc.c main.c
SOURCES_ABS_PATH := ${SOURCES:%=${SRC_DIR}/%}
ASSEMBLIES := ${SOURCES:c=s}
OBJECTS := ${SOURCES:c=o}

build:
	@mkdir -p ${BUILD_DIR}/build/wcc2
	@mkdir -p ${BUILD_DIR}/build/wcc3

make-internals: ${SRC_DIR}/make-internals.c
	${GCC} ${GCC_OPTS} $< -o $@

internals.c: ${SRC_DIR}/internals.h make-internals
	./make-internals ${SRC_DIR}/internals.h > internals.c

internals.o: internals.c
	${GCC} ${GCC_OPTS} -c $< -o $@

instrgen: ${SOURCES_ABS_PATH} ${SRC_DIR}/instrgen.c ${SRC_DIR}/instrrules.c
	${GCC} ${GCC_OPTS} -Wno-return-type ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} ${SOURCES_ABS_PATH} ${SRC_DIR}/instrgen.c ${SRC_DIR}/instrrules.c -o $@

instrrules-generated.c: instrgen
	./instrgen > instrrules-generated.c

instrrules-generated.o: instrrules-generated.c
	${GCC} ${GCC_OPTS} -g -Wunused ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} -I ${SRC_DIR} -c $< -o $@

%.o: ${SRC_DIR}/%.c ${BUILD_DIR}/config.h ${SRC_DIR}/wcc.h build
	${GCC} ${GCC_OPTS} -g -Wunused ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} -c $< -o $@

libwcc.a: ${OBJECTS} wcc.o instrrules-generated.o internals.o
	ar rcs libwcc.a ${OBJECTS} wcc.o instrrules-generated.o internals.o

wcc: libwcc.a ${SRC_DIR}/main.c instrrules-generated.c config.h ${SRC_DIR}/wcc.h
	${GCC} ${GCC_OPTS} -g -Wunused -Wno-return-type ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} -I ${SRC_DIR} ${SRC_DIR}/main.c instrrules-generated.c libwcc.a -o $@

# wcc2
WCC2_SOURCES := ${SOURCES:%=build/wcc2/%}
WCC2_ASSEMBLIES := ${WCC2_SOURCES:.c=.s}
WCC2_MISC_SOURCES := ${MISC_SOURCES:%=build/wcc2/%}
WCC2_MISC_ASSEMBLIES := ${WCC2_MISC_SOURCES:.c=.s}

build/wcc2/instrrules-generated.s: instrrules-generated.c wcc
	./wcc ${WCC_SELFHOST_FLAGS} ${WCC_RULE_COVERAGE_FLAGS} ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} ${WCC_SRC_INCLUDE} -I ${SRC_DIR} -c $< -S -o $@

build/wcc2/internals.s: internals.c wcc
	./wcc ${WCC_SELFHOST_FLAGS} ${WCC_RULE_COVERAGE_FLAGS} ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} ${WCC_SRC_INCLUDE} -I ${SRC_DIR} -c $< -S -o $@

build/wcc2/%.s: ${SRC_DIR}/%.c wcc
	./wcc ${WCC_SELFHOST_FLAGS} ${WCC_RULE_COVERAGE_FLAGS} ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} ${WCC_SRC_INCLUDE} -c $< -S -o $@

wcc2: ${WCC2_ASSEMBLIES} ${WCC2_MISC_ASSEMBLIES} wcc
	./wcc ${WCC_OPTS} ${WCC2_ASSEMBLIES} ${WCC2_MISC_ASSEMBLIES} -o wcc2

# wcc3
WCC3_SOURCES := ${SOURCES:%=build/wcc3/%}
WCC3_ASSEMBLIES := ${WCC3_SOURCES:.c=.s}
WCC3_MISC_SOURCES := ${MISC_SOURCES:%=build/wcc3/%}
WCC3_MISC_ASSEMBLIES := ${WCC3_MISC_SOURCES:.c=.s}

build/wcc3/instrrules-generated.s: instrrules-generated.c wcc2
	./wcc2 ${WCC_OPTS} ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} ${WCC_SRC_INCLUDE} -I ${SRC_DIR} -c $< -S -o $@

build/wcc3/internals.s: internals.c wcc2
	./wcc2 ${WCC_OPTS} ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} ${WCC_SRC_INCLUDE} -I ${SRC_DIR} -c $< -S -o $@

build/wcc3/%.s: ${SRC_DIR}/%.c wcc2
	./wcc2 ${WCC_SELFHOST_FLAGS} ${WCC_BUILD_FLAGS} -I ${BUILD_DIR} ${WCC_SRC_INCLUDE} -c $< -S -o $@

wcc3: ${WCC3_ASSEMBLIES} ${WCC3_MISC_ASSEMBLIES} wcc2
	./wcc2 ${WCC_OPTS} ${WCC3_ASSEMBLIES} ${WCC3_MISC_ASSEMBLIES} -o wcc3

.PHONY: test-self-compilation
test-self-compilation: wcc2 wcc3
	cat build/wcc2/*.s > build/wcc2/all-s
	cat build/wcc3/*.s > build/wcc3/all-s
	diff build/wcc2/all-s build/wcc3/all-s
	@echo self compilation test passed

.PHONY: test
test: test-self-compilation test-all
	@echo All tests passed

.PHONY: test-all
test-all: wcc internals.c libwcc.a utils.o memory.o ${SRC_DIR}/include/stdarg.h
	${MAKE} -C ${SRC_DIR}/tests all

.PHONY: test-unit
test-unit: libwcc.a
	${MAKE} -C ${SRC_DIR}/tests test-unit

.PHONY: test-unit-parser
test-unit-parser: libwcc.a
	${MAKE} -C ${SRC_DIR}/tests test-unit-parser

.PHONY: test-unit-ssa
test-unit-ssa: libwcc.a
	${MAKE} -C ${SRC_DIR}/tests test-unit-ssa

.PHONY: test-unit-cpp
test-unit-cpp: libwcc.a
	${MAKE} -C ${SRC_DIR}/tests test-unit-cpp

.PHONY: regen-cpp-tests-output
regen-cpp-tests-output: wcc
	${MAKE} -C ${SRC_DIR}/tests regen-cpp-tests-output

.PHONY: test-unit-types
test-unit-types: libwcc.a
	${MAKE} -C ${SRC_DIR}/tests test-unit-types

.PHONY: test-integration
test-integration: libwcc.a wcc
	${MAKE} -C ${SRC_DIR}/tests test-integration

.PHONY: test-e2e
test-e2e: wcc
	${MAKE} -C ${SRC_DIR}/tests test-e2e

.PHONY: test-cpp
test-cpp: wcc
	${MAKE} -C ${SRC_DIR}/tests  test-cpp

.PHONY: run-benchmark
run-benchmark: wcc wcc2
	${MAKE} -C ${SRC_DIR}/tools run-benchmark

.PHONY: rule-coverage-report
rule-coverage-report: wcc wcc2 test
	${MAKE} -C ${SRC_DIR}/tools rule-coverage-report

install: wcc
	mkdir -p '${INSTALL_BIN_DIR}'
	cp wcc '${INSTALL_BIN_DIR}/wcc'
	cp ${SRC_DIR}/musl-wcc '${INSTALL_BIN_DIR}/'
	mkdir -p '${INSTALL_LIB_INCLUDE_DIR}'
	cp ${SRC_DIR}/include/* '${INSTALL_LIB_INCLUDE_DIR}'

clean:
	${MAKE} -C ${SRC_DIR}/tests clean
	${MAKE} -C ${SRC_DIR}/tools clean

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

# Detect out-of-source build
ifneq ($(SRC_DIR),$(BUILD_DIR))

distclean: clean
	@rm -f config.h
	@rm -f config.mk
	@rm -f Makefile
	@rm -f wcc-musl

else

distclean: clean
	@rm -f config.h
	@rm -f config.mk

endif
