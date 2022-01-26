all: run-test-instrsel run-test-errors check-test-debug-headers

test-instrsel: ../../libwcc.a ../test-utils.c test-instrsel.c
	gcc ${GCC_OPTS} -g -o test-instrsel test-instrsel.c ../test-utils.c ../../libwcc.a

run-test-instrsel: test-instrsel
	./test-instrsel

test-errors: ../../wcc ../test-lib.o test-errors.c
	gcc ${GCC_OPTS} -g -o test-errors test-errors.c ../test-lib.o

run-test-errors: test-errors
	./test-errors

test-debug: test-debug.c ../../wcc
	../../wcc test-debug.c -o test-debug -g

check-test-debug-headers: test-debug
	objdump -h test-debug | grep .debug_info > /dev/null
	objdump -h test-debug | grep .debug_line > /dev/null
	objdump -h test-debug | grep .debug_abbrev > /dev/null
	objdump -h test-debug | grep .debug_str > /dev/null

	objdump -g test-debug | grep -E 'DW_AT_producer.*wcc' > /dev/null
	objdump -g test-debug | grep 'Line Number Statements:' > /dev/null

clean:
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f test-instrsel
	@rm -f test-errors
	@rm -f instrsel-tests.rulecov
	@rm -f test-debug
