all: run-test-lexer run-test-set run-test-map run-test-ssa run-test-parser run-test-types run-test-graph run-test-functions

test-lexer: ../../libwcc.a test-lexer.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -g -o test-lexer test-lexer.c ../../libwcc.a

run-test-lexer: test-lexer
	 ./test-lexer

test-set: ../../libwcc.a test-set.c
	gcc ${GCC_OPTS} -g -o test-set test-set.c ../../libwcc.a

run-test-set: test-set
	 ./test-set

test-map: test-map.c ../../map.c
	gcc ${GCC_OPTS} -g -o test-map test-map.c ../../libwcc.a

run-test-map: test-map
	 ./test-map

test-ssa: ../../libwcc.a ../test-utils.c test-ssa.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-ssa test-ssa.c ../test-utils.c ../../libwcc.a

run-test-ssa: test-ssa
	 ./test-ssa

test-graph: test-graph.c ../../graph.c ../../utils.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE test-graph.c ../../graph.c ../../utils.c -o test-graph

run-test-graph: test-graph
	 ./test-graph

test-parser: ../../libwcc.a test-parser.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-parser test-parser.c ../../libwcc.a ../test-lib.o

run-test-parser: test-parser
	 ./test-parser

test-types: ../../libwcc.a test-types.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-types test-types.c ../../libwcc.a ../test-lib.o

run-test-types: test-types
	 ./test-types

test-functions: ../../libwcc.a test-functions.c
	gcc ${GCC_OPTS} -D _GNU_SOURCE -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-functions test-functions.c ../../libwcc.a ../test-lib.o

run-test-functions: test-functions
	 ./test-functions

clean:
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f test-lexer
	@rm -f test-set
	@rm -f test-map
	@rm -f test-ssa
	@rm -f test-graph
	@rm -f test-parser
	@rm -f test-types
	@rm -f test-functions
