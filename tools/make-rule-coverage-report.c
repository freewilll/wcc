#include <stdio.h>
#include <fcntl.h>

#include "../wcc.h"

static void read_coverage_file(char *path, Set* coverage) {
    void *f;
    char *input;
    int rule;

    f = fopen(path, "r");
    if (!f) {
        perror(path);
        exit(1);
    }

    input = malloc(10 * 1024 * 1024);
    input_size = fread(input, 1, 10 * 1024 * 1024, f);
    if (input_size < 0) {
        printf("Unable to read input file\n");
        exit(1);
    }
    input[input_size] = 0;

    rule = 0;
    while (*input) {
        if (*input == '\n') {
            add_to_set(coverage, rule);
            rule = 0;
        }
        else
            rule = rule * 10 + (*input - '0');

        input++;
    }

    fclose(f);
}

int main() {
    Set *wcc2_cov, *instrsel_tests_cov, *wcc_tests_cov;
    void *f;
    int i;
    int covered, covered_count;
    Rule *r;

    init_instruction_selection_rules();

    wcc2_cov = new_set(instr_rule_count);
    instrsel_tests_cov = new_set(instr_rule_count);
    wcc_tests_cov = new_set(instr_rule_count);

    read_coverage_file("../tests/e2e/wcc-tests.rulecov", wcc2_cov);
    read_coverage_file("../tests/integration/instrsel-tests.rulecov", instrsel_tests_cov);
    read_coverage_file("../wcc2.rulecov", wcc_tests_cov);

    f = fopen("rulecov.html", "w");

    fprintf(f, "<html>\n");
    fprintf(f, "<head>\n");

    fprintf(f, "<style>\n");
    fprintf(f, "body {\n");
    fprintf(f, "  background-color: linen;\n");
    fprintf(f, "  font-size: 8px;\n");
    fprintf(f, "}    \n");
    fprintf(f, "table {\n");
    fprintf(f, "  border: 1px solid black;\n");
    fprintf(f, "  border-collapse: collapse;\n");
    fprintf(f, "}    \n");
    fprintf(f, "td, th {\n");
    fprintf(f, "  border: 1px solid black;\n");
    fprintf(f, "}    \n");
    fprintf(f, ".inset {\n");
    fprintf(f, "  background-color: #8f8;\n");
    fprintf(f, "}    \n");
    fprintf(f, "</style>\n");

    fprintf(f, "</head>\n");
    fprintf(f, "<body>\n");
    fprintf(f, "<h1>WCC Instruction Selection Rules Coverage</h1>\n");
    fprintf(f, "<table>\n");

    fprintf(f, "<tr>");
    fprintf(f, "<th>Rule</th>\n");
    fprintf(f, "<th>WCC tests</th>\n");
    fprintf(f, "<th>Instrsel tests</th>\n");
    fprintf(f, "<th>WCC2</th>\n");
    fprintf(f, "<th>Operation</th>\n");
    fprintf(f, "<th>Dst</th>\n");
    fprintf(f, "<th>Src1</th>\n");
    fprintf(f, "<th>Src2</th>\n");
    fprintf(f, "<th>Cost</th>\n");
    fprintf(f, "</tr>");

    covered_count = 0;
    for (i = 0; i <= rule_coverage->max_value; i++) {
        covered = wcc2_cov->elements[i] || instrsel_tests_cov->elements[i] || wcc_tests_cov->elements[i];
        if (covered) covered_count++;

        fprintf(f, "<tr>");
        fprintf(f, "<td>%d</td>\n", i);
        fprintf(f, "<td class='%s'</td>", wcc2_cov->elements[i] ? "inset" : "");
        fprintf(f, "<td class='%s'</td>", instrsel_tests_cov->elements[i] ? "inset" : "");
        fprintf(f, "<td class='%s'</td>", wcc_tests_cov->elements[i] ? "inset" : "");

        r = &(instr_rules[i]);
        fprintf(f, "<td class='%s'>%s</td>", covered ? "inset" : "", operation_string(r->operation));
        fprintf(f, "<td class='%s'>%s</td>", covered ? "inset" : "", non_terminal_string(r->dst));
        fprintf(f, "<td class='%s'>%s</td>", covered ? "inset" : "", non_terminal_string(r->src1));
        fprintf(f, "<td class='%s'>%s</td>", covered ? "inset" : "", non_terminal_string(r->src2));
        fprintf(f, "<td class='%s'>%d</td>", covered ? "inset" : "", r->cost);

        fprintf(f, "</td>");

        fprintf(f, "</tr>");
    }

    fprintf(f, "</table>\n");
    fprintf(f, "</body>\n");
    fprintf(f, "</html>\n");

    fclose(f);

    printf("%d%% (%d / %d) of the rules are covered \n", covered_count * 100 / instr_rule_count, covered_count, instr_rule_count);
}
