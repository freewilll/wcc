all: wc4 was4

wc4: wc4.c
	gcc wc4.c -o wc4 -g -Wno-parentheses

was4: was4.c
	gcc was4.c -o was4 -g -Wno-parentheses

wc4.ws: wc4 wc4.c
	@# tmp is used as a workaround for vagrant nfs being terribly slow when using dprintf in was4
	./wc4 -o - wc4.c > /tmp/wc4.ws && cp /tmp/wc4.ws wc4.ws

wc42.o: was4 wc4.ws
	@# tmp is used as a workaround for vagrant nfs being terribly slow when using dprintf in was4
	./was4 wc4.ws

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
	rm -f was4
	rm -f wc4.ws
	rm -f wc4-level1.ws wc4-level2.ws
