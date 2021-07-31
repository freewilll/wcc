#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

typedef struct vreg_cost {
    int vreg;
    int cost;
} VregCost;

int *preg_map;              // Map from reserved register 0-11 to physical register 0-15

// Renumber all vregs so that they are consecutive
void compress_vregs(Function *function) {
    if (!opt_enable_vreg_renumbering) return;

    make_vreg_count(function, 0);
    int old_vreg_count = function->vreg_count;
    int *vreg_map = malloc((old_vreg_count + 1) * sizeof(int));
    memset(vreg_map, 0, (old_vreg_count + 1) * sizeof(int));
    int new_vreg_count = live_range_reserved_pregs_offset;

    if (debug_ssa_vreg_renumbering) {
        printf("Before vreg renumbering:\n");
        print_ir(function, 0, 0);
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst && tac->dst->vreg) if (!vreg_map[tac->dst->vreg]) vreg_map[tac->dst->vreg] = ++new_vreg_count;
        if (tac->src1 && tac->src1->vreg) if (!vreg_map[tac->src1->vreg]) vreg_map[tac->src1->vreg] = ++new_vreg_count;
        if (tac->src2 && tac->src2->vreg) if (!vreg_map[tac->src2->vreg]) vreg_map[tac->src2->vreg] = ++new_vreg_count;

        if (tac->dst ) tac->dst ->has_been_renamed = 0;
        if (tac->src1) tac->src1->has_been_renamed = 0;
        if (tac->src2) tac->src2->has_been_renamed = 0;
    }

    if (debug_ssa_vreg_renumbering) {
        printf("\nVreg renames:\n");
        for (int i = 1; i <= old_vreg_count; i++) {
            if (vreg_map[i]) printf("%6d -> %6d\n", i, vreg_map[i]);
        }
    }

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst  && !tac->dst ->has_been_renamed && tac->dst ->vreg) { tac->dst ->vreg = vreg_map[tac ->dst->vreg]; tac->dst ->has_been_renamed = 1; }
        if (tac->src1 && !tac->src1->has_been_renamed && tac->src1->vreg) { tac->src1->vreg = vreg_map[tac->src1->vreg]; tac->src1->has_been_renamed = 1; }
        if (tac->src2 && !tac->src2->has_been_renamed && tac->src2->vreg) { tac->src2->vreg = vreg_map[tac->src2->vreg]; tac->src2->has_been_renamed = 1; }
    }

    function->vreg_count = new_vreg_count;

    if (debug_ssa_vreg_renumbering) {
        printf("\nAfter vreg renumbering:\n");
        print_ir(function, 0, 0);
    }

    free(vreg_map);
}


static void quicksort_vreg_cost(VregCost *vreg_cost, int left, int right) {
    if (left >= right) return;

    int i = left;
    int j = right;
    int pivot = vreg_cost[i].cost;

    while (1) {
        while (vreg_cost[i].cost > pivot) i++;
        while (pivot > vreg_cost[j].cost) j--;
        if (i >= j) break;

        int tmp_vreg = vreg_cost[i].vreg;
        int tmp_cost = vreg_cost[i].cost;
        vreg_cost[i].vreg = vreg_cost[j].vreg;
        vreg_cost[i].cost = vreg_cost[j].cost;
        vreg_cost[j].vreg = tmp_vreg;
        vreg_cost[j].cost = tmp_cost;

        i++;
        j--;
    }

    quicksort_vreg_cost(vreg_cost, left, i - 1);
    quicksort_vreg_cost(vreg_cost, j + 1, right);
}

static int *make_original_stack_indexes(Function *function) {
    int *result = malloc(sizeof(int *) * (function->vreg_count + 1));
    memset(result, 0, sizeof(int *) * (function->vreg_count + 1));

    for (Tac *tac = function->ir; tac; tac = tac->next)
        if (tac->operation == X_MOV && tac->src1 && tac->src1->is_function_param && tac->src1->function_param_original_stack_index)
            result[tac->dst->vreg] = tac->src1->function_param_original_stack_index;

    return result;
}

static int graph_node_degree(char *ig, int vreg_count, int node) {
    int result = 0;
    int offset = node * vreg_count;
    for (int i = 1; i <= vreg_count; i++)
        if ((i < node && ig[i * vreg_count + node]) || ig[offset + i]) result++;

    return result;
}

