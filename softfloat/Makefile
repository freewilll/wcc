ifeq ($(TARGET),aarch64)
test: test-softfloat
else
test:
endif

CC ?= gcc
OBJECTS = \
	encoding.o \
	load.o \
	load_float.o \
	load_double.o \
	load_ld.o \
	store.o \
	store_float.o \
	store_double.o \
	store_ld.o \
	rounding.o \
	integers.o \
	arithmetic.o \
	arithmetic_add.o \
	arithmetic_subtract.o \
	arithmetic_multiply.o \
	arithmetic_divide.o \
	arithmetic_negate.o \
	comparisons.o \
	comparison_ld_eq.o \
	comparison_ld_ne.o \
	comparison_ld_lt.o \
	comparison_ld_gt.o \
	comparison_ld_le.o \
	comparison_ld_ge.o \
	conversion_float_to_double.o \
	conversion_float_to_ld.o \
	conversion_double_to_float.o \
	conversion_double_to_ld.o \
	conversion_ld_to_float.o \
	conversion_ld_to_double.o \
	conversion_ld_to_int64.o \
	conversion_ld_to_uint64.o \
	conversion_int64_to_ld.o \
	conversion_uint64_to_ld.o \


OBJECTS_ABS_PATH := ${OBJECTS:%=$(BUILD_DIR)/softfloat/%}

# set DEBUG=1 to activate all debug flags
ifeq ($(DEBUG),1)
	CC_OPTS=-DDEBUG_LOAD -DDEBUG_STORE -DDEBUG_ROUNDING -DDEBUG_ARITHMETIC
endif

$(BUILD_DIR)/softfloat/%.o: %.c softfloat.h testlib.h
	${CC} -c $< -o $@ $(CC_OPTS)

# Use gcc to build the tests
$(BUILD_DIR)/softfloat/testlib.o: testlib.c softfloat.h testlib.h
	gcc -c $< -o $@ $(CC_OPTS)

$(BUILD_DIR)/softfloat/test_softfloat.o: test_softfloat.c softfloat.h testlib.h
	gcc -c $< -o $@ $(CC_OPTS)

test_softfloat: $(BUILD_DIR)/softfloat/test_softfloat.o $(OBJECTS_ABS_PATH) $(BUILD_DIR)/softfloat/testlib.o
	gcc $^ -o $(BUILD_DIR)/softfloat/test_softfloat

.PHONY: lib
lib: $(OBJECTS_ABS_PATH)
	@mkdir -p $(BUILD_DIR)/softfloat/lib
	cp $(OBJECTS_ABS_PATH) $(BUILD_DIR)/softfloat/lib

.PHONY: debug
debug: debug.o $(OBJECTS_ABS_PATH)
	${CC} $^ -o debug
	$(BUILD_DIR)/softfloat/debug

PHONY: test-softfloat
test-softfloat: test_softfloat
	$(BUILD_DIR)/softfloat/test_softfloat

clean:
	@rm -f $(BUILD_DIR)/softfloat/debug
	@rm -f $(BUILD_DIR)/softfloat/test_softfloat
	@rm -f $(BUILD_DIR)/softfloat/*.o
	@rm -Rf $(BUILD_DIR)/softfloat/lib
