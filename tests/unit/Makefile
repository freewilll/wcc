GCC ?= gcc

TEST_BUILD_DIR = ${BUILD_DIR}/tests/unit
UNIT_TEST_FLAGS := ${GCC_OPTS} -g
UNIT_TEST_WARN_FLAGS := -Wno-int-conversion -Wno-pointer-to-int-cast

all:  \
	run-test-lexer \
	run-test-cpp \
	run-test-set \
	run-test-list \
	run-test-strmap \
	run-test-strset \
	run-test-longmap \
	run-test-longset \
	run-test-ssa \
	run-test-parser \
	run-test-types \
	run-test-graph  \
	run-test-functions \

${TEST_BUILD_DIR}/test-lexer: test-lexer.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-cpp: test-cpp.c ${BUILD_DIR}/libwcc.a ${BUILD_DIR}/tests/test-lib.o
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-set: test-set.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-list: test-list.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-strmap: test-strmap.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-strset: test-strset.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-longmap: test-longmap.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-longset: test-longset.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-ssa: test-ssa.c ${SRC_DIR}/tests/test-utils.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} ${UNIT_TEST_WARN_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-graph: test-graph.c ${SRC_DIR}/graph.c ${SRC_DIR}/utils.c ${SRC_DIR}/memory.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} $^ -o $@

${TEST_BUILD_DIR}/test-parser: test-parser.c ${BUILD_DIR}/libwcc.a ${BUILD_DIR}/tests/test-lib.o
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} ${UNIT_TEST_WARN_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-types: test-types.c ${BUILD_DIR}/libwcc.a ${BUILD_DIR}/tests/test-lib.o
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} ${UNIT_TEST_WARN_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-functions: test-functions.c ${BUILD_DIR}/libwcc.a ${BUILD_DIR}/tests/test-lib.o
	@mkdir -p $(@D)
	${GCC} ${UNIT_TEST_FLAGS} ${UNIT_TEST_WARN_FLAGS} $^ -o $@

run-%: ${TEST_BUILD_DIR}/%
	cd ${TEST_BUILD_DIR} && ./$(notdir $<)

clean:
	@rm -f ${TEST_BUILD_DIR}/*.s
	@rm -f ${TEST_BUILD_DIR}/*.o
	@rm -f ${TEST_BUILD_DIR}/core
	@rm -f ${TEST_BUILD_DIR}/test-lexer
	@rm -f ${TEST_BUILD_DIR}/test-cpp
	@rm -f ${TEST_BUILD_DIR}/test-set
	@rm -f ${TEST_BUILD_DIR}/test-list
	@rm -f ${TEST_BUILD_DIR}/test-strmap
	@rm -f ${TEST_BUILD_DIR}/test-strset
	@rm -f ${TEST_BUILD_DIR}/test-longmap
	@rm -f ${TEST_BUILD_DIR}/test-longset
	@rm -f ${TEST_BUILD_DIR}/test-ssa
	@rm -f ${TEST_BUILD_DIR}/test-graph
	@rm -f ${TEST_BUILD_DIR}/test-parser
	@rm -f ${TEST_BUILD_DIR}/test-types
	@rm -f ${TEST_BUILD_DIR}/test-functions
