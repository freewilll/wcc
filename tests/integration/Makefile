GCC ?= gcc

WCC_RUN_FLAGS = -I ${SRC_DIR}/include
TEST_BUILD_DIR = ${BUILD_DIR}/tests/integration
INTEGRATION_TEST_FLAGS := ${GCC_OPTS} -g -I ${BUILD_DIR}

all: run-test-instrsel run-test-errors check-test-debug-headers

${TEST_BUILD_DIR}/test-instrsel: test-instrsel.c ${SRC_DIR}/tests/test-utils.c ${BUILD_DIR}/libwcc.a
	@mkdir -p $(@D)
	${GCC} ${INTEGRATION_TEST_FLAGS} $^ -o $@

${TEST_BUILD_DIR}/test-errors: test-errors.c ${BUILD_DIR}/wcc ${BUILD_DIR}/tests/test-lib.o
	@mkdir -p $(@D)
	${GCC} ${INTEGRATION_TEST_FLAGS} -D WCC='"${BUILD_DIR}/wcc -I ${SRC_DIR}/include"' $< ${BUILD_DIR}/tests/test-lib.o -o $@

run-%: ${TEST_BUILD_DIR}/%
	cd ${TEST_BUILD_DIR} && ./$(notdir $<)

${TEST_BUILD_DIR}/test-debug: test-debug.c ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_OPTS} ${WCC_RUN_FLAGS} -g $< -o $@

check-test-debug-headers: ${TEST_BUILD_DIR}/test-debug
	objdump -h $< | grep .debug_info > /dev/null
	objdump -h $< | grep .debug_line > /dev/null
	objdump -h $< | grep .debug_abbrev > /dev/null
	objdump -h $< | grep .debug_str > /dev/null

	objdump -g $< | grep -E 'DW_AT_producer.*wcc' > /dev/null
	objdump -g $< | grep 'Line Number Statements:' > /dev/null

clean:
	@rm -f ${TEST_BUILD_DIR}/*.s
	@rm -f ${TEST_BUILD_DIR}/*.o
	@rm -f ${TEST_BUILD_DIR}/core
	@rm -f ${TEST_BUILD_DIR}/test-instrsel
	@rm -f ${TEST_BUILD_DIR}/test-errors
	@rm -f ${TEST_BUILD_DIR}/instrsel-tests.rulecov
	@rm -f ${TEST_BUILD_DIR}/test-debug
	@rm -f ${TEST_BUILD_DIR}/a.out
