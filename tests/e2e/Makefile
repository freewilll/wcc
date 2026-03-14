GCC ?= gcc

# Remove implicit rule, otherwise rules here making an intermedate .s between a .c
# and .o  will not run since the chain is longer than the implict rule that creates
# an .o from .c directly.
%.o: %.c

AS ?= as
TEST_BUILD_DIR = ${BUILD_DIR}/tests/e2e

WCC_TESTS=\
	expr \
	comma \
	function-call-args \
	func-calls \
	pointers \
	arrays \
	memory-functions \
	inc-dec \
	conditionals \
	structs \
	pointer-arithmetic \
	loops \
	enums \
	regressions \
	arithmetic-types-move-torture \
	arithmetic-torture \
	ifdef \
	long-doubles \
	floats-and-doubles \
	typedefs \
	switch \
	character-and-string-literals \
	varargs \
	initializers

WCC_RUN_FLAGS = -I ${SRC_DIR}/include
WCC_E2E_FLAGS := ${WCC_OPTS} ${WCC_RUN_FLAGS} --fail-on-leaked-memory --rule-coverage-file ${BUILD_DIR}/tests/e2e/wcc-tests.rulecov
WCC_E2E_WARN_FLAGS := -Wno-integer-constant-too-large -Wno-warn-assignment-types-incompatible
E2E_LINK_OBJECTS := ${BUILD_DIR}/tests/e2e/stack-check.o ${BUILD_DIR}/tests/test-lib.o ${BUILD_DIR}/utils.o ${BUILD_DIR}/memory.o
ABI_LINK_OBJECTS := ${BUILD_DIR}/tests/e2e/stack-check.o ${BUILD_DIR}/tests/test-lib.o

all: run-test-gcc run-test-wcc run-test-abi run-test-include run-test-shlib

${BUILD_DIR}/tests/e2e/stack-check.o: stack-check.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -c $< -o $@

${BUILD_DIR}/tests/e2e/test-%.s: test-%.c ${BUILD_DIR}/wcc ${SRC_DIR}/include/stdarg.h
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_E2E_FLAGS} ${WCC_E2E_WARN_FLAGS} -c -S $< -o $@

${BUILD_DIR}/tests/e2e/test-%.o: ${BUILD_DIR}/tests/e2e/test-%.s
	${AS} $< -o $@

${BUILD_DIR}/tests/e2e/test-%-wcc: ${BUILD_DIR}/tests/e2e/test-%.o ${E2E_LINK_OBJECTS}
	${GCC} ${GCC_OPTS} $^ -o $@

run-test-%-wcc: ${BUILD_DIR}/tests/e2e/test-%-wcc
	cd ${TEST_BUILD_DIR} && ./$(notdir $<)

${BUILD_DIR}/tests/e2e/gen-test-arithmetic-torture: ${SRC_DIR}/tests/e2e/gen-test-arithmetic-torture.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -Wunused $< -o $@

${BUILD_DIR}/tests/e2e/test-arithmetic-torture.c: ${BUILD_DIR}/tests/e2e/gen-test-arithmetic-torture
	cd ${TEST_BUILD_DIR} && ./gen-test-arithmetic-torture

${BUILD_DIR}/tests/e2e/gen-test-arithmetic-types-move-torture: ${SRC_DIR}/tests/e2e/gen-test-arithmetic-types-move-torture.c
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -Wunused $< -o $@

${BUILD_DIR}/tests/e2e/test-arithmetic-types-move-torture.c: ${BUILD_DIR}/tests/e2e/gen-test-arithmetic-types-move-torture
	cd ${TEST_BUILD_DIR} && ./gen-test-arithmetic-types-move-torture

${BUILD_DIR}/tests/e2e/test-arithmetic-torture.s: ${BUILD_DIR}/tests/e2e/test-arithmetic-torture.c ${BUILD_DIR}/wcc ${SRC_DIR}/include/stdarg.h
	${BUILD_DIR}/wcc ${WCC_E2E_FLAGS} ${WCC_E2E_WARN_FLAGS} -I ${SRC_DIR}/tests -c -S $< -o $@

