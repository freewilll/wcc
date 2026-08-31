GCC ?= gcc

all: test-unit test-integration

.PHONY: test-unit
test-unit: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/target/aarch64/tests/unit all

.PHONY: test-integration
test-integration: ${COMMON_DEPENDENCIES}
	${MAKE} -C ${SRC_DIR}/target/aarch64/tests/integration all

clean:
	${MAKE} -C ${SRC_DIR}/target/aarch64/tests/unit clean
	${MAKE} -C ${SRC_DIR}/target/aarch64/tests/integration clean

	@rm -f ${BUILD_DIR}/target/aarch64/tests/*.s
	@rm -f ${BUILD_DIR}/target/aarch64/tests/*.o
	@rm -f ${BUILD_DIR}/target/aarch64/tests/core
