all: test_softfloat

SOURCES = testlib.o loadstore.o conversions.o rounding.o integers.o arithmetic.o comparisons.o
OBJECTS := ${SOURCES:c=o}
OBJECTS_ABS_PATH := ${OBJECTS:%=$(BUILD_DIR)/softfloat/%}

# set DEBUG=1 to activate all debug flags
ifeq ($(DEBUG),1)
	CC_OPTS=-DDEBUG_LOAD -DDEBUG_STORE -DDEBUG_ROUNDING -DDEBUG_ARITHMETIC
endif

$(BUILD_DIR)/softfloat/%.o: %.c softfloat.h testlib.h
	gcc -c $< -o $@ $(CC_OPTS)

test_softfloat: $(BUILD_DIR)/softfloat/test_softfloat.o $(OBJECTS_ABS_PATH)
	gcc $^ -o $(BUILD_DIR)/softfloat/test_softfloat

.PHONY: debug
debug: debug.o $(OBJECTS_ABS_PATH)
	gcc $^ -o debug
	$(BUILD_DIR)/softfloat/debug

test: test_softfloat
	$(BUILD_DIR)/softfloat/test_softfloat

clean:
	@rm -f $(BUILD_DIR)/softfloat/debug
	@rm -f $(BUILD_DIR)/softfloat/test_softfloat
	@rm -f $(BUILD_DIR)/softfloat/*.o