${BUILD_DIR}/tests/e2e/test-arithmetic-types-move-torture.s: ${BUILD_DIR}/tests/e2e/test-arithmetic-types-move-torture.c ${BUILD_DIR}/wcc ${SRC_DIR}/include/stdarg.h
	${BUILD_DIR}/wcc ${WCC_E2E_FLAGS} ${WCC_E2E_WARN_FLAGS} -I ${SRC_DIR}/tests -c -S $< -o $@

.PHONY: run-test-wcc
run-test-wcc: ${WCC_TESTS:%=run-test-%-wcc}
	@echo wcc tests passed

${BUILD_DIR}/tests/e2e/test-%-gcc: test-%.c ${E2E_LINK_OBJECTS}
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -I ${SRC_DIR}/tests $^ -o $@ -w

${BUILD_DIR}/tests/e2e/test-arithmetic-torture-gcc: ${BUILD_DIR}/tests/e2e/test-arithmetic-torture.c ${E2E_LINK_OBJECTS}
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -I ${SRC_DIR}/tests $^ -o $@ -w

${BUILD_DIR}/tests/e2e/test-arithmetic-types-move-torture-gcc: ${BUILD_DIR}/tests/e2e/test-arithmetic-types-move-torture.c ${E2E_LINK_OBJECTS}
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -I ${SRC_DIR}/tests $^ -o $@ -w

run-test-%-gcc: ${BUILD_DIR}/tests/e2e/test-%-gcc
	cd ${TEST_BUILD_DIR} && ./$(notdir $<)

.PHONY: run-test-gcc
run-test-gcc: ${WCC_TESTS:%=run-test-%-gcc}
	@echo gcc tests passed

# ABI tests
${BUILD_DIR}/tests/e2e/test-abi-function-caller-gcc.o: test-abi-function-caller.c test-abi.h ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -c $< -o $@

${BUILD_DIR}/tests/e2e/test-abi-function-callee-gcc.o: test-abi-function-callee.c test-abi.h ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -c $< -o $@

${BUILD_DIR}/tests/e2e/test-abi-function-caller-wcc.o: test-abi-function-caller.c test-abi.h ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_E2E_FLAGS} -c $< -o $@

${BUILD_DIR}/tests/e2e/test-abi-function-callee-wcc.o: test-abi-function-callee.c test-abi.h ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_E2E_FLAGS} -c $< -o $@

${BUILD_DIR}/tests/e2e/test-abi-gcc-gcc: ${ABI_LINK_OBJECTS} ${BUILD_DIR}/tests/e2e/test-abi-function-caller-gcc.o ${BUILD_DIR}/tests/e2e/test-abi-function-callee-gcc.o
	${GCC} ${GCC_OPTS} $^ -o $@

${BUILD_DIR}/tests/e2e/test-abi-wcc-gcc: ${ABI_LINK_OBJECTS} ${BUILD_DIR}/tests/e2e/test-abi-function-caller-wcc.o ${BUILD_DIR}/tests/e2e/test-abi-function-callee-gcc.o
	${GCC} ${GCC_OPTS} $^ -o $@

${BUILD_DIR}/tests/e2e/test-abi-gcc-wcc: ${ABI_LINK_OBJECTS} ${BUILD_DIR}/tests/e2e/test-abi-function-caller-gcc.o ${BUILD_DIR}/tests/e2e/test-abi-function-callee-wcc.o
	${GCC} ${GCC_OPTS} $^ -o $@

${BUILD_DIR}/tests/e2e/test-abi-wcc-wcc: ${ABI_LINK_OBJECTS} ${BUILD_DIR}/tests/e2e/test-abi-function-caller-wcc.o ${BUILD_DIR}/tests/e2e/test-abi-function-callee-wcc.o
	${GCC} ${GCC_OPTS} $^ -o $@

