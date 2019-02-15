all: wc4 wc42 wc42-frp wc43 benchmark

wc4: wc4.c
	gcc wc4.c -o wc4 -g -Wno-parentheses

wc42.s: wc4
	./wc4 -c -S -o wc42.s wc4.c

wc42-urfl.s: wc4
	./wc4 -c -S -fuse-registers-for-locals -o wc42-urfl.s wc4.c

wc42-frp.s: wc4
	./wc4 -c -S --frp -o wc42-frp.s wc4.c

wc42: wc42.s
	gcc wc42.s -o wc42

wc42-urfl: wc42-urfl.s
	gcc wc42-urfl.s -o wc42-urfl

wc42-frp: wc42-frp.s
	gcc wc42-frp.s -o wc42-frp

wc43.s: wc42
	./wc42 -c -S -o wc43.s wc4.c

wc43-urfl.s: wc42-urfl
	./wc42-urfl -c -S -fuse-registers-for-locals -o wc43-urfl.s wc4.c

wc43: wc43.s
	gcc wc43.s -o wc43

wc43-urfl: wc43.s
	gcc wc43-urfl.s -o wc43-urfl

test-wc4.s: wc4 test-wc4.c
	./wc4 -c -S test-wc4.c

test-wc4-frp.s: wc4 test-wc4.c
	./wc4 --frp -c -S -o test-wc4-frp.s test-wc4.c

test-wc4-frp-drc.s: wc4 test-wc4.c
	./wc4 --frp --drc -c -S -o test-wc4-frp-drc.s test-wc4.c

test-wc4-urfl.s: wc4 test-wc4.c
	./wc4 -fuse-registers-for-locals -c -S -o test-wc4-urfl.s test-wc4.c

test-wc4: test-wc4.s
	gcc test-wc4.s -o test-wc4

test-wc4-frp: test-wc4-frp.s
	gcc test-wc4-frp.s -o test-wc4-frp

test-wc4-frp-drc: test-wc4-frp-drc.s
	gcc test-wc4-frp-drc.s -o test-wc4-frp-drc

test-wc4-urfl: test-wc4-urfl.s
	gcc test-wc4-urfl.s -o test-wc4-urfl

benchmark: wc4 wc42 wc42-frp wc42-urfl benchmark.c
	gcc benchmark.c -o benchmark

run-benchmark: benchmark
	./benchmark

.PHONY: run-test-wc4
run-test-wc4: test-wc4
	./test-wc4
	@echo wc4 tests passed

.PHONY: run-test-wc4-frp
run-test-wc4-frp: test-wc4-frp
	./test-wc4-frp
	@echo wc4 FRP tests passed

.PHONY: run-test-wc4-frp-drc
run-test-wc4-frp-drc: test-wc4-frp-drc
	./test-wc4-frp-drc
	@echo wc4 FRP DRC tests passed

.PHONY: run-test-wc4-urfl
run-test-wc4-urfl: test-wc4-urfl
	./test-wc4-urfl
	@echo wc4 URFL tests passed

test-wc4-gcc: test-wc4.c
	gcc test-wc4.c -o test-wc4-gcc -Wno-int-conversion -Wno-incompatible-pointer-types

.PHONY: run-test-wc4-gcc
run-test-wc4-gcc: test-wc4-gcc
	./test-wc4-gcc
	@echo gcc tests passed

test-self-compilation: wc42.s wc43.s
	diff wc42.s wc43.s
	@echo self compilation test passed

test-urfl-self-compilation: wc42-urfl.s wc43-urfl.s
	diff wc42-urfl.s wc43-urfl.s
	@echo URFL self compilation test passed

.PHONY: test
test: run-test-wc4 run-test-wc4-frp test-wc4-frp-drc run-test-wc4-urfl run-test-wc4-gcc test-self-compilation test-urfl-self-compilation

clean:
	@rm -f wc4
	@rm -f wc42
	@rm -f wc42-frp
	@rm -f wc42-urfl
	@rm -f wc43
	@rm -f test-wc4
	@rm -f test-wc4-frp
	@rm -f test-wc4-gcc
	@rm -f test-wc4-frp-drc
	@rm -f test-wc4-urfl
	@rm -f benchmark
	@rm -f *.s
	@rm -f *.o
	@rm -f test.c test test1.c test1 test2.c test2
	@rm -f core