static void color_vreg(char *ig, int vreg_count, VregLocation *vreg_locations,
    int physical_register_count, int *stack_register_count, int vreg, int *original_stack_indexes,
    int preferred_live_range_preg_index,
    int preg_live_range_start, int preg_live_range_end) {

    // The 16 is for unit tests
    Set *neighbor_colors = new_set(live_range_reserved_pregs_offset == 0 ? 16 : live_range_reserved_pregs_offset);

    if (debug_graph_coloring) printf("Allocating register for vreg %d, live range %d-%d\n", vreg, preg_live_range_start, preg_live_range_end);

    int offset = vreg * vreg_count;
    for (int i = 1; i <= vreg_count; i++) {
        if (i <= live_range_reserved_pregs_offset && (i < preg_live_range_start || i > preg_live_range_end)) continue;

        if ((i < vreg && ig[i * vreg_count + vreg]) || ig[offset + i]) {
            int preg = vreg_locations[i].preg;
            if (preg != -1) add_to_set(neighbor_colors, preg);
        }
    }

    if (debug_graph_coloring) {
        printf("  Neighbor colors: ");
        print_set(neighbor_colors);
        printf("\n");
    }

    if (set_len(neighbor_colors) >= physical_register_count) {
        int stack_index;
        if (original_stack_indexes[vreg])
            stack_index = original_stack_indexes[vreg];
        else {
            stack_index = -*stack_register_count - 1;
            (*stack_register_count)++;
        }
        vreg_locations[vreg].stack_index = stack_index;

        if (debug_graph_coloring) printf("  spilled vreg %d to stack index %d\n", vreg, stack_index);
    }
    else {
        if (debug_graph_coloring && preferred_live_range_preg_index) printf("  searching for preferred preg live range index %d\n", preferred_live_range_preg_index);
        if (preferred_live_range_preg_index && !in_set(neighbor_colors, vreg_locations[preferred_live_range_preg_index].preg)) {
            vreg_locations[vreg].preg = vreg_locations[preferred_live_range_preg_index].preg;
            if (debug_graph_coloring) printf("  allocated preferred LR %d preg %d\n", preferred_live_range_preg_index, vreg_locations[preferred_live_range_preg_index].preg);
            return;
        }
        else {
            // Find first free register in the range preg_live_range_start - 1 to preg_live_range_end - 1
            // The reason for the -1, is that live ranges start at 1, but pregs start at zero
            for (int j = preg_live_range_start - 1; j < preg_live_range_end; j++) {
                if (!in_set(neighbor_colors, j)) {
                    vreg_locations[vreg].preg = j;
                    if (debug_graph_coloring) printf("  allocated %d to vreg %d\n", j, vreg);
                    return;
                }
            }
        }

        panic("Should not get here");
    }

    free_set(neighbor_colors);
}

