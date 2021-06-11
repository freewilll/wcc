benchmark: benchmark.c
	gcc ${GCC_OPTS} benchmark.c -o benchmark

run-benchmark: benchmark
	./benchmark

make-rule-coverage-report.s: ../wcc make-rule-coverage-report.c
	../wcc ${WCC_OPTS} -c -S make-rule-coverage-report.c -c -S -o make-rule-coverage-report.s

make-rule-coverage-report: make-rule-coverage-report.s
	gcc ${GCC_OPTS} -D _GNU_SOURCE -g -o make-rule-coverage-report make-rule-coverage-report.s ../lexer.c ../parser.c ../types.c ../ir.c ../instrrules.c ../instrutil.c ../codegen.c ../utils.c ../set.c

rule-coverage-report: make-rule-coverage-report
	./make-rule-coverage-report

clean:
	@rm -f benchmark
	@rm -f make-rule-coverage-report
	@rm -f rulecov.html
	@rm -f *.s
	@rm -f *.o
	@rm -f core
	@rm -f instrsel-tests.rulecov
