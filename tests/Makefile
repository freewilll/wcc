GCC ?= gcc

all: test-unit test-integration test-e2e test-cpp

COMMON_DEPENDENCIES := ${BUILD_DIR}/tests/test-utils.s ${BUILD_DIR}/tests/test-lib.o

${BUILD_DIR}/tests/test-lib.o: ${SRC_DIR}/tests/test-lib.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -c $< -o $@

${BUILD_DIR}/tests/test-utils.s: ${SRC_DIR}/tests/test-utils.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -I ${BUILD_DIR} -c -S $< -o $@

.PHONY: test-unit
test-unit: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit all

.PHONY: test-unit-parser
test-unit-parser: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit run-test-parser

.PHONY: test-unit-ssa
test-unit-ssa: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit run-test-ssa

.PHONY: test-unit-functions
test-unit-functions: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit run-test-functions

.PHONY: test-unit-cpp
test-unit-cpp: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit run-test-cpp

.PHONY: test-unit-types
test-unit-types: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/unit run-test-types

.PHONY: test-integration
test-integration: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/integration all

.PHONY: test-integration-test-linkage-and-storage
test-integration-test-linkage-and-storage: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/tests/integration run-test-linkage-and-storage

.PHONY: test-cpp
test-cpp:
	${MAKE} -C ${SRC_DIR}/tests/cpp all

.PHONY: regen-cpp-tests-output
regen-cpp-tests-output:
	${MAKE} -C ${SRC_DIR}/tests/cpp regen-cpp-tests-output

.PHONY: test-e2e
test-e2e: ${BUILD_DIR}/tests/test-lib.o
	${MAKE} -C ${SRC_DIR}/tests/e2e all

clean:
	${MAKE} -C ${SRC_DIR}/tests/unit clean
	${MAKE} -C ${SRC_DIR}/tests/integration clean
	${MAKE} -C ${SRC_DIR}/tests/e2e clean
	${MAKE} -C ${SRC_DIR}/tests/cpp clean

	@rm -f ${BUILD_DIR}/tests/*.s
	@rm -f ${BUILD_DIR}/tests/*.o
	@rm -f ${BUILD_DIR}/tests/core
