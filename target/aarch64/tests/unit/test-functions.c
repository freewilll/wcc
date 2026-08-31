#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wcc.h"
#include "test-lib.h"
#include "test-functions-lib.h"

int verbose;
int passes;
int failures;

void test_scalar_params() {
    test_param_allocation(PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "0        |          | 000 000 | ");
    test_param_allocation(PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "01       |          | 000 000 | ");
    test_param_allocation(PI, PI, PI, PI, PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "012345   |          | 000 000 | ");
    test_param_allocation(PI, PI, PI, PI, PI, PI, PI, PI, 0,  0,  0,  0,  0,  0,  0,  0,  "01234567 |          | 000 000 | ");
    test_param_allocation(PF, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 0        | 000 000 | ");
    test_param_allocation(PF, PF, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 , "         | 01       | 000 000 | ");
    test_param_allocation(PF, PF, PF, PF, PF, PF, PF, PF, 0,  0,  0,  0,  0,  0,  0,  0,  "         | 01234567 | 000 000 | ");
    test_param_allocation(PI, PF, PL, PI, PF, PL, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  "03       | 1245     | 000 000 | ");
    // TODO aarch64
}

int main(int argc, char **argv) {
    passes = 0;
    failures = 0;

    parse_args(argc, argv);

    init_memory_management_for_translation_unit();

    test_scalar_params();
    // test_struct_params(); // TODO aarch64
    // test_int128(); // TODO aarch64

    finalize();
}
