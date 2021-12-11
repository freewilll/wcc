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

OK_FILENAMES := $(addsuffix -ok, ${CPP_TESTS})
RESULT_FILENAMES := $(addsuffix -result.c, ${CPP_TESTS})


all: ${OK_FILENAMES}

%-ok: %.c %-output.c ../../wcc ${CPP_TESTS_INCLUDES}
	@# Run preprocessor & diff with expected result
	../../wcc -E $< -o $*-result.c
	diff $*-output.c $*-result.c

	@# check re-preprocessing produces the same result
	../../wcc -E $*-result.c -o $*-result2.c
	diff -I '^#' $*-result.c $*-result2.c

	touch $*-ok

.PHONY: regen
regen:
	$(foreach var,$(CPP_TESTS),../../wcc -E $(var).c -o $(var)-output.c;)

clean:
	@rm -f *-result*.c
	@rm -f *-ok