// Allocate/spill registers for all vregs of class preg_class.
void allocate_registers_top_down(Function *function, int live_range_start, int physical_register_count, int preg_class) {
    char *interference_graph = function->interference_graph;
    VregLocation *vreg_locations = function->vreg_locations;
    int vreg_count = function->vreg_count;
    int *spill_cost = function->spill_cost;
    char *preferred_live_range_preg_indexes = function->preferred_live_range_preg_indexes;
    int *original_stack_indexes = make_original_stack_indexes(function);
    int live_range_end = live_range_start + physical_register_count - 1;

    if (debug_register_allocation) {
        printf("Allocating registers for live_range_start=%d, live_range_end=%d physical_register_count=%d\n", live_range_start, live_range_end, physical_register_count);
        print_ir(function, 0, 0);
    }

    VregCost *ordered_nodes = malloc((vreg_count + 1) * sizeof(VregCost));
    for (int i = 1; i <= vreg_count; i++) {
        ordered_nodes[i].vreg = i;
        ordered_nodes[i].cost = spill_cost[i];
    }

    quicksort_vreg_cost(ordered_nodes, 1, vreg_count);

    Set *constrained = new_set(vreg_count);
    Set *unconstrained = new_set(vreg_count);
    Set *preferred_pregs = new_set(vreg_count);

    for (int i = 1; i <= vreg_count; i++) {
        int vreg = ordered_nodes[i].vreg;
        if (vreg > live_range_reserved_pregs_offset && function->vreg_preg_classes[vreg] != preg_class) continue;
        if (!function->vreg_preg_classes[vreg]) panic1d("Unexpected zero preg class for vreg %d", i);

        int degree = graph_node_degree(interference_graph, vreg_count, vreg);
        if (degree < physical_register_count)
            if (opt_enable_preferred_pregs && preferred_live_range_preg_indexes[vreg])
                add_to_set(preferred_pregs, vreg);
            else
                add_to_set(unconstrained, vreg);
        else
            add_to_set(constrained, vreg);
    }

    if (debug_register_allocation) {
        printf("Nodes in order of decreasing cost:\n");
        for (int i = 1; i <= vreg_count; i++)
            printf("%d: cost=%d degree=%d\n", ordered_nodes[i].vreg, ordered_nodes[i].cost, graph_node_degree(interference_graph, vreg_count, ordered_nodes[i].vreg));

        printf("\nPriority sets:\n");
        printf("constrained:     "); print_set(constrained); printf("\n");
        printf("unconstrained:   "); print_set(unconstrained); printf("\n");
        printf("preferred_pregs: "); print_set(preferred_pregs); printf("\n\n");
    }

    // Pre-color reserved registers. Vregs start at one, pregs start at 0.
    if (live_range_reserved_pregs_offset > 0)
        for (int i = 0; i < live_range_reserved_pregs_offset; i++) vreg_locations[i + 1].preg = i;

    int stack_register_count = function->stack_register_count;

    if (debug_ssa_interference_graph) {
        printf("Live range preg index -> preg map:\n");
        for (int i = 1; i < live_range_reserved_pregs_offset; i++)
            printf("%-2d -> %-2d: %s\n", i, vreg_locations[i].preg, register_name(vreg_locations[i].preg));
    }

    // Color constrained nodes first
    for (int i = 1; i <= vreg_count; i++) {
        int vreg = ordered_nodes[i].vreg;
        if (!constrained->elements[vreg]) continue;
        if (live_range_reserved_pregs_offset > 0 && vreg <= live_range_reserved_pregs_offset) continue;
        color_vreg(interference_graph, vreg_count, vreg_locations, physical_register_count, &stack_register_count, vreg, original_stack_indexes, 0, live_range_start, live_range_end);
    }

    // Color preferred preg nodes next
    for (int i = 1; i <= vreg_count; i++) {
        int vreg = ordered_nodes[i].vreg;
        if (!preferred_pregs->elements[vreg]) continue;
        if (live_range_reserved_pregs_offset > 0 && vreg <= live_range_reserved_pregs_offset) continue;
        color_vreg(interference_graph, vreg_count, vreg_locations, physical_register_count, &stack_register_count, vreg, original_stack_indexes, preferred_live_range_preg_indexes[vreg], live_range_start, live_range_end);
    }

    // Color unconstrained nodes lsat
    for (int i = 1; i <= vreg_count; i++) {
        int vreg = ordered_nodes[i].vreg;
        if (!unconstrained->elements[vreg]) continue;
        if (live_range_reserved_pregs_offset > 0 && vreg <= live_range_reserved_pregs_offset) continue;
        color_vreg(interference_graph, vreg_count, vreg_locations, physical_register_count, &stack_register_count, vreg, original_stack_indexes, 0, live_range_start, live_range_end);
    }

    if (debug_register_allocation) {
        printf("Assigned physical registers and stack indexes for live_range_start=%d:\n", live_range_start);

        for (int i = live_range_reserved_pregs_offset + 1; i <= vreg_count; i++) {
            printf("%-3d ", i);
            if (vreg_locations[i].preg == -1) printf("    "); else printf("%3d", vreg_locations[i].preg);
            if (vreg_locations[i].stack_index) printf("    "); else printf("%3d", vreg_locations[i].stack_index);
            printf("\n");
        }
    }

    function->stack_register_count = stack_register_count;

    free_set(constrained);
    free_set(unconstrained);
}

