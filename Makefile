all: wc4

wc4: wc4.c
	gcc wc4.c -o wc4 -Wno-incompatible-library-redeclaration -Wno-parentheses

.PHONY: test
test-unit: wc4
	venv/bin/py.test -vs

test-inception-codegen: wc4
	./wc4 -ne -nc -c wc4.c                                   > wc4-level1.code
	./wc4 -ne -nc    wc4.c -ne -nc -c wc4.c                  > wc4-level2.code
	@#./wc4 -ne -nc    wc4.c -ne -nc    wc4.c -ne -nc -c wc4.c > wc4-level3.code
	diff wc4-level1.code wc4-level2.code
	@#diff wc4-level1.code wc4-level3.code
	@echo Two-level deep generated code is identical

test: test-unit test-inception-codegen

clean:
	rm -f wc4
