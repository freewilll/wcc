all: wc4 wc42 wc42-O1 wc42-frp wc43 wc43-O1 benchmark

SOURCES = \
  wc4.c \
  lexer.c \
  parser.c \
  ir.c \
  ssa.c \
  codegen.c \
  utils.c \
  intset.c \

# OBJECTS := ${SOURCES:c=o}
ASSEMBLIES := ${SOURCES:c=s}

build:
	@mkdir -p build/wc42
	@mkdir -p build/wc42-O1
	@mkdir -p build/wc42-frp
	@mkdir -p build/wc43
	@mkdir -p build/wc43-O1

wc4: ${SOURCES} wc4.h build
	gcc ${SOURCES} -o wc4 -g -Wno-return-type

# wc42
WC42_SOURCES := ${SOURCES:%=build/wc42/%}
WC42_ASSEMBLIES := ${WC42_SOURCES:.c=.s}

build/wc42/%.s: %.c wc4
	./wc4 -c $< -S -o $@

wc42: ${WC42_ASSEMBLIES}
	gcc ${WC42_ASSEMBLIES} -o wc42

# wc42-O1
WC42_O1_SOURCES := ${SOURCES:%=build/wc42-O1/%}
WC42_O1_ASSEMBLIES := ${WC42_O1_SOURCES:.c=.s}

build/wc42-O1/%.s: %.c wc4
	./wc4 -c $< -S -o $@ -O1

wc42-O1: ${WC42_O1_ASSEMBLIES}
	gcc ${WC42_O1_ASSEMBLIES} -o wc42-O1

# wc42-frp
WC42_FRP_SOURCES := ${SOURCES:%=build/wc42-frp/%}
WC42_FRP_ASSEMBLIES := ${WC42_FRP_SOURCES:.c=.s}

build/wc42-frp/%.s: %.c wc4
	./wc4 -c $< -S -o $@ --frp

wc42-frp: ${WC42_FRP_ASSEMBLIES}
	gcc ${WC42_FRP_ASSEMBLIES} -o wc42-frp

# wc43
WC43_SOURCES := ${SOURCES:%=build/wc43/%}
WC43_ASSEMBLIES := ${WC43_SOURCES:.c=.s}

build/wc43/%.s: %.c wc42
	./wc42 -c $< -S -o $@

wc43: ${WC43_ASSEMBLIES}
	gcc ${WC43_ASSEMBLIES} -o wc43

# wc43-O1
WC43_O1_SOURCES := ${SOURCES:%=build/wc43-O1/%}
WC43_O1_ASSEMBLIES := ${WC43_O1_SOURCES:.c=.s}

build/wc43-O1/%.s: %.c wc42-O1
	./wc42-O1 -c $< -S -o $@ -O1

wc43-O1: ${WC43_O1_ASSEMBLIES}
	gcc ${WC43_O1_ASSEMBLIES} -o wc43-O1

# tests
stack-check.o: stack-check.c
	gcc stack-check.c -c

test-wc4.s: wc4 test-wc4.c
	./wc4 -c -S test-wc4.c

test-wc4-frp.s: wc4 test-wc4.c
	./wc4 --frp -c -S -o test-wc4-frp.s test-wc4.c

test-wc4-frp-ncr.s: wc4 test-wc4.c
	./wc4 --frp -fno-coalesce-registers -c -S -o test-wc4-frp-ncr.s test-wc4.c

test-wc4-O1.s: wc4 test-wc4.c
	./wc4 -O1 -c -S -o test-wc4-O1.s test-wc4.c

test-wc4: test-wc4.s stack-check.o
	gcc test-wc4.s stack-check.o -o test-wc4

test-wc4-frp: test-wc4-frp.s stack-check.o
	gcc test-wc4-frp.s stack-check.o -o test-wc4-frp

test-wc4-frp-ncr: test-wc4-frp-ncr.s stack-check.o
	gcc test-wc4-frp-ncr.s stack-check.o -o test-wc4-frp-ncr

test-wc4-O1: test-wc4-O1.s stack-check.o
	gcc test-wc4-O1.s stack-check.o -o test-wc4-O1

benchmark: wc4 wc42 wc42-frp wc42-O1 benchmark.c
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

.PHONY: run-test-wc4-frp-ncr
run-test-wc4-frp-ncr: test-wc4-frp-ncr
	./test-wc4-frp-ncr
	@echo wc4 FRP NRC tests passed

.PHONY: run-test-wc4-O1
run-test-wc4-O1: test-wc4-O1
	./test-wc4-O1
	@echo wc4 -O1 tests passed

test-wc4-gcc: test-wc4.c
	gcc test-wc4.c stack-check.o -o test-wc4-gcc -Wno-int-conversion -Wno-incompatible-pointer-types

.PHONY: run-test-wc4-gcc
run-test-wc4-gcc: test-wc4-gcc
	./test-wc4-gcc
	@echo gcc tests passed

test-self-compilation: ${WC42_ASSEMBLIES} ${WC43_ASSEMBLIES}
	cat build/wc42/*.s > build/wc42/all-s
	cat build/wc43/*.s > build/wc43/all-s
	diff build/wc42/all-s build/wc43/all-s
	@echo self compilation test passed

test-O1-self-compilation: ${WC42_O1_ASSEMBLIES} ${WC43_O1_ASSEMBLIES}
	cat build/wc42-O1/*.s > build/wc42-O1/all-s
	cat build/wc43-O1/*.s > build/wc43-O1/all-s
	diff build/wc42-O1/all-s build/wc43-O1/all-s
	@echo O1 self compilation test passed

test-include/test-include: wc4 test-include/include.h test-include/main.c test-include/foo.c
	cd test-include && ../wc4 main.c foo.c -o test-include

run-test-include: test-include/test-include
	test-include/test-include

test-intset: wc4 test-intset.c intset.c
	./wc4 intset.c test-intset.c -o test-intset

run-test-intset: test-intset
	 ./test-intset

test-ssa: wc4 test-ssa.c ssa.c intset.c
	./wc4 ssa.c test-ssa.c intset.c -o test-ssa

run-test-ssa: test-ssa
	 ./test-ssa

.PHONY: test
test: run-test-intset run-test-ssa run-test-wc4 run-test-wc4-frp test-wc4-frp-ncr run-test-wc4-O1 run-test-include run-test-wc4-gcc test-self-compilation test-O1-self-compilation

clean:
	@rm -f wc4
	@rm -f wc42
	@rm -f wc42-frp
	@rm -f wc42-O1
	@rm -f wc43
	@rm -f wc43-O1
	@rm -f test-wc4
	@rm -f test-wc4-frp
	@rm -f test-wc4-gcc
	@rm -f test-wc4-frp-ncr
	@rm -f test-wc4-O1
	@rm -f test-intset
	@rm -f test-ssa
	@rm -f benchmark
	@rm -f *.s
	@rm -f *.o
	@rm -f test.c test test1.c test1 test2.c test2
	@rm -f core
	@rm -f a.out
	@rm -f test-include/test-include
	@rm -Rf build
