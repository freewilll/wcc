#include <stdio.h>
#include <stdlib.h>

#include "wcc.h"

// Dynamically defined rules in instrrules.c and dump the result out to a .c file with
// global declarations. That file is then compiled & linked into compiler.
int main(int argc, char **argv) {
    define_rules();

    printf("// Auto generated by %s\n\n", argv[0]);

    printf("#include \"wcc.h\"\n\n");

    LongMap *operation_counts = new_longmap();

    for (int i = 0; i < instr_rule_count; i++) {
        Rule *r = &(instr_rules[i]);

        int count = r->x86_operation_count;

        if (count) {
            printf("static X86Operation rule_%d_operations[] = {\n", i);
            for (int j = 0; j < r->x86_operation_count; j++) {
                X86Operation *op = &r->x86_operations[j];

                printf("    { %d, %d, %d, %d, ", op->operation, op->dst, op->v1, op->v2);
                if (op->template)
                    printf("\"%s\", ", op->template);
                else
                    printf("NULL, ");

                if (op->save_value_in_slot + op->allocate_stack_index_in_slot +
                        op->allocate_register_in_slot + op->allocate_label_in_slot +
                        op->allocated_type + op->arg != 0) {
                    printf("%d, %d, %d, %d, %d, %d },\n",
                        op->save_value_in_slot, op->allocate_stack_index_in_slot,
                        op->allocate_register_in_slot, op->allocate_label_in_slot,
                        op->allocated_type, op->arg /*, next */);
                }
                else
                    printf("},\n");
            }

            printf("};\n\n");
        }

        longmap_put(operation_counts, i, (void *) (long) count);
    }

    printf("\n");
    printf("static Rule local_instr_rules[%d] = {\n", instr_rule_count);

    for (int i = 0; i < instr_rule_count; i++) {
        Rule *r = &(instr_rules[i]);
        printf("    { ");
        printf("%d, ", r->index);
        printf("%d, ", r->operation);
        printf("%d, ", r->dst);
        printf("%d, ", r->src1);
        printf("%d, ", r->src2);
        printf("%d, ", r->cost);

        int operation_count = (long) longmap_get(operation_counts, i);
        if (operation_count)
            printf("rule_%d_operations, ", i);
        else
            printf("NULL, ");

        printf("%d, ", operation_count);

        printf("},\n");
    }
    printf("};\n\n");

    printf("void init_generated_instruction_selection_rules(void) {\n");
    printf("    instr_rule_count = %d;\n", instr_rule_count);
    printf("    instr_rules = local_instr_rules;\n");
    printf("}\n");

    free_longmap(operation_counts);
}
