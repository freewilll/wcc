WCC_TESTS=\
	expr \
	comma \
	function-call-args \
	func-calls \
	pointers \
	arrays \
	memory-functions \
	inc-dec \
	conditionals \
	structs \
	pointer-arithmetic \
	loops \
	enums \
	regressions \
	arithmetic-types-move-torture \
	arithmetic-torture \
	ifdef \
	long-doubles \
	floats-and-doubles \
	typedefs \
	switch \
	character-and-string-literals \
	varargs \
	initializers

all: run-test-gcc run-test-wcc run-test-abi run-test-include run-test-shlib

stack-check.o: stack-check.c
	gcc ${GCC_OPTS} stack-check.c -c

test-%.s: test-%.c ../../wcc ../../include/stdarg.h
	../../wcc ${WCC_OPTS} --rule-coverage-file wcc-tests.rulecov -Wno-integer-constant-too-large -Wno-warn-assignment-types-incompatible -c -S $<

test-%-wcc: test-%.s stack-check.o ../test-lib.o ../utils.o
	gcc ${GCC_OPTS} $< stack-check.o ../test-lib.o ../utils.o -o $@

run-test-%-wcc: test-%-wcc
	./$<
	touch run-$<

gen-test-arithmetic-torture: gen-test-arithmetic-torture.c
	gcc ${GCC_OPTS} -Wunused gen-test-arithmetic-torture.c -o gen-test-arithmetic-torture

test-arithmetic-torture.c: gen-test-arithmetic-torture
	./gen-test-arithmetic-torture

gen-test-arithmetic-types-move-torture: gen-test-arithmetic-types-move-torture.c
	gcc ${GCC_OPTS} -Wunused gen-test-arithmetic-types-move-torture.c -o gen-test-arithmetic-types-move-torture

test-arithmetic-types-move-torture.c: gen-test-arithmetic-types-move-torture
	./gen-test-arithmetic-types-move-torture

.PHONY: run-test-wcc
run-test-wcc: ${WCC_TESTS:%=run-test-%-wcc}
	@echo wcc tests passed

test-%-gcc: test-%.c stack-check.o ../test-lib.o ../utils.o
	gcc ${GCC_OPTS} $< stack-check.o ../test-lib.o ../utils.o -o $@ -w

run-test-%-gcc: test-%-gcc
	./$<
	touch run-$<

.PHONY: run-test-gcc
run-test-gcc: ${WCC_TESTS:%=run-test-%-gcc}
	@echo gcc tests passed

# ABI tests
test-abi-function-caller-gcc.o: test-abi-function-caller.c test-abi.h ../../wcc
	gcc ${GCC_OPTS} -c -o test-abi-function-caller-gcc.o test-abi-function-caller.c

test-abi-function-callee-gcc.o: test-abi-function-callee.c test-abi.h ../../wcc
	gcc ${GCC_OPTS} -c -o test-abi-function-callee-gcc.o test-abi-function-callee.c

test-abi-function-caller-wcc.o: test-abi-function-caller.c test-abi.h ../../wcc
	../../wcc ${WCC_OPTS} --rule-coverage-file wcc-tests.rulecov -c -o test-abi-function-caller-wcc.o test-abi-function-caller.c

test-abi-function-callee-wcc.o: test-abi-function-callee.c test-abi.h ../../wcc
	../../wcc ${WCC_OPTS} --rule-coverage-file wcc-tests.rulecov -c -o test-abi-function-callee-wcc.o test-abi-function-callee.c

test-abi-gcc-gcc: stack-check.o ../test-lib.o test-abi-function-caller-gcc.o test-abi-function-callee-gcc.o
	gcc ${GCC_OPTS} stack-check.o ../test-lib.o -o test-abi-gcc-gcc test-abi-function-caller-gcc.o test-abi-function-callee-gcc.o

test-abi-wcc-gcc: stack-check.o ../test-lib.o test-abi-function-caller-wcc.o test-abi-function-callee-gcc.o
	gcc ${GCC_OPTS} stack-check.o ../test-lib.o -o test-abi-wcc-gcc test-abi-function-caller-wcc.o test-abi-function-callee-gcc.o

test-abi-gcc-wcc: stack-check.o ../test-lib.o test-abi-function-caller-gcc.o test-abi-function-callee-wcc.o
	gcc ${GCC_OPTS} stack-check.o ../test-lib.o -o test-abi-gcc-wcc test-abi-function-caller-gcc.o test-abi-function-callee-wcc.o

test-abi-wcc-wcc: stack-check.o ../test-lib.o test-abi-function-caller-wcc.o test-abi-function-callee-wcc.o
	gcc ${GCC_OPTS} stack-check.o ../test-lib.o -o test-abi-wcc-wcc test-abi-function-caller-wcc.o test-abi-function-callee-wcc.o

