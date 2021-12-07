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
	token-spacing \
	object-like-macros \
	function-like-macros \
	newlines-and-macros \
	stringizing-operator \
	token-paste-operator \
	conditional-includes \
	comments \
	inception1

CPP_TESTS_INCLUDES = \
	include.h \
	inception2.h \
	inception3.h

OK_FILENAMES := $(addsuffix -ok, ${CPP_TESTS})
RESULT_FILENAMES := $(addsuffix -result.c, ${CPP_TESTS})


all: ${OK_FILENAMES}

%-ok: %.c %-output.c ../../wcc ${CPP_TESTS_INCLUDES}
	@# Run preprocessor & diff with expected result
	../../wcc -E $< | grep -v '^#' > $*-result.c | true
	diff $*-output.c $*-result.c

	@# check re-preprocessing produces the same result
	../../wcc -E $*-result.c | grep -v '^#' > $*-result2.c | true
	diff $*-result.c $*-result2.c

	touch $*-ok

.PHONY: regen
regen:
	$(foreach var,$(CPP_TESTS),../../wcc -E $(var).c | grep -v '^#' > $(var)-output.c;)

clean:
	@rm -f *-result*.c
	@rm -f *-ok
