all: test-unit test-integration test-e2e

.PHONY:
test-unit: test-utils.s
	cd unit && ${MAKE} all

.PHONY:
test-integration: test-utils.s
	cd integration && ${MAKE} all

.PHONY:
test-e2e: test-lib.o
	cd e2e && ${MAKE} all

test-lib.o: test-lib.c
	gcc ${GCC_OPTS} test-lib.c -c

test-utils.s: test-utils.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -c -S test-utils.c -o test-utils.s

clean:
	cd unit && ${MAKE} clean
	cd integration && ${MAKE} clean
	cd e2e && ${MAKE} clean

	@rm -f *.s
	@rm -f *.o
	@rm -f core