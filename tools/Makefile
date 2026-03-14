GCC ?= gcc

TOOLS_BUILD_DIR = ${BUILD_DIR}/tools

${TOOLS_BUILD_DIR}/benchmark: ${SRC_DIR}/tools/benchmark.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} $< -o $@

run-benchmark: ${TOOLS_BUILD_DIR}/benchmark
	cd ${TOOLS_BUILD_DIR} && ./$(notdir $<)

${TOOLS_BUILD_DIR}/make-rule-coverage-report.s: ${SRC_DIR}/tools/make-rule-coverage-report.c ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_OPTS} -c -S $< -o $@

${TOOLS_BUILD_DIR}/make-rule-coverage-report: ${TOOLS_BUILD_DIR}/make-rule-coverage-report.s
	${GCC} ${GCC_OPTS} -D _GNU_SOURCE -g $< ${SRC_DIR}/lexer.c ${SRC_DIR}/parser.c ${SRC_DIR}/types.c ${SRC_DIR}/ir.c ${SRC_DIR}/instrrules.c ${SRC_DIR}/instrutil.c ${SRC_DIR}/codegen.c ${SRC_DIR}/utils.c ${SRC_DIR}/set.c -o $@

rule-coverage-report: ${TOOLS_BUILD_DIR}/make-rule-coverage-report
	cd ${TOOLS_BUILD_DIR} && ./make-rule-coverage-report

clean:
	@rm -f ${TOOLS_BUILD_DIR}/benchmark
	@rm -f ${TOOLS_BUILD_DIR}/make-rule-coverage-report
	@rm -f ${TOOLS_BUILD_DIR}/rulecov.html
	@rm -f ${TOOLS_BUILD_DIR}/*.s
	@rm -f ${TOOLS_BUILD_DIR}/*.o
	@rm -f ${TOOLS_BUILD_DIR}/core
	@rm -f ${TOOLS_BUILD_DIR}/instrsel-tests.rulecov
