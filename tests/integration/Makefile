all: run-test-instrsel

test-instrsel: ../../libwcc.a ../test-utils.c test-instrsel.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -g -o test-instrsel test-instrsel.c ../test-utils.c ../../libwcc.a

run-test-instrsel: test-instrsel
	./test-instrsel

clean:
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f test-instrsel
	@rm -f instrsel-tests.rulecov
