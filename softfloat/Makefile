all: test_softfloat

LIBS = testlib.o loadstore.o conversions.o rounding.o integers.o arithmetic.o

# set DEBUG=1 to activate all debug flags
ifeq ($(DEBUG),1)
	CC_OPTS=-DDEBUG_LOAD -DDEBUG_STORE -DDEBUG_ROUNDING -DDEBUG_ARITHMETIC
endif

%.o: %.c softfloat.h testlib.h
	gcc -c $< -o $@ $(CC_OPTS)

test_softfloat: test_softfloat.o $(LIBS)
	gcc $^ -o test_softfloat

.PHONY: debug
debug: debug.o $(LIBS)
	gcc $^ -o debug
	./debug

test: test_softfloat
	./test_softfloat

clean:
	@rm -f debug
	@rm -f test_softfloat
	@rm -f *.o
