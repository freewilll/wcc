GCC ?= gcc

all: test-unit test-integration test-e2e test-cpp

COMMON_DEPENDENCIES := ${BUILD_DIR}/tests/test-utils.s ${BUILD_DIR}/tests/test-lib.o
MAKE_ARGS := SRC_DIR=${SRC_DIR} BUILD_DIR=${BUILD_DIR} GCC=${GCC}

${BUILD_DIR}/tests/test-lib.o: ${SRC_DIR}/tests/test-lib.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -c $< -o $@

${BUILD_DIR}/tests/test-utils.s: ${SRC_DIR}/tests/test-utils.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -c -S $< -o $@

.PHONY: test-unit
test-unit: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit ${MAKE_ARGS} all

.PHONY: test-unit-parser
test-unit-parser: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit ${MAKE_ARGS} run-test-parser

.PHONY: test-unit-ssa
test-unit-ssa: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit ${MAKE_ARGS} run-test-ssa

.PHONY: test-unit-cpp
test-unit-cpp: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit ${MAKE_ARGS} run-test-cpp

.PHONY: test-unit-types
test-unit-types: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit ${MAKE_ARGS} run-test-types

.PHONY: test-integration
test-integration: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/integration ${MAKE_ARGS} all

.PHONY: test-cpp
test-cpp:
	${MAKE} -C ${SRC_DIR}/tests/cpp ${MAKE_ARGS} all

.PHONY: regen-cpp-tests-output
regen-cpp-tests-output:
	${MAKE} -C ${SRC_DIR}/tests/cpp ${MAKE_ARGS} regen-cpp-tests-output

.PHONY: test-e2e
test-e2e: ${BUILD_DIR}/tests/test-lib.o
	${MAKE} -C ${SRC_DIR}/tests/e2e ${MAKE_ARGS} all

clean:
	${MAKE} -C ${SRC_DIR}/tests/unit ${MAKE_ARGS} clean
	${MAKE} -C ${SRC_DIR}/tests/integration ${MAKE_ARGS} clean
	${MAKE} -C ${SRC_DIR}/tests/e2e ${MAKE_ARGS} clean
	${MAKE} -C ${SRC_DIR}/tests/cpp ${MAKE_ARGS} clean

	@rm -f ${BUILD_DIR}/tests/*.s
	@rm -f ${BUILD_DIR}/tests/*.o
	@rm -f ${BUILD_DIR}/tests/core
