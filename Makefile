all: wc4 was4

wc4: wc4.c
	gcc wc4.c -o wc4 -Wno-parentheses

was4: was4.c
	gcc was4.c -o was4 -Wno-parentheses

was4-test.o: was4
	./was4 --hw

was4-test: was4-test.o
	ld was4-test.o -o was4-test -lc --dynamic-linker /lib64/ld-linux-x86-64.so.2

.PHONY: test
test-unit: wc4 was4
	venv/bin/py.test -vs

test-inception-codegen: wc4
	./wc4 -ne -nc -c wc4.c                                   > wc4-level1.ws
	./wc4 -ne -nc    wc4.c -ne -nc -c wc4.c                  > wc4-level2.ws
	@#./wc4 -ne -nc    wc4.c -ne -nc    wc4.c -ne -nc -c wc4.c > wc4-level3.ws
	diff wc4-level1.ws wc4-level2.ws
	@#diff wc4-level1.ws wc4-level3.ws
	@echo Two-level deep generated code is identical

test: test-unit test-inception-codegen

clean:
	rm -f wc4
	rm -f *.ws
	rm -f was-test.o
	rm -f was-test