run-test-abi: ${BUILD_DIR}/tests/e2e/test-abi-gcc-gcc ${BUILD_DIR}/tests/e2e/test-abi-wcc-gcc ${BUILD_DIR}/tests/e2e/test-abi-gcc-wcc ${BUILD_DIR}/tests/e2e/test-abi-wcc-wcc
	cd ${TEST_BUILD_DIR} && ./test-abi-gcc-gcc
	cd ${TEST_BUILD_DIR} && ./test-abi-wcc-gcc
	cd ${TEST_BUILD_DIR} && ./test-abi-gcc-wcc
	cd ${TEST_BUILD_DIR} && ./test-abi-wcc-wcc

# Shared library tests
${BUILD_DIR}/tests/e2e/test-shlib-lib-gcc.o: test-shlib-lib.c test-shlib.h
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -c -fPIC $< -o $@

${BUILD_DIR}/tests/e2e/test-shlib-lib-wcc.o: test-shlib-lib.c test-shlib.h ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_E2E_FLAGS} -c -fPIC $< -o $@

${BUILD_DIR}/tests/e2e/libtest-shlib-lib-gcc.so: ${BUILD_DIR}/tests/e2e/test-shlib-lib-gcc.o
	${GCC} -shared $< -o $@

${BUILD_DIR}/tests/e2e/libtest-shlib-lib-wcc.so: ${BUILD_DIR}/tests/e2e/test-shlib-lib-wcc.o
	${GCC} -shared $< -o $@

${BUILD_DIR}/tests/e2e/test-shlib-main-gcc.o: test-shlib-main.c test-shlib.h
	@mkdir -p $(@D)
	${GCC} ${GCC_OPTS} -c -fPIC $< -o $@

${BUILD_DIR}/tests/e2e/test-shlib-main-wcc.o: test-shlib-main.c test-shlib.h ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_E2E_FLAGS} -c -fPIC $< -o $@

${BUILD_DIR}/tests/e2e/test-shlib-main-no-pic-wcc.o: test-shlib-main.c test-shlib.h ${BUILD_DIR}/wcc
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_E2E_FLAGS} -c $< -o $@

${BUILD_DIR}/tests/e2e/test-shlib-gcc-gcc: ${BUILD_DIR}/tests/e2e/test-shlib-main-gcc.o ${BUILD_DIR}/tests/e2e/libtest-shlib-lib-gcc.so
	${GCC} ${BUILD_DIR}/tests/e2e/test-shlib-main-gcc.o -L${BUILD_DIR}/tests/e2e -ltest-shlib-lib-gcc ${BUILD_DIR}/tests/test-lib.o -o $@

${BUILD_DIR}/tests/e2e/test-shlib-wcc-gcc: ${BUILD_DIR}/tests/e2e/test-shlib-main-wcc.o ${BUILD_DIR}/tests/e2e/libtest-shlib-lib-gcc.so
	${GCC} ${BUILD_DIR}/tests/e2e/test-shlib-main-wcc.o -L${BUILD_DIR}/tests/e2e -ltest-shlib-lib-gcc ${BUILD_DIR}/tests/test-lib.o -o $@

${BUILD_DIR}/tests/e2e/test-shlib-gcc-wcc: ${BUILD_DIR}/tests/e2e/test-shlib-main-gcc.o ${BUILD_DIR}/tests/e2e/libtest-shlib-lib-wcc.so
	${GCC} ${BUILD_DIR}/tests/e2e/test-shlib-main-gcc.o -L${BUILD_DIR}/tests/e2e -ltest-shlib-lib-wcc ${BUILD_DIR}/tests/test-lib.o -o $@

${BUILD_DIR}/tests/e2e/test-shlib-wcc-wcc: ${BUILD_DIR}/tests/e2e/test-shlib-main-wcc.o ${BUILD_DIR}/tests/e2e/libtest-shlib-lib-wcc.so
	${GCC} ${BUILD_DIR}/tests/e2e/test-shlib-main-wcc.o -L${BUILD_DIR}/tests/e2e -ltest-shlib-lib-wcc ${BUILD_DIR}/tests/test-lib.o -o $@

