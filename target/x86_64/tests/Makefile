GCC ?= gcc

all: test-unit test-integration

.PHONY: test-unit
test-unit: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/target/x86_64/tests/unit all

.PHONY: test-unit-functions
test-unit-functions: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/target/x86_64/tests/unit run-test-functions

.PHONY: test-integration
test-integration: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/target/x86_64/tests/integration all

clean:
	${MAKE} -C ${SRC_DIR}/target/x86_64/tests/unit clean
	${MAKE} -C ${SRC_DIR}/target/x86_64/tests/integration clean

	@rm -f ${BUILD_DIR}/target/x86_64/tests/*.s
	@rm -f ${BUILD_DIR}/target/x86_64/tests/*.o
	@rm -f ${BUILD_DIR}/target/x86_64/tests/core
