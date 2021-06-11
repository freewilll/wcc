WCC_TESTS=\
	expr \
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
	reg-move-torture

all: run-test-gcc run-test-wcc run-test-include

stack-check.o: stack-check.c
	gcc ${GCC_OPTS} stack-check.c -c

test-wcc.s: ../../wcc test-main.c
	../../wcc ${WCC_OPTS} -c -S test-main.c

test-%.s: test-%.c ../../wcc
	../../wcc ${WCC_OPTS} --rule-coverage-file wcc-tests.rulecov --Wno-integer-constant-too-large -c -S $<

test-%-wcc: test-%.s stack-check.o ../test-lib.o
	gcc ${GCC_OPTS} $< stack-check.o ../test-lib.o -o $@

run-test-%-wcc: test-%-wcc
	./$<
	touch run-$<

gen-test-reg-move-torture: gen-test-reg-move-torture.c
	gcc ${GCC_OPTS} -Wunused -D _GNU_SOURCE gen-test-wcc-reg-move-torture.c -o gen-test-wcc-reg-move-torture

test-reg-move-torture.c: gen-test-reg-move-torture
	./gen-test-reg-move-torture

.PHONY: run-test-wcc
run-test-wcc: ${WCC_TESTS:%=run-test-%-wcc}
	@echo wcc tests passed

test-%-gcc: test-%.c stack-check.o ../test-lib.o
	gcc ${GCC_OPTS} $< stack-check.o ../test-lib.o -o $@ -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-overflow -Wno-format -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -D _GNU_SOURCE

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
	@rm -f test-wcc-reg-move-torture.c
	@rm -f gen-test-wcc-reg-move-torture
	@rm -f run-test-wcc-*
	@rm -f test-wcc-gcc
	@rm -f test-wcc-*-gcc
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f test-include/test-include
	@rm -f wcc-tests.rulecov
