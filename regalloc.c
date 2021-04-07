#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"

// Indexes used to reference the physically reserved registers, starting at 0
int SSA_PREG_REG_RAX;
int SSA_PREG_REG_RBX;
int SSA_PREG_REG_RCX;
int SSA_PREG_REG_RDX;
int SSA_PREG_REG_RSI;
int SSA_PREG_REG_RDI;
int SSA_PREG_REG_RBP;
int SSA_PREG_REG_RSP;
int SSA_PREG_REG_R8;
int SSA_PREG_REG_R9;
int SSA_PREG_REG_R10;
int SSA_PREG_REG_R11;
int SSA_PREG_REG_R12;
int SSA_PREG_REG_R13;
int SSA_PREG_REG_R14;
int SSA_PREG_REG_R15;

typedef struct vreg_cost {
    int vreg;
    int cost;
} VregCost;

void quicksort_vreg_cost(VregCost *vreg_cost, int left, int right) {
    int i, j, pivot;
    int tmp_vreg, tmp_cost;

    if (left >= right) return;

    i = left;
    j = right;
    pivot = vreg_cost[i].cost;

    while (1) {
        while (vreg_cost[i].cost > pivot) i++;
        while (pivot > vreg_cost[j].cost) j--;
        if (i >= j) break;

        tmp_vreg = vreg_cost[i].vreg;
        tmp_cost = vreg_cost[i].cost;
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

int graph_node_degree(char *ig, int vreg_count, int node) {
    int i, result, offset;

    result = 0;
    offset = node * vreg_count;
    for (i = 1; i <= vreg_count; i++)
        if ((i < node && ig[i * vreg_count + node]) || ig[offset + i]) result++;

    return result;
}

void color_vreg(char *ig, int vreg_count, VregLocation *vreg_locations, int physical_register_count, int *spilled_register_count, int vreg) {
    int i, j, preg, offset;
    Set *neighbor_colors;

    neighbor_colors = new_set(physical_register_count);

    if (debug_graph_coloring) printf("Allocating register for vreg %d\n", vreg);
    offset = vreg * vreg_count;
    for (i = 1; i <= vreg_count; i++) {
        if ((i < vreg && ig[i * vreg_count + vreg]) || ig[offset + i]) {
            preg = vreg_locations[i].preg;
            if (preg != -1) add_to_set(neighbor_colors, preg);
        }
    }

    if (debug_graph_coloring) {
        printf("  Neighbor colors: ");
        print_set(neighbor_colors);
        printf("\n");
    }

    if (!opt_enable_register_allocation || set_len(neighbor_colors) == physical_register_count) {
        vreg_locations[vreg].stack_index = -*spilled_register_count - 1;
        (*spilled_register_count)++;
        if (debug_graph_coloring) printf("  spilled %d\n", vreg);
    }
    else {
        // Find first free physical register
        for (j = 0; j < physical_register_count; j++) {
            if (!in_set(neighbor_colors, j)) {
                vreg_locations[vreg].preg = j;
                if (debug_graph_coloring) printf("  allocated %d\n", j);
                return;
            }
        }

        panic("Should not get here");
    }

    free_set(neighbor_colors);
}

void allocate_registers_top_down(Function *function, int physical_register_count) {
    int i, vreg_count, *spill_cost, edge_count, degree, spilled_register_count, vreg, vreg_locations_count;
    char *interference_graph;
    VregLocation *vreg_locations;
    Set *constrained, *unconstrained;
    VregCost *ordered_nodes;

    if (debug_register_allocation) print_ir(function, 0);

    interference_graph = function->interference_graph;
    vreg_count = function->vreg_count;
    spill_cost = function->spill_cost;

    ordered_nodes = malloc((vreg_count + 1) * sizeof(VregCost));
    for (i = 1; i <= vreg_count; i++) {
        ordered_nodes[i].vreg = i;
        ordered_nodes[i].cost = spill_cost[i];
    }

    quicksort_vreg_cost(ordered_nodes, 1, vreg_count);

    constrained = new_set(vreg_count);
    unconstrained = new_set(vreg_count);

    for (i = 1; i <= vreg_count; i++) {
        degree = graph_node_degree(interference_graph, vreg_count, ordered_nodes[i].vreg);
        if (degree < physical_register_count)
            add_to_set(unconstrained, ordered_nodes[i].vreg);
        else
            add_to_set(constrained, ordered_nodes[i].vreg);
    }

    if (debug_register_allocation) {
        printf("Nodes in order of decreasing cost:\n");
        for (i = 1; i <= vreg_count; i++)
            printf("%d: cost=%d degree=%d\n", ordered_nodes[i].vreg, ordered_nodes[i].cost, graph_node_degree(interference_graph, vreg_count, ordered_nodes[i].vreg));

        printf("\nPriority sets:\n");
        printf("constrained:   "); print_set(constrained); printf("\n");
        printf("unconstrained: "); print_set(unconstrained); printf("\n\n");
    }

    vreg_locations_count = vreg_count > PHYSICAL_REGISTER_COUNT ? vreg_count : PHYSICAL_REGISTER_COUNT;
    vreg_locations = malloc((vreg_locations_count + 1) * sizeof(VregLocation));
    for (i = 1; i <= vreg_count; i++) {
        vreg_locations[i].preg = -1;
        vreg_locations[i].stack_index = 0;
    }

    spilled_register_count = function->spilled_register_count;

    // Pre-color reserved registers
    if (live_range_reserved_pregs_offset > 0) {
        // RBP, RSP, R10, R11 are reserved and not in this list
        vreg_locations[LIVE_RANGE_PREG_RAX_INDEX].preg = SSA_PREG_REG_RAX;
        vreg_locations[LIVE_RANGE_PREG_RBX_INDEX].preg = SSA_PREG_REG_RBX;
        vreg_locations[LIVE_RANGE_PREG_RCX_INDEX].preg = SSA_PREG_REG_RCX;
        vreg_locations[LIVE_RANGE_PREG_RDX_INDEX].preg = SSA_PREG_REG_RDX;
        vreg_locations[LIVE_RANGE_PREG_RSI_INDEX].preg = SSA_PREG_REG_RSI;
        vreg_locations[LIVE_RANGE_PREG_RDI_INDEX].preg = SSA_PREG_REG_RDI;
        vreg_locations[LIVE_RANGE_PREG_R8_INDEX ].preg = SSA_PREG_REG_R8;
        vreg_locations[LIVE_RANGE_PREG_R9_INDEX ].preg = SSA_PREG_REG_R9;
        vreg_locations[LIVE_RANGE_PREG_R12_INDEX].preg = SSA_PREG_REG_R12;
        vreg_locations[LIVE_RANGE_PREG_R13_INDEX].preg = SSA_PREG_REG_R13;
        vreg_locations[LIVE_RANGE_PREG_R14_INDEX].preg = SSA_PREG_REG_R14;
        vreg_locations[LIVE_RANGE_PREG_R15_INDEX].preg = SSA_PREG_REG_R15;
    }

    // Color constrained nodes first
    for (i = 1; i <= vreg_count; i++) {
        vreg = ordered_nodes[i].vreg;
        if (!constrained->elements[vreg]) continue;
        if (live_range_reserved_pregs_offset > 0 && vreg <= RESERVED_PHYSICAL_REGISTER_COUNT) continue;
        color_vreg(interference_graph, vreg_count, vreg_locations, physical_register_count, &spilled_register_count, vreg);
    }

    // Color unconstrained nodes
    for (i = 1; i <= vreg_count; i++) {
        vreg = ordered_nodes[i].vreg;
        if (!unconstrained->elements[vreg]) continue;
        if (live_range_reserved_pregs_offset > 0 && vreg <= RESERVED_PHYSICAL_REGISTER_COUNT) continue;
        color_vreg(interference_graph, vreg_count, vreg_locations, physical_register_count, &spilled_register_count, vreg);
    }

    if (debug_register_allocation) {
        printf("Assigned physical registers and stack indexes:\n");

        for (i = 1; i <= vreg_count; i++) {
            printf("%-3d ", i);
            if (vreg_locations[i].preg == -1) printf("    "); else printf("%3d", vreg_locations[i].preg);
            if (vreg_locations[i].stack_index) printf("    "); else printf("%3d", vreg_locations[i].stack_index);
            printf("\n");
        }
    }

    function->vreg_locations = vreg_locations;
    function->spilled_register_count = spilled_register_count;

    free_set(constrained);
    free_set(unconstrained);
}

void init_allocate_registers() {
    int i;
    Set *reserved_registers;

    preg_map = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(preg_map, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    // Determine amount of free physical registers
    physical_registers = malloc(sizeof(int) * PHYSICAL_REGISTER_COUNT);
    memset(physical_registers, 0, sizeof(int) * PHYSICAL_REGISTER_COUNT);

    // Blacklist registers if certain operations are happening in this function.
    reserved_registers = new_set(PHYSICAL_REGISTER_COUNT);
    add_to_set(reserved_registers, REG_RSP); // Stack pointer
    add_to_set(reserved_registers, REG_RBP); // Base pointer
    add_to_set(reserved_registers, REG_R10); // Not preserved in function calls & used as temporary
    add_to_set(reserved_registers, REG_R11); // Not preserved in function calls & used as temporary

    preg_count = 0;
    for (i = 0; i < PHYSICAL_REGISTER_COUNT; i++)
        if (!in_set(reserved_registers, i)) {
            if (i == REG_RAX) SSA_PREG_REG_RAX = preg_count;
            if (i == REG_RBX) SSA_PREG_REG_RBX = preg_count;
            if (i == REG_RCX) SSA_PREG_REG_RCX = preg_count;
            if (i == REG_RDX) SSA_PREG_REG_RDX = preg_count;
            if (i == REG_RSI) SSA_PREG_REG_RSI = preg_count;
            if (i == REG_RDI) SSA_PREG_REG_RDI = preg_count;
            if (i == REG_R8)  SSA_PREG_REG_R8  = preg_count;
            if (i == REG_R9)  SSA_PREG_REG_R9  = preg_count;
            if (i == REG_R12) SSA_PREG_REG_R12 = preg_count;
            if (i == REG_R13) SSA_PREG_REG_R13 = preg_count;
            if (i == REG_R14) SSA_PREG_REG_R14 = preg_count;
            if (i == REG_R15) SSA_PREG_REG_R15 = preg_count;

            preg_map[preg_count++] = i;
        }
}

void assign_vreg_locations(Function *function) {
    Tac *tac;
    VregLocation *function_vl, *vl;

    function_vl = function->vreg_locations;

    tac = function->ir;
    while (tac) {
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

        tac = tac->next;
    }

    function->local_symbol_count = 0; // This nukes ancient code that assumes local vars are on the stack
}

void remove_preg_self_moves(Function *function) {
    // This removes instructions that copy a physical register to itself by replacing them with noops.

    Tac *tac;

    tac = function->ir;
    while (tac) {
        if ((tac->operation == IR_MOVE || tac->operation == X_MOV) && tac->dst && tac->dst->preg != -1 && tac->src1 && tac->src1->preg != -1 && tac->dst->preg == tac->src1->preg)
            tac->operation = IR_NOP;

        tac = tac->next;
    }
}

void allocate_registers(Function *function) {
    int i, vreg_count;

    allocate_registers_top_down(function, preg_count);

    // Remap SSA pregs which run from 0 to preg_count -1 to the actual
    // x86_64 physical register numbers.
    vreg_count = function->vreg_count;
    for (i = 1; i <= vreg_count; i++) {
        if (function->vreg_locations[i].preg != -1)
            function->vreg_locations[i].preg = preg_map[function->vreg_locations[i].preg];
    }

    total_spilled_register_count += function->spilled_register_count;

    assign_vreg_locations(function);
    remove_preg_self_moves(function);
}