// Called once at startup
void init_allocate_registers() {
    // Which registers are preserved across function calls
    callee_saved_registers = malloc(sizeof(int) * (PHYSICAL_REGISTER_COUNT + 1));
    memset(callee_saved_registers, 0, sizeof(int) * (PHYSICAL_REGISTER_COUNT + 1));

    callee_saved_registers[REG_RBX] = 1;
    callee_saved_registers[REG_R12] = 1;
    callee_saved_registers[REG_R13] = 1;
    callee_saved_registers[REG_R14] = 1;
    callee_saved_registers[REG_R15] = 1;

    // Registers used for function calls
    int_arg_registers = malloc(sizeof(int) * 6);
    int_arg_registers[0]  = LIVE_RANGE_PREG_RDI_INDEX;
    int_arg_registers[1]  = LIVE_RANGE_PREG_RSI_INDEX;
    int_arg_registers[2]  = LIVE_RANGE_PREG_RDX_INDEX;
    int_arg_registers[3]  = LIVE_RANGE_PREG_RCX_INDEX;
    int_arg_registers[4]  = LIVE_RANGE_PREG_R08_INDEX;
    int_arg_registers[5]  = LIVE_RANGE_PREG_R09_INDEX;

    // Map SSE xmm0-xmm7 argument registers
    sse_arg_registers = malloc(sizeof(int) * 8);
    for (int i = 0; i < 8; i++) sse_arg_registers[i]  = LIVE_RANGE_PREG_XMM00_INDEX + i;

    // Map from reserved register 0-11 to physical register 0-15
    preg_map = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(preg_map, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    // All registers except RSP, RBP, R10 and R11
    preg_map[LIVE_RANGE_PREG_RAX_INDEX - 1] = REG_RAX;
    preg_map[LIVE_RANGE_PREG_RBX_INDEX - 1] = REG_RBX;
    preg_map[LIVE_RANGE_PREG_RCX_INDEX - 1] = REG_RCX;
    preg_map[LIVE_RANGE_PREG_RDX_INDEX - 1] = REG_RDX;
    preg_map[LIVE_RANGE_PREG_RSI_INDEX - 1] = REG_RSI;
    preg_map[LIVE_RANGE_PREG_RDI_INDEX - 1] = REG_RDI;
    preg_map[LIVE_RANGE_PREG_R08_INDEX - 1] = REG_R08;
    preg_map[LIVE_RANGE_PREG_R09_INDEX - 1] = REG_R09;
    preg_map[LIVE_RANGE_PREG_R12_INDEX - 1] = REG_R12;
    preg_map[LIVE_RANGE_PREG_R13_INDEX - 1] = REG_R13;
    preg_map[LIVE_RANGE_PREG_R14_INDEX - 1] = REG_R14;
    preg_map[LIVE_RANGE_PREG_R15_INDEX - 1] = REG_R15;

    // Map all 16 SSE xmm* registers
    for (int i = 0; i < SSE_REGISTER_COUNT; i++)
        preg_map[LIVE_RANGE_PREG_XMM00_INDEX + i - 1] = REG_XMM00 + i;

    live_range_reserved_pregs_offset = INT_REGISTER_COUNT + SSE_REGISTER_COUNT;
}

static void assign_vreg_locations(Function *function) {
    VregLocation *vl;
    VregLocation *function_vl = function->vreg_locations;

    for (Tac *tac = function->ir; tac; tac = tac->next) {
        if (tac->dst && tac->dst->vreg) {
            vl = &function_vl[tac->dst->vreg];
            if (vl->stack_index) {
                tac->dst->stack_index = vl->stack_index;
                tac->dst->spilled = 1;
            }
            else
                tac->dst->preg = vl->preg;
        }

        if (tac->src1 && tac->src1->vreg) {
            vl = &function_vl[tac->src1->vreg];
            if (vl->stack_index) {
                tac->src1->stack_index = vl->stack_index;
                tac->src1->spilled = 1;
            }
            else
                tac->src1->preg = vl->preg;
        }

        if (tac->src2 && tac->src2->vreg) {
            vl = &function_vl[tac->src2->vreg];
            if (vl->stack_index) {
                tac->src2->stack_index = vl->stack_index;
                tac->src2->spilled = 1;
            }
            else
                tac->src2->preg = vl->preg;
        }
    }

    function->local_symbol_count = 0; // This nukes ancient code that assumes local vars are on the stack
}

// This removes instructions that copy a physical register to itself by replacing them with noops.
static void remove_preg_self_moves(Function *function) {
    for (Tac *tac = function->ir; tac; tac = tac->next)
        if (tac->dst && tac->dst->preg != -1 && tac->src1 && tac->src1->preg != -1 && tac->dst->preg == tac->src1->preg)
            if (tac->operation == X_MOV) tac->operation = IR_NOP;
}

// Initialize vreg_locations, which maps vregs to either a preg or a stack index
void init_vreg_locations(Function *function) {
    int vreg_count = function->vreg_count;
    int vreg_locations_count = vreg_count > PHYSICAL_REGISTER_COUNT ? vreg_count : PHYSICAL_REGISTER_COUNT;
    VregLocation *vreg_locations = malloc((vreg_locations_count + 1) * sizeof(VregLocation));
    for (int i = 1; i <= vreg_count; i++) {
        vreg_locations[i].preg = -1;
        vreg_locations[i].stack_index = 0;
    }

    function->vreg_locations = vreg_locations;
}

void allocate_registers(Function *function) {
    init_vreg_locations(function);

    // Allocate integer registers
    int physical_int_register_count = live_range_reserved_pregs_offset == 0 ? 0 : INT_REGISTER_COUNT;
    allocate_registers_top_down(function, 1, physical_int_register_count, PC_INT);

    // Allocate floating point xmm* registers
    int physical_sse_register_count = live_range_reserved_pregs_offset == 0 ? 0 : SSE_REGISTER_COUNT;
    allocate_registers_top_down(function, 13, physical_sse_register_count, PC_SSE);

    // Remap SSA pregs which run from 0 to live_range_reserved_pregs_offset -1 to the actual
    // x86_64 physical register numbers.
    int vreg_count = function->vreg_count;
    for (int i = 1; i <= vreg_count; i++)
        if (function->vreg_locations[i].preg != -1)
            function->vreg_locations[i].preg = preg_map[function->vreg_locations[i].preg];

    total_stack_register_count += function->stack_register_count;

    assign_vreg_locations(function);
    remove_preg_self_moves(function);
}

