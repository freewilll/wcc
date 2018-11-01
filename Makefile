all: wc4

wc4: wc4.c
	gcc wc4.c -o wc4 -Wno-incompatible-library-redeclaration

.PHONY: test
test: wc4
	venv/bin/py.test -vs
clean:
	rm -f wc4