run-test-abi: test-abi-gcc-gcc test-abi-wcc-gcc test-abi-gcc-wcc test-abi-wcc-wcc
	./test-abi-gcc-gcc
	./test-abi-wcc-gcc
	./test-abi-gcc-wcc
	./test-abi-wcc-wcc
	touch run-test-abi

# Shared library tests
test-shlib-lib-gcc.o: test-shlib-lib.c test-shlib.h
	gcc ${GCC_OPTS} -c -fPIC -o test-shlib-lib-gcc.o test-shlib-lib.c

test-shlib-lib-wcc.o: test-shlib-lib.c test-shlib.h ../../wcc
	../../wcc ${WCC_OPTS} --rule-coverage-file wcc-tests.rulecov -c -fPIC -o test-shlib-lib-wcc.o test-shlib-lib.c

libtest-shlib-lib-gcc.so: test-shlib-lib-gcc.o
	gcc test-shlib-lib-gcc.o -shared -o libtest-shlib-lib-gcc.so

libtest-shlib-lib-wcc.so: test-shlib-lib-wcc.o
	gcc test-shlib-lib-wcc.o -shared -o libtest-shlib-lib-wcc.so

test-shlib-main-gcc.o: test-shlib-main.c test-shlib.h
	gcc ${GCC_OPTS} -c -fPIC -o test-shlib-main-gcc.o test-shlib-main.c

test-shlib-main-wcc.o: test-shlib-main.c test-shlib.h ../../wcc
	../../wcc ${WCC_OPTS} --rule-coverage-file wcc-tests.rulecov -c -fPIC -o test-shlib-main-wcc.o test-shlib-main.c

test-shlib-gcc-gcc: test-shlib-main-gcc.o libtest-shlib-lib-gcc.so
	gcc test-shlib-main-gcc.o -L. -l test-shlib-lib-gcc ../test-lib.o -o test-shlib-gcc-gcc

test-shlib-wcc-gcc: test-shlib-main-wcc.o libtest-shlib-lib-gcc.so
	gcc test-shlib-main-wcc.o -L. -l test-shlib-lib-gcc ../test-lib.o -o test-shlib-wcc-gcc

test-shlib-gcc-wcc: test-shlib-main-gcc.o libtest-shlib-lib-wcc.so
	gcc test-shlib-main-gcc.o -L. -l test-shlib-lib-wcc ../test-lib.o -o test-shlib-gcc-wcc

test-shlib-wcc-wcc: test-shlib-main-wcc.o libtest-shlib-lib-wcc.so
	gcc test-shlib-main-wcc.o -L. -l test-shlib-lib-wcc ../test-lib.o -o test-shlib-wcc-wcc

run-test-shlib: test-shlib-gcc-gcc test-shlib-wcc-gcc test-shlib-gcc-wcc test-shlib-wcc-wcc
	LD_LIBRARY_PATH=. ./test-shlib-gcc-gcc
	LD_LIBRARY_PATH=. ./test-shlib-wcc-gcc
	LD_LIBRARY_PATH=. ./test-shlib-gcc-wcc
	LD_LIBRARY_PATH=. ./test-shlib-wcc-wcc
	touch run-test-shlib

# Includes
test-include/test-include: ../../wcc test-include/include.h test-include/main.c test-include/foo.c
	cd test-include && ../../../wcc ${WCC_OPTS} main.c foo.c -o test-include

run-test-include: test-include/test-include
	test-include/test-include

clean:
	@rm -f test-wcc
	@rm -f test-*-gcc
	@rm -f test-*-wcc
	@rm -f test-wcc-arithmetic-torture.c
	@rm -f gen-test-arithmetic-torture
	@rm -f test-wcc-arithmetic-types-move-torture.c
	@rm -f gen-test-arithmetic-types-move-torture
	@rm -f run-test-*
	@rm -f run-test-abi
	@rm -f test-wcc-gcc
	@rm -f test-wcc-*-gcc
	@rm -f *.s
	@rm -f *.o
	@rm -f *.so
	@rm -f core
	@rm -f test-include/test-include
	@rm -f wcc-tests.rulecov
	@rm -f test-abi-gcc-gcc
	@rm -f test-abi-wcc-gcc
	@rm -f test-abi-gcc-wcc
	@rm -f test-abi-wcc-wcc
	@rm -f run-test-shlib
	@rm -f libtest-shlib-gcc.so
	@rm -f libtest-shlib-wcc.so
	@rm -f test-shlib-gcc-gcc
	@rm -f test-shlib-wcc-gcc
	@rm -f test-shlib-gcc-wcc
	@rm -f test-shlib-wcc-wcc
