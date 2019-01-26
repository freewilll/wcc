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

.PHONY: test
test-unit: wc4
	venv/bin/py.test -vs test_wc4.py

test-unit-frp: wc4
	FRP=1 venv/bin/py.test -vs test_wc4.py

test-self-compilation: wc42.s wc43.s
	diff wc42.s wc43.s

test: test-unit test-unit-frp test-self-compilation

clean:
	@rm -f wc4
	@rm -f wc42
	@rm -f wc43
	@rm -f *.s
	@rm -f *.o
	@rm -f test.c test test1.c test1 test2.c test2
	@rm -f core
