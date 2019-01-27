all: wc4

wc4: wc4.c
	gcc wc4.c -o wc4 -g -Wno-parentheses

wc42.s: wc4
	@# Workaround poor performance on NFS due to missing buffering in dprintf
	./wc4 -o /tmp/wc42.s wc4.c
	cp /tmp/wc42.s wc42.s

wc42: wc42.s
	gcc wc42.s -o wc42

wc43.s: wc42
	@# Workaround poor performance on NFS due to missing buffering in dprintf
	./wc42 -o /tmp/wc43.s wc4.c
	cp /tmp/wc43.s wc43.s

wc43: wc43.s
	gcc wc43.s -o wc43

test-wc4.s: wc4 test-wc4.c
	@# Workaround poor performance on NFS due to missing buffering in dprintf
	./wc4 -o /tmp/test-wc4.s test-wc4.c
	cp /tmp/test-wc4.s test-wc4.s
	@echo self compilation test passed

test-wc4-frp.s: wc4 test-wc4.c
	@# Workaround poor performance on NFS due to missing buffering in dprintf
	./wc4 -frp -o /tmp/test-wc4-frp.s test-wc4.c
	cp /tmp/test-wc4-frp.s test-wc4-frp.s
	@echo self compilation test passed

test-wc4: test-wc4.s
	gcc test-wc4.s -o test-wc4

test-wc4-frp: test-wc4-frp.s
	gcc test-wc4-frp.s -o test-wc4-frp

.PHONY: run-test-wc4
run-test-wc4: test-wc4
	./test-wc4
	@echo wc4 tests passed

.PHONY: run-test-wc4-gcc
run-test-wc4-frp: test-wc4-frp
	./test-wc4-frp
	@echo wc4 FRP tests passed

test-wc4-gcc: test-wc4.c
	gcc test-wc4.c -o test-wc4-gcc -Wno-int-conversion -Wno-incompatible-pointer-types

.PHONY: run-test-wc4-gcc
run-test-wc4-gcc: test-wc4-gcc
	./test-wc4-gcc
	@echo gcc tests passed

test-self-compilation: wc42.s wc43.s
	diff wc42.s wc43.s

.PHONY: test
test: run-test-wc4 run-test-wc4-frp run-test-wc4-gcc test-self-compilation

clean:
	@rm -f wc4
	@rm -f wc42
	@rm -f wc43
	@rm -f test-wc4
	@rm -f test-wc4-frp
	@rm -f test-wc4-gcc
	@rm -f *.s
	@rm -f *.o
	@rm -f test.c test test1.c test1 test2.c test2
	@rm -f core
