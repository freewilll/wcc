CPP_TESTS = \
	empty \
	empty-line \
	one-line \
	backslash-newlines \
	whitespace \
	hashes \
	one-empty-line \
	empty-line-spacing \
	empty-line-spacing-7-lines \
	empty-line-spacing-8-lines \
	empty-line-spacing-9-lines \
	object-like-macros \
	comments

OK_FILENAMES := $(addsuffix -ok, ${CPP_TESTS})
RESULT_FILENAMES := $(addsuffix -result.c, ${CPP_TESTS})

all: ${OK_FILENAMES}

%-ok: %.c %-output.c ../../wcc
	@# Run preprocessor & diff with expected result
	../../wcc -E $< -o $*-result.c
	diff $*-output.c $*-result.c

	@# check re-preprocessing produces the same result
	../../wcc -E $*-result.c -o $*-result2.c
	diff $*-result.c $*-result2.c

	touch $*-ok

clean:
	@rm -f *-result*.c
	@rm -f *-ok
