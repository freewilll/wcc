all: run-test-instrsel run-test-errors

test-instrsel: ../../libwcc.a ../test-utils.c test-instrsel.c
	gcc ${GCC_OPTS} -g -o test-instrsel test-instrsel.c ../test-utils.c ../../libwcc.a

run-test-instrsel: test-instrsel
	./test-instrsel

test-errors: ../../wcc ../test-lib.o test-errors.c
	gcc ${GCC_OPTS} -g -o test-errors test-errors.c ../test-lib.o

run-test-errors: test-errors
	./test-errors

clean:
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f test-instrsel
	@rm -f test-errors
	@rm -f instrsel-tests.rulecov
