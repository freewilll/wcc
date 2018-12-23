all: wc4 was4

wc4: wc4.c
	gcc wc4.c -o wc4 -Wno-parentheses

was4: was4.c
	gcc was4.c -o was4 -Wno-parentheses

.PHONY: test
test-unit: wc4 was4
	venv/bin/py.test -vs

test-inception-codegen: wc4
	LEVEL1=$(shell mktemp) ;\
	LEVEL2=$(shell mktemp) ;\
	./wc4 -ne -nc -o $$LEVEL1 wc4.c ;\
	./wc4 -ne -nc             wc4.c -ne -nc -o $$LEVEL2 wc4.c ;\
	diff $$LEVEL1 $$LEVEL2
	@echo Two-level deep generated code is identical

test: test-unit test-inception-codegen

clean:
	rm -f wc4
	rm -f *.ws
	rm -f wc4-level1.ws wc4-level2.ws
