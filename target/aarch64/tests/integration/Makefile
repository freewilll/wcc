GCC ?= gcc

WCC_RUN_FLAGS = -I ${SRC_DIR}/include
TEST_BUILD_DIR = ${BUILD_DIR}/tests/integration
INTEGRATION_TEST_FLAGS := ${GCC_OPTS} -g -I ${BUILD_DIR}

all: run-test-instrsel

${TEST_BUILD_DIR}/test-instrsel: test-instrsel.c ${SRC_DIR}/tests/test-utils.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} -I ${SRC_DIR} ${INTEGRATION_TEST_FLAGS} $^ -o $@

run-%: ${TEST_BUILD_DIR}/%
	cd ${TEST_BUILD_DIR} && ./$(notdir $<)

clean:
	@rm -f ${TEST_BUILD_DIR}/*.s
	@rm -f ${TEST_BUILD_DIR}/*.o
	@rm -f ${TEST_BUILD_DIR}/core
	@rm -f ${TEST_BUILD_DIR}/test-functions
	@rm -f ${TEST_BUILD_DIR}/test-instrsel
	@rm -f ${TEST_BUILD_DIR}/a.out
