TEST_BUILD_DIR = ${BUILD_DIR}/tests/cpp

CPP_TESTS = \
	empty \
	empty-line \
	one-line \
	backslash-newlines \
	string-literals \
	whitespace \
	hashes \
	one-empty-line \
	empty-line-spacing \
	empty-line-spacing-7-lines \
	empty-line-spacing-8-lines \
	empty-line-spacing-9-lines \
	empty-line-spacing-18-38-lines \
	empty-line-spacing-42-lines \
	token-spacing \
	object-like-macros \
	function-like-macros \
	newlines-and-macros \
	stringizing-operator \
	token-paste-operator \
	conditional-includes \
	comments \
	inception1 \
	includes \
	line \
	c99-examples \
	null \
	pragma


CPP_TESTS_INCLUDES = \
	include.h \
	inception2.h \
	inception3.h

OK_FILENAMES := $(addprefix ${TEST_BUILD_DIR}/,$(addsuffix -ok, ${CPP_TESTS}))
RESULT_FILENAMES := $(addprefix ${TEST_BUILD_DIR}/,$(addsuffix -result.c, ${CPP_TESTS}))

all: ${OK_FILENAMES}

${TEST_BUILD_DIR}/%-ok: %.c %-output.c ${BUILD_DIR}/wcc ${CPP_TESTS_INCLUDES}
	@# Run the preprocessor and diff with the expected result.
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_OPTS} -E $< -o ${TEST_BUILD_DIR}/$*-result.c
	diff $*-output.c ${TEST_BUILD_DIR}/$*-result.c

	@# Re-preprocessing should produce the same result.
	${BUILD_DIR}/wcc ${WCC_OPTS} -E ${TEST_BUILD_DIR}/$*-result.c -o ${TEST_BUILD_DIR}/$*-result2.c
	diff -I '^#' ${TEST_BUILD_DIR}/$*-result.c ${TEST_BUILD_DIR}/$*-result2.c

	touch $@

.PHONY: regen-cpp-tests-output
regen-cpp-tests-output:
	$(foreach var,$(CPP_TESTS),${BUILD_DIR}/wcc ${WCC_OPTS} -E $(var).c -o $(var)-output.c;)

clean:
	@rm -f ${TEST_BUILD_DIR}/*-result*.c
	@rm -f ${TEST_BUILD_DIR}/*-ok
