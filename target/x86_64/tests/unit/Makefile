GCC ?= gcc

TEST_BUILD_DIR = ${BUILD_DIR}/target/x86_64/tests/unit
UNIT_TEST_FLAGS := ${GCC_OPTS} -g -I ${BUILD_DIR}
UNIT_TEST_WARN_FLAGS := -Wno-int-conversion -Wno-pointer-to-int-cast

all: run-test-functions

${TEST_BUILD_DIR}/test-functions: test-functions.c ${BUILD_DIR}/libwcc.a ${BUILD_DIR}/tests/test-lib.o
	@mkdir -p $(@D)
	${GCC} -I ${SRC_DIR} -I ${SRC_DIR}/tests/ ${UNIT_TEST_FLAGS} ${UNIT_TEST_WARN_FLAGS} $^ -o $@

run-%: ${TEST_BUILD_DIR}/%
	cd ${TEST_BUILD_DIR} && ./$(notdir $<)

clean:
	@rm -f ${TEST_BUILD_DIR}/*.s
	@rm -f ${TEST_BUILD_DIR}/*.o
	@rm -f ${TEST_BUILD_DIR}/core
	@rm -f ${TEST_BUILD_DIR}/test-functions
