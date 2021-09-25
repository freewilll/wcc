WCC_TESTS=\
	expr \
	comma \
	function-call-args \
	func-calls \
	pointers \
	memory-functions \
	inc-dec \
	conditionals \
	structs \
	typedef \
	pointer-arithmetic \
	loops \
	enums \
	regressions \
	arithmetic-types-move-torture \
	arithmetic-torture \
	ifdef \
	long-doubles \
	floats-and-doubles

all: run-test-gcc run-test-wcc run-test-include

stack-check.o: stack-check.c struct-params.h
	gcc ${GCC_OPTS} stack-check.c -c

struct-params.o: struct-params.c
	gcc ${GCC_OPTS} struct-params.c -c

test-%.s: test-%.c ../../wcc
	../../wcc ${WCC_OPTS} --rule-coverage-file wcc-tests.rulecov --Wno-integer-constant-too-large -c -S $<

test-%-wcc: test-%.s stack-check.o struct-params.o ../test-lib.o
	gcc ${GCC_OPTS} $< stack-check.o struct-params.o ../test-lib.o -o $@

run-test-%-wcc: test-%-wcc
	./$<
	touch run-$<

gen-test-arithmetic-torture: gen-test-arithmetic-torture.c
	gcc ${GCC_OPTS} -Wunused -D _GNU_SOURCE gen-test-arithmetic-torture.c -o gen-test-arithmetic-torture

test-arithmetic-torture.c: gen-test-arithmetic-torture
	./gen-test-arithmetic-torture


gen-test-arithmetic-types-move-torture: gen-test-arithmetic-types-move-torture.c
	gcc ${GCC_OPTS} -Wunused -D _GNU_SOURCE gen-test-arithmetic-types-move-torture.c -o gen-test-arithmetic-types-move-torture

test-arithmetic-types-move-torture.c: gen-test-arithmetic-types-move-torture
	./gen-test-arithmetic-types-move-torture

.PHONY: run-test-wcc
run-test-wcc: ${WCC_TESTS:%=run-test-%-wcc}
	@echo wcc tests passed

test-%-gcc: test-%.c stack-check.o struct-params.o ../test-lib.o
	gcc ${GCC_OPTS} $< stack-check.o struct-params.o ../test-lib.o -o $@ -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-overflow -Wno-format -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-implicit-int -D _GNU_SOURCE

run-test-%-gcc: test-%-gcc
	./$<
	touch run-$<

test-include/test-include: ../../wcc test-include/include.h test-include/main.c test-include/foo.c
	cd test-include && ../../../wcc ${WCC_OPTS} main.c foo.c -o test-include

run-test-include: test-include/test-include
	test-include/test-include

.PHONY: run-test-gcc
run-test-gcc: ${WCC_TESTS:%=run-test-%-gcc}
	@echo gcc tests passed

clean:
	@rm -f test-wcc
	@rm -f test-wcc-*-wcc
	@rm -f test-wcc-arithmetic-torture.c
	@rm -f gen-test-arithmetic-torture
	@rm -f test-wcc-arithmetic-types-move-torture.c
	@rm -f gen-test-arithmetic-types-move-torture
	@rm -f run-test-*
	@rm -f test-wcc-gcc
	@rm -f test-wcc-*-gcc
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f test-include/test-include
	@rm -f wcc-tests.rulecov
