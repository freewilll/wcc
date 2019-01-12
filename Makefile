all: wc4

wc4: wc4.c
	gcc wc4.c -o wc4 -g -Wno-parentheses

.PHONY: test
test-unit: wc4
	venv/bin/py.test -vs test_wc4.py

test: test-unit

clean:
	rm -f wc4
	rm -f wc42
	rm -f was4
	rm -f *.s
	rm -f *.o
	rm -f test.c test test1.c test1
