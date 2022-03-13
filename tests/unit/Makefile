all: run-test-lexer run-test-cpp run-test-set run-test-list run-test-strmap run-test-strset run-test-longmap run-test-longset run-test-ssa run-test-parser run-test-types run-test-graph run-test-functions

test-lexer: ../../libwcc.a test-lexer.c
	gcc ${GCC_OPTS} -g -o test-lexer test-lexer.c ../../libwcc.a

run-test-lexer: test-lexer
	 ./test-lexer

test-cpp: ../../libwcc.a ../test-lib.o test-cpp.c
	gcc ${GCC_OPTS} -g -o test-cpp test-cpp.c ../../libwcc.a ../test-lib.o

run-test-cpp: test-cpp
	 ./test-cpp

test-set: ../../libwcc.a test-set.c
	gcc ${GCC_OPTS} -g -o test-set test-set.c ../../libwcc.a

run-test-set: test-set
	 ./test-set

test-list: ../../libwcc.a test-list.c
	gcc ${GCC_OPTS} -g -o test-list test-list.c ../../libwcc.a

run-test-list: test-list
	 ./test-list

test-strmap: test-strmap.c ../../strmap.c
	gcc ${GCC_OPTS} -g -o test-strmap test-strmap.c ../../libwcc.a

run-test-strmap: test-strmap
	 ./test-strmap

test-strset: test-strset.c ../../strset.c
	gcc ${GCC_OPTS} -g -o test-strset test-strset.c ../../libwcc.a

run-test-strset: test-strset
	 ./test-strset

test-longmap: test-longmap.c ../../longmap.c
	gcc ${GCC_OPTS} -g -o test-longmap test-longmap.c ../../libwcc.a

run-test-longmap: test-longmap
	 ./test-longmap

test-longset: test-longset.c ../../longset.c
	gcc ${GCC_OPTS} -g -o test-longset test-longset.c ../../libwcc.a

run-test-longset: test-longset
	 ./test-longset

test-ssa: ../../libwcc.a ../test-utils.c test-ssa.c
	gcc ${GCC_OPTS} -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-ssa test-ssa.c ../test-utils.c ../../libwcc.a

run-test-ssa: test-ssa
	 ./test-ssa

test-graph: test-graph.c ../../graph.c ../../utils.c ../../memory.c
	gcc ${GCC_OPTS} test-graph.c ../../graph.c ../../utils.c ../../memory.c -o test-graph

run-test-graph: test-graph
	 ./test-graph

test-parser: ../../libwcc.a test-parser.c
	gcc ${GCC_OPTS} -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-parser test-parser.c ../../libwcc.a ../test-lib.o

run-test-parser: test-parser
	 ./test-parser

test-types: ../../libwcc.a test-types.c
	gcc ${GCC_OPTS} -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-types test-types.c ../../libwcc.a ../test-lib.o

run-test-types: test-types
	 ./test-types

test-functions: ../../libwcc.a test-functions.c
	gcc ${GCC_OPTS} -Wno-int-conversion -Wno-pointer-to-int-cast -g -o test-functions test-functions.c ../../libwcc.a ../test-lib.o

run-test-functions: test-functions
	 ./test-functions

clean:
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f test-lexer
	@rm -f test-cpp
	@rm -f test-set
	@rm -f test-list
	@rm -f test-strmap
	@rm -f test-strset
	@rm -f test-longmap
	@rm -f test-longset
	@rm -f test-ssa
	@rm -f test-graph
	@rm -f test-parser
	@rm -f test-types
	@rm -f test-functions