${BUILD_DIR}/tests/e2e/test-shlib-mixed-pic-wcc-wcc: ${BUILD_DIR}/tests/e2e/test-shlib-main-no-pic-wcc.o ${BUILD_DIR}/tests/e2e/libtest-shlib-lib-wcc.so
	${GCC} ${BUILD_DIR}/tests/e2e/test-shlib-main-no-pic-wcc.o ${BUILD_DIR}/tests/e2e/libtest-shlib-lib-wcc.so ${BUILD_DIR}/tests/test-lib.o -o $@

run-test-shlib: ${BUILD_DIR}/tests/e2e/test-shlib-gcc-gcc ${BUILD_DIR}/tests/e2e/test-shlib-wcc-gcc ${BUILD_DIR}/tests/e2e/test-shlib-gcc-wcc ${BUILD_DIR}/tests/e2e/test-shlib-wcc-wcc
	cd ${TEST_BUILD_DIR} && LD_LIBRARY_PATH=. ./test-shlib-gcc-gcc
	cd ${TEST_BUILD_DIR} && LD_LIBRARY_PATH=. ./test-shlib-wcc-gcc
	cd ${TEST_BUILD_DIR} && LD_LIBRARY_PATH=. ./test-shlib-gcc-wcc
	cd ${TEST_BUILD_DIR} && LD_LIBRARY_PATH=. ./test-shlib-wcc-wcc

# Includes
${BUILD_DIR}/tests/e2e/test-include/test-include: ${BUILD_DIR}/wcc ${SRC_DIR}/tests/e2e/test-include/include.h ${SRC_DIR}/tests/e2e/test-include/main.c ${SRC_DIR}/tests/e2e/test-include/foo.c
	@mkdir -p $(@D)
	${BUILD_DIR}/wcc ${WCC_OPTS} ${WCC_RUN_FLAGS} -I ${SRC_DIR}/tests/e2e/test-include ${SRC_DIR}/tests/e2e/test-include/main.c ${SRC_DIR}/tests/e2e/test-include/foo.c -o $@

run-test-include: ${BUILD_DIR}/tests/e2e/test-include/test-include
	$<

clean:
	@rm -f ${BUILD_DIR}/tests/e2e/*.o
	@rm -f ${BUILD_DIR}/tests/e2e/*.s
	@rm -f ${BUILD_DIR}/tests/e2e/*.so
	@rm -f ${BUILD_DIR}/tests/e2e/core
	@rm -f ${BUILD_DIR}/tests/e2e/gen-test-arithmetic-torture
	@rm -f ${BUILD_DIR}/tests/e2e/gen-test-arithmetic-types-move-torture
	@rm -f ${BUILD_DIR}/tests/e2e/test-arithmetic-torture.c
	@rm -f ${BUILD_DIR}/tests/e2e/test-arithmetic-types-move-torture.c
	@rm -f ${BUILD_DIR}/tests/e2e/test-arithmetic-torture-gcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-arithmetic-types-move-torture-gcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-*-gcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-*-wcc
	@rm -f ${BUILD_DIR}/tests/e2e/run-test-*
	@rm -f ${BUILD_DIR}/tests/e2e/test-include/test-include
	@rm -f ${BUILD_DIR}/tests/e2e/wcc-tests.rulecov
	@rm -f ${BUILD_DIR}/tests/e2e/test-abi-gcc-gcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-abi-wcc-gcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-abi-gcc-wcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-abi-wcc-wcc
	@rm -f ${BUILD_DIR}/tests/e2e/libtest-shlib-lib-gcc.so
	@rm -f ${BUILD_DIR}/tests/e2e/libtest-shlib-lib-wcc.so
	@rm -f ${BUILD_DIR}/tests/e2e/test-shlib-gcc-gcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-shlib-wcc-gcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-shlib-gcc-wcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-shlib-wcc-wcc
	@rm -f ${BUILD_DIR}/tests/e2e/test-shlib-mixed-pic-wcc-wcc
