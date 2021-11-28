all: test-unit test-integration test-e2e test-cpp

utils.o: ../utils.c
	gcc ${GCC_OPTS} -c ../utils.c -o utils.o

.PHONY:
test-unit: test-utils.s test-lib.o
	cd unit && ${MAKE} all

.PHONY:
test-unit-parser: test-utils.s test-lib.o
	cd unit && ${MAKE} run-test-parser

.PHONY:
test-unit-cpp: test-utils.s test-lib.o
	cd unit && ${MAKE} run-test-cpp

.PHONY:
test-unit-types: test-utils.s test-lib.o
	cd unit && ${MAKE} run-test-types

.PHONY:
test-integration: test-utils.s test-lib.o
	cd integration && ${MAKE} all

.PHONY:
test-cpp:
	cd cpp && ${MAKE} all

.PHONY:
test-e2e: test-lib.o utils.o
	cd e2e && ${MAKE} all

test-lib.o: test-lib.c
	gcc ${GCC_OPTS} test-lib.c -c

test-utils.s: test-utils.c
	gcc ${GCC_OPTS} -c -S test-utils.c -o test-utils.s

clean:
	cd unit && ${MAKE} clean
	cd integration && ${MAKE} clean
	cd e2e && ${MAKE} clean

	@rm -f *.s
	@rm -f *.o
	@rm -f core
