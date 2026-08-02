#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wcc.h"
#include "../../aarch64.h"

Function *function;

extern int test_inflate_stack_size;

void assert_long(long expected, long actual);

void assert_int(int expected, int actual, char *message) {
    if (expected != actual) {
        failures++;
        printf("%-18s failed expected %d, got %d\n", message, expected, actual);
    }
}

static void n() {
    ir_start = ir_start->next;
}

static void assert_preg_op(char *expected) {
    char *got;

    if (!ir_start && expected) {
        printf("Expected %s, got nothing\n", expected);
        failures++;
        return;
    }
    else if (!ir_start && !expected) return;

    got = 0;
    while (ir_start && !got) {
        got = render_target_operation(ir_start, 0, 1);
        n();
    }

    if (!expected) {
        printf("Expected nothing, got %s\n", got);
        failures++;
        return;
    }

    if (got && expected && strcmp(got, expected)) {
        printf("Mismatch:\n  expected: %s\n  got:      %s\n", expected, got);
        failures++;
    }
}

static void add_some_final_instructions(Function *function) {
    // Note: deliberately omitting expand_adrp_instructions so that the assertions can be simple

    add_load_memory_instructions(function);
    add_store_memory_instructions(function);
    add_address_of_instructions(function);
    expand_indirect_offsets(function);
}

// Run with a single instruction
void si(Function *function, int label, int operation, Value *dst, Value *src1, Value *src2) {
    start_ir();
    i(label, operation, dst, src1, src2);
    finish_ir(function);
    add_some_final_instructions(function);
}

// Rewind the IR so that it points to the first non-nop in the funciton.
// This is usful for checking inserted code.
void rewind_ir(void) {
    ir_start = function->ir;

    while (ir_start->prev) ir_start = ir_start->prev;
    while (ir_start && ir_start->operation.id == IR_NOP) ir_start = ir_start->next;
    function->ir = ir_start;
}

void finish_spill_ir_and_memory_load_stores(void) {
    finish_spill_ir(function);
    add_some_final_instructions(function);
    rewind_ir();
}

void test_global_loads(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_MOVE v << g
    si(function, 0, IR_MOVE, vsz(3, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    assert_target_op("adrp        r3x, g1");
    assert_target_op("ldrsb       r2w, [r3x]");

    si(function, 0, IR_MOVE, vsz(3, TYPE_SHORT), gsz(1, TYPE_SHORT), 0);
    assert_target_op("adrp        r3x, g1");
    assert_target_op("ldrsh       r2w, [r3x]");

    si(function, 0, IR_MOVE, vsz(3, TYPE_INT), gsz(1, TYPE_INT), 0);
    assert_target_op("adrp        r3x, g1");
    assert_target_op("ldr         r2w, [r3x]");

    si(function, 0, IR_MOVE, vsz(3, TYPE_LONG), gsz(1, TYPE_LONG), 0);
    assert_target_op("adrp        r3x, g1");
    assert_target_op("ldr         r2x, [r3x]");

    // IR_MOVE v << g with a small offset
    start_ir();
    tac = i(0, IR_MOVE, vsz(3, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    tac->src1->offset = 8;
    finish_ir(function);
    add_some_final_instructions(function);
    assert_target_op("adrp        r3x, g1");
    assert_target_op("add         r3x, r3x, 8");
    assert_target_op("ldrsb       r2w, [r3x]");

    // IR_MOVE v << g with a large offset
    start_ir();
    tac = i(0, IR_MOVE, vsz(3, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x0, g1");
    assert_preg_op("movz        x16, 5000");
    assert_preg_op("add         x0, x0, x16");
    assert_preg_op("ldrsb       w0, [x0]");
}

void test_global_stores(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_MOVE g << v
    start_ir();
    i(0, IR_MOVE, gsz(1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("strb        w0, [x17]");

    start_ir();
    i(0, IR_MOVE, gsz(1, TYPE_SHORT), vsz(3, TYPE_SHORT), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("strh        w0, [x17]");

    i(0, IR_MOVE, gsz(1, TYPE_INT), vsz(3, TYPE_INT), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("str         w0, [x17]");

    i(0, IR_MOVE, gsz(1, TYPE_LONG), vsz(3, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("str         x0, [x17]");

    // IR_MOVE g << v with a small offset
    tac = i(0, IR_MOVE, gsz(1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    tac->dst->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("strb        w0, [x17, 8]");

    // IR_MOVE g << v with a large offset
    // x16 is used to hold the large constant
    tac = i(0, IR_MOVE, gsz(1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    tac->dst->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("movz        x16, 5000");
    assert_preg_op("add         x17, x17, x16");
    assert_preg_op("strb        w0, [x17]");

    live_range_reserved_pregs_offset = 0; // Disable register allocation

    // IR_MOVE g << v with everything spilled
    start_ir();
    i(0, IR_MOVE, gsz(1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrb        w15, [sp, 15]");
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("strb        w15, [x17]");

    // IR_MOVE g << v with a small offset with a spilled value
    start_ir();
    tac = i(0, IR_MOVE, gsz(1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    tac->dst->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrb        w15, [sp, 15]");
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("strb        w15, [x17, 8]");

    // IR_MOVE g << v with a large offset with a spilled value
    // x16 is used to hold the large constant
    start_ir();
    tac = i(0, IR_MOVE, gsz(1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    tac->dst->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrb        w15, [sp, 15]");
    assert_preg_op("adrp        x17, g1");
    assert_preg_op("movz        x16, 5000");
    assert_preg_op("add         x17, x17, x16");
    assert_preg_op("strb        w15, [x17]");

    init_allocate_registers(); // Enable register allocation again
}

void test_global_address_of(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_ADDRESS_OF v = &g
    start_ir();
    i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x0, g1");

    // IR_ADDRESS_OF v = &g with a small offset
    start_ir();
    tac = i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    tac->src1->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x0, g1");
    assert_preg_op("add         x0, x0, 8");

    // IR_ADDRESS_OF v = &g with a large offset
    start_ir();
    tac = i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x0, g1");
    assert_preg_op("movz        x16, 5000");
    assert_preg_op("add         x0, x0, x16");

    live_range_reserved_pregs_offset = 0; // Disable register allocation

    // IR_ADDRESS_OF v = &g with everything spilled
    start_ir();
    i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x14, g1");
    assert_preg_op("str         x14, [sp, 8]");

    // IR_ADDRESS_OF v = &g with a small offset and everything spilled
    start_ir();
    tac = i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    tac->src1->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x14, g1");
    assert_preg_op("add         x14, x14, 8");
    assert_preg_op("str         x14, [sp, 8]");

    // IR_ADDRESS_OF v = &g with a large offset and everything spilled
    start_ir();
    tac = i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), gsz(1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("adrp        x14, g1");
    assert_preg_op("movz        x16, 5000");
    assert_preg_op("add         x14, x14, x16");
    assert_preg_op("str         x14, [sp, 8]");

    init_allocate_registers(); // Enable register allocation again
}

void test_stack_loads(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_MOVE v << s
    start_ir();
    i(0, IR_MOVE, vsz(3, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrsb       w0, [sp, 15]");

    start_ir();
    i(0, IR_MOVE, vsz(3, TYPE_SHORT), Ssz(-1, TYPE_SHORT), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrsh       w0, [sp, 14]");

    start_ir();
    i(0, IR_MOVE, vsz(3, TYPE_INT), Ssz(-1, TYPE_INT), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         w0, [sp, 12]");

    start_ir();
    i(0, IR_MOVE, vsz(3, TYPE_LONG), Ssz(-1, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         x0, [sp, 8]");

    // IR_MOVE v << s with a small offset
    start_ir();
    tac = i(0, IR_MOVE, vsz(3, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    tac->src1->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrsb       w0, [sp, 23]");

    // IR_MOVE v << s with a large offset
    start_ir();
    tac = i(0, IR_MOVE, vsz(3, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("movz        x17, 5015");
    assert_preg_op("add         x17, sp, x17");
    assert_preg_op("ldrsb       w0, [x17]");

    live_range_reserved_pregs_offset = 0; // Disable register allocation

    // IR_MOVE v << s with everything spilled
    start_ir();
    i(0, IR_MOVE, vsz(3, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrsb       w14, [sp, 15]");
    assert_preg_op("strb        w14, [sp, 14]");

    // IR_MOVE v << s with a small offset with a spilled value
    start_ir();
    tac = i(0, IR_MOVE, vsz(3, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    tac->src1->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrsb       w14, [sp, 23]");
    assert_preg_op("strb        w14, [sp, 14]");

    // Some of the generated code that follows is quite bad.
    // The generated code is a combination of spill code followed by load/store adaptations
    // on the spilled code.
    // - Instrsel allocates a temp register and generates instructions to load from the stack into the register
    // - Spill code spills everything: the temp register and destination.

    // IR_MOVE v << s with a large offset with a spilled value
    start_ir();
    tac = i(0, IR_MOVE, vsz(3, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("movz        x17, 5015");
    assert_preg_op("add         x17, sp, x17");
    assert_preg_op("ldrsb       w14, [x17]");       // Load into spilled temp register
    assert_preg_op("strb        w14, [sp, 14]");    // Store into spilled temp register
    assert_preg_op("ldrb        w14, [sp, 14]");    // Reload into spilled temp register
    assert_preg_op("mov         w14, w14");         // Load/store join
    assert_preg_op("strb        w14, [sp, 13]");    // Spill store

    // IR_MOVE s << v with no offset with a spilled value and a stack slot with a small offset.
    test_inflate_stack_size = 32;
    start_ir();
    i(0, IR_MOVE, vsz(2, TYPE_LONG), Ssz(-1, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         x14, [sp, 56]");    // Load into spilled temp register
    assert_preg_op("str         x14, [sp, 48]");    // Store into spilled temp register
    assert_preg_op("ldr         x14, [sp, 48]");    // Reload into spilled temp register
    assert_preg_op("mov         x14, x14");         // Load/store join
    assert_preg_op("str         x14, [sp, 40]");    // Spill store

    // IR_MOVE s << v with no offset with a spilled value and a stack slot with a large offset.
    test_inflate_stack_size = 40000;
    start_ir();
    i(0, IR_MOVE, vsz(2, TYPE_LONG), Ssz(-1, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("movz        x17, 40024");
    assert_preg_op("add         x17, sp, x17");
    assert_preg_op("ldr         x14, [x17]");       // Load into spilled temp register
    assert_preg_op("movz        x15, 40016");
    assert_preg_op("add         x15, sp, x15");
    assert_preg_op("str         x14, [x15]");       // Store into spilled temp register
    assert_preg_op("movz        x14, 40016");
    assert_preg_op("add         x14, sp, x14");
    assert_preg_op("ldr         x14, [x14]");       // Reload into spilled temp register
    assert_preg_op("mov         x14, x14");         // Load/store join
    assert_preg_op("movz        x15, 40008");
    assert_preg_op("add         x15, sp, x15");
    assert_preg_op("str         x14, [x15]");       // Spill store

    test_inflate_stack_size = 0; // Restore artificial stack size inflator

    init_allocate_registers(); // Enable register allocation again
}

void test_stack_stores(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_MOVE s << v
    start_ir();
    i(0, IR_MOVE, Ssz(-1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("strb        w0, [sp, 15]");

    start_ir();
    i(0, IR_MOVE, Ssz(-1, TYPE_SHORT), vsz(3, TYPE_SHORT), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("strh        w0, [sp, 14]");

    start_ir();
    i(0, IR_MOVE, Ssz(-1, TYPE_INT), vsz(3, TYPE_INT), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("str         w0, [sp, 12]");

    start_ir();
    i(0, IR_MOVE, Ssz(-1, TYPE_LONG), vsz(3, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("str         x0, [sp, 8]");

    // IR_MOVE s << v with a small offset
    start_ir();
    tac = i(0, IR_MOVE, Ssz(-1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    tac->dst->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("strb        w0, [sp, 23]");

    // IR_MOVE s << v with a large offset
    start_ir();
    tac = i(0, IR_MOVE, Ssz(-1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    tac->dst->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("movz        x17, 5015");
    assert_preg_op("add         x17, sp, x17");
    assert_preg_op("strb        w0, [x17]");

    live_range_reserved_pregs_offset = 0; // Disable register allocation

    // IR_MOVE s << v with everything spilled
    start_ir();
    i(0, IR_MOVE, Ssz(-1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrb        w15, [sp, 14]");
    assert_preg_op("strb        w15, [sp, 15]");

    // IR_MOVE s << v with a small offset with a spilled value
    start_ir();
    tac = i(0, IR_MOVE, Ssz(-1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    tac->dst->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrb        w15, [sp, 14]");
    assert_preg_op("strb        w15, [sp, 23]");

    // IR_MOVE s << v with a large offset with a spilled value
    // x16 is used to hold the large constant
    start_ir();
    tac = i(0, IR_MOVE, Ssz(-1, TYPE_CHAR), vsz(3, TYPE_CHAR), 0);
    tac->dst->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrb        w15, [sp, 14]");
    assert_preg_op("movz        x17, 5015");
    assert_preg_op("add         x17, sp, x17");
    assert_preg_op("strb        w15, [x17]");

    // IR_MOVE s << v with no offset with a spilled value and a stack slot with a small offset.
    test_inflate_stack_size = 32;
    start_ir();
    i(0, IR_MOVE, Ssz(-1, TYPE_LONG), vsz(2, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         x15, [sp, 32]");        // Spill load
    assert_preg_op("str         x15, [sp, 40]");        // Spill store

    // IR_MOVE s << v with no offset with a spilled value and a stack slot with a large offset.
    test_inflate_stack_size = 40000;
    start_ir();
    i(0, IR_MOVE, Ssz(-1, TYPE_LONG), vsz(2, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("movz        x15, 40000");       // r2 is spilled to [sp, 40000]
    assert_preg_op("add         x15, sp, x15");
    assert_preg_op("ldr         x15, [x15]");       // Spill load
    assert_preg_op("movz        x17, 40008");       // r1 is spilled to [sp, 40008]
    assert_preg_op("add         x17, sp, x17");
    assert_preg_op("str         x15, [x17]");       // Spill store

    test_inflate_stack_size = 0; // Restore artificial stack size inflator

    init_allocate_registers(); // Enable register allocation again
}

void test_stack_address_of() {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_ADDRESS_OF v = &s
    start_ir();
    i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("add         x0, sp, 15");

    // IR_ADDRESS_OF two v = &s, both long, both together take up the entire stack frame,
    // so that the second one is at the stack pointer.
    start_ir();
    i(0, IR_ADDRESS_OF, asz(2, TYPE_LONG), Ssz(-1, TYPE_LONG), 0);
    i(0, IR_ADDRESS_OF, asz(2, TYPE_LONG), Ssz(-2, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("add         x0, sp, 8");
    assert_preg_op("mov         x0, sp");

    // IR_ADDRESS_OF v = &s with a small offset
    start_ir();
    tac = i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    tac->src1->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("add         x0, sp, 23");

    // IR_ADDRESS_OF v = &s with a large offset
    start_ir();
    tac = i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("movz        x16, 5015");
    assert_preg_op("add         x0, sp, x16");

    live_range_reserved_pregs_offset = 0; // Disable register allocation

    // IR_ADDRESS_OF v = &s with everything spilled
    start_ir();
    i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("add         x14, sp, 7");
    assert_preg_op("str         x14, [sp, 8]");

    // IR_ADDRESS_OF v = &s with a small offset and everything spilled
    start_ir();
    tac = i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    tac->src1->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("add         x14, sp, 15");
    assert_preg_op("str         x14, [sp, 8]");

    // IR_ADDRESS_OF v = &s with a large offset and everything spilled
    start_ir();
    tac = i(0, IR_ADDRESS_OF, asz(2, TYPE_CHAR), Ssz(-1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("movz        x16, 5007");
    assert_preg_op("add         x14, sp, x16");
    assert_preg_op("str         x14, [sp, 8]");

    init_allocate_registers(); // Enable register allocation again
}

void test_indirects(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    start_ir();
    i(0, IR_INDIRECT, vsz(2, TYPE_CHAR), asz(1, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrsb       w0, [x0]");

    start_ir();
    i(0, IR_INDIRECT, vsz(2, TYPE_SHORT), asz(1, TYPE_SHORT), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrsh       w0, [x0]");

    start_ir();
    i(0, IR_INDIRECT, vsz(2, TYPE_INT), asz(1, TYPE_INT), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         w0, [x0]");

    start_ir();
    i(0, IR_INDIRECT, vsz(2, TYPE_LONG), asz(1, TYPE_LONG), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         x0, [x0]");

    // With a small offset
    start_ir();
    tac = i(0, IR_INDIRECT, vsz(2, TYPE_CHAR), asz(1, TYPE_CHAR), 0);
    tac->src1->offset = 8;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldrsb       w0, [x0, 8]");

    // With a large offset
    start_ir();
    tac = i(0, IR_INDIRECT, vsz(2, TYPE_CHAR), asz(1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("movz        x16, 5000");
    assert_preg_op("ldrb        w0, [x0, x16]");

    live_range_reserved_pregs_offset = 0; // Disable register allocation

    // With everything spilled
    start_ir();
    i(0, IR_INDIRECT, vsz(2, TYPE_CHAR), asz(1, TYPE_CHAR), 0);
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         x14, [sp, 8]");
    assert_preg_op("ldrsb       w14, [x14]");
    assert_preg_op("strb        w14, [sp, 7]");

    // With a small offset and everything spilled
    start_ir();
    tac = i(0, IR_INDIRECT, vsz(2, TYPE_CHAR), asz(1, TYPE_CHAR), 0);
    tac->src1->offset = 32;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         x14, [sp, 8]");     // Load the pointer from the stack
    assert_preg_op("add         x14, x14, 32");     // Add the offset
    assert_preg_op("ldrsb       w14, [x14]");       // Load the char
    assert_preg_op("strb        w14, [sp, 7]");     // Store the result to the stack

    // With a large offset and everything spilled
    start_ir();
    tac = i(0, IR_INDIRECT, vsz(2, TYPE_CHAR), asz(1, TYPE_CHAR), 0);
    tac->src1->offset = 5000;
    finish_spill_ir_and_memory_load_stores();
    assert_preg_op("ldr         x14, [sp, 8]");     // Load the pointer from the stack
    assert_preg_op("movz        x16, 5000");
    assert_preg_op("add         x14, x14, x16");    // Add the offset
    assert_preg_op("ldrsb       w14, [x14]");       // Load the char
    assert_preg_op("strb        w14, [sp, 7]");     // Store the result to the stack

    init_allocate_registers(); // Enable register allocation again
}

void test_instrsel_constant_bitshift(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_BSHL v << c
    si(function, 0, IR_BSHL, vsz(3, TYPE_CHAR), vsz(1, TYPE_CHAR), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_INT), vsz(1, TYPE_INT), c(1));
    assert_target_op("lsl         r2w, r1w, 1");

    si(function, 0, IR_BSHL, vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(1));
    assert_target_op("lsl         r2x, r1x, 1");

    // IR_ASHR v << c
    si(function, 0, IR_ASHR, vsz(3, TYPE_CHAR), vsz(1, TYPE_CHAR), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_INT), vsz(1, TYPE_INT), c(1));
    assert_target_op("asr         r2w, r1w, 1");

    si(function, 0, IR_ASHR, vsz(3, TYPE_LONG), vsz(1, TYPE_LONG), c(1));
    assert_target_op("asr         r2x, r1x, 1");

    // IR_BSHR v << c
    si(function, 0, IR_BSHR, vusz(3, TYPE_CHAR), vusz(1, TYPE_CHAR), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_INT), vusz(1, TYPE_INT), c(1));
    assert_target_op("lsr         r2w, r1w, 1");

    si(function, 0, IR_BSHR, vusz(3, TYPE_LONG), vusz(1, TYPE_LONG), c(1));
    assert_target_op("lsr         r2x, r1x, 1");
}

// Immediates in add/sub operations can be encoded in the instruction if.
// the constant is 12-bit immediate, optionally shifted left by 12 bits.
void test_instrsel_add_sub_constant(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // IR_ADD v1 + v1 + c
    si(function, 0, IR_ADD, vsz(3, TYPE_CHAR),   vsz(1, TYPE_CHAR),   ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_SHORT),  vsz(1, TYPE_SHORT),  ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_INT),    vsz(1, TYPE_INT),    ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vsz(3, TYPE_LONG),   vsz(1, TYPE_LONG),   ci(1)); assert_target_op("add         r2x, r1x, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   ci(1)); assert_target_op("add         r2w, r1w, 1");
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(1)); assert_target_op("add         r2x, r1x, 1");

    // IR_SUB v1 + v1 + c
    si(function, 0, IR_SUB, vsz(3, TYPE_CHAR),   vsz(1, TYPE_CHAR),   ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_SHORT),  vsz(1, TYPE_SHORT),  ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_INT),    vsz(1, TYPE_INT),    ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vsz(3, TYPE_LONG),   vsz(1, TYPE_LONG),   ci(1)); assert_target_op("sub         r2x, r1x, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   ci(1)); assert_target_op("sub         r2w, r1w, 1");
    si(function, 0, IR_SUB, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(1)); assert_target_op("sub         r2x, r1x, 1");

    // Check encodable constants
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(4095));        assert_target_op("add         r2x, r1x, 4095");     // 0xfff
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(4096));        assert_target_op("add         r2x, r1x, 4096");     // 0x001 << 12
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(0x00800000));  assert_target_op("add         r2x, r1x, 8388608");  // 0x800 << 12
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  ci(0x00fff000));  assert_target_op("add         r2x, r1x, 16773120"); // 0xfff << 12

    // Not encodable constant, it has to get loaded in a register
    si(function, 0, IR_ADD, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(0x00ffffff));
    assert_long(AARCH64_OP_MOV_INT_CST, ir_start->operation.id);
    ir_start = ir_start->next;
    assert_target_op("add         r2x, r1x, r3x");
}

#define CHECK_32_BIT_LOGICAL_IMMEDIATE(expected_result, value) assert_int(expected_result, is_logical_immediate(value, 1), # value);
#define CHECK_64_BIT_LOGICAL_IMMEDIATE(expected_result, value) assert_int(expected_result, is_logical_immediate(value, 0), # value);

int test_is_logical_immediate_32_bit() {
    // All zeros or all ones
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000000U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0xffffffffU);

    // Simple patterns
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00000001U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00000003U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x000000ffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00000fffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00ffffffU);

    // High bit set
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x80000000U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xf0000000U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xffff0000U);

    // Repeated 2 bit
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x55555555U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xaaaaaaaaU);

    // Repeated 4 bit
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x11111111U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x33333333U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x0f0f0f0fU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xf0f0f0f0U);

    // Repeated 8 bit
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x1f1f1f1fU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xf8f8f8f8U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xff00ff00U);

    // Repeated 16 bit
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00ff00ffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xff0000ffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x0fff0fffU);

    // Wrapped
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xc0000001U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xe0000003U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xf000000fU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x80000001U);

    // Common masks
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xffff0000U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x00ffffffU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0xffffff00U);

    // I think not
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000123U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00001234U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0xdeadbeefU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x13579bdfU);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x12345678U);

    // Non contiguous
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000005U); // 0101
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000009U); // 1001
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x00000015U); // 10101

    // Tricky
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x81818181U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x01010101U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x03030303U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(1, 0x7f7f7f7fU);

    // Failures
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x01020304U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x80402010U);
    CHECK_32_BIT_LOGICAL_IMMEDIATE(0, 0x11223344U);
}

int test_is_logical_immediate_64_bit() {
    // All zeros or all ones
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0xffffffffffffffffULL);

    // Simple runs of ones
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000000000000001ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000000000000003ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x000000000000000fULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00000000000000ffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000000000000fffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00000000ffffffffULL);

    // High-bit runs
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x8000000000000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xf000000000000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xffff000000000000ULL);

    // // Sepeated 2-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x5555555555555555ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xaaaaaaaaaaaaaaaaULL);

    // // Repeated 4-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x1111111111111111ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x3333333333333333ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0f0f0f0f0f0f0f0fULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xf0f0f0f0f0f0f0f0ULL);

    // // Repeated 8-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x1f1f1f1f1f1f1f1fULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xf8f8f8f8f8f8f8f8ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xff00ff00ff00ff00ULL);

    // // Repeated 16-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00ff00ff00ff00ffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xff0000ffff0000ffULL);

    // // Repeated 32-bit patterns
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffff0000ffffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xffff0000ffff0000ULL);

    // // Full 64-bit masks
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00000000ffffffffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xffffffff00000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffffffff0000ULL);

    // // Wrapped runs (rotation required)
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xc000000000000001ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xe000000000000003ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0xf00000000000000fULL);

    // Failures
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000123ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000001234ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x00000000deadbeefULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0123456789abcdefULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x13579bdf2468ace0ULL);

    // Almost right but not contiguous
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000005ULL); // 0101
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000009ULL); // 1001
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x0000000000000015ULL); // 10101

    // Random sparse masks
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0001000100010001ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(0, 0x8001000180010001ULL);

    // Common compiler-generated masks
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x000000000000ffffULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00000000ffff0000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x0000ffff00000000ULL);
    CHECK_64_BIT_LOGICAL_IMMEDIATE(1, 0x00ffffff00000000ULL);
}

void test_instrsel_logical_instruction_with_constant(void) {
    Tac *tac;

    remove_reserved_physical_registers = 1;

    // Test case where a constant cannot be encoded and must be moved into a register first
    si(function, 0, IR_BOR, vsz(3, TYPE_INT), vsz(1, TYPE_INT), ci(5));
    assert_long(AARCH64_OP_MOV_INT_CST, ir_start->operation.id);
    ir_start = ir_start->next;
    assert_target_op("orr         r2w, r1w, r3w");

    // binary or
    // signed v | c
    si(function, 0, IR_BOR, vsz(3, TYPE_CHAR),  vsz(1, TYPE_CHAR),  c(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vsz(3, TYPE_INT),   vsz(1, TYPE_INT),   c(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vsz(3, TYPE_LONG),  vsz(1, TYPE_LONG),  c(15)); assert_target_op("orr         r2x, r1x, 15");

    // unsigned v | c
    si(function, 0, IR_BOR, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  uc(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), uc(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   uc(15)); assert_target_op("orr         r2w, r1w, 15");
    si(function, 0, IR_BOR, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(15)); assert_target_op("orr         r2x, r1x, 15");

    // binary and
    // signed v & c
    si(function, 0, IR_BAND, vsz(3, TYPE_CHAR),  vsz(1, TYPE_CHAR),  c(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vsz(3, TYPE_INT),   vsz(1, TYPE_INT),   c(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vsz(3, TYPE_LONG),  vsz(1, TYPE_LONG),  c(15)); assert_target_op("and         r2x, r1x, 15");

    // unsigned v & c
    si(function, 0, IR_BAND, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  uc(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), uc(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   uc(15)); assert_target_op("and         r2w, r1w, 15");
    si(function, 0, IR_BAND, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(15)); assert_target_op("and         r2x, r1x, 15");

    // exclusive or
    // signed v ^ c
    si(function, 0, IR_XOR, vsz(3, TYPE_CHAR),  vsz(1, TYPE_CHAR),  c(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vsz(3, TYPE_SHORT), vsz(1, TYPE_SHORT), c(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vsz(3, TYPE_INT),   vsz(1, TYPE_INT),   c(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vsz(3, TYPE_LONG),  vsz(1, TYPE_LONG),  c(15)); assert_target_op("eor         r2x, r1x, 15");

    // unsigned v ^ c
    si(function, 0, IR_XOR, vusz(3, TYPE_CHAR),  vusz(1, TYPE_CHAR),  uc(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vusz(3, TYPE_SHORT), vusz(1, TYPE_SHORT), uc(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vusz(3, TYPE_INT),   vusz(1, TYPE_INT),   uc(15)); assert_target_op("eor         r2w, r1w, 15");
    si(function, 0, IR_XOR, vusz(3, TYPE_LONG),  vusz(1, TYPE_LONG),  uc(15)); assert_target_op("eor         r2x, r1x, 15");
}

// Test storing an integer constant to a long in the stack.
// This ensures the rules are setup such that the constant isn't first loaded in a 32-bit register,
// then sign extended to 64-bit, then stored.
void test_constant_store_to_stack(void) {
    si(function, 0, IR_MOVE, Ssz(-1, TYPE_LONG), ci(1), 0);
    assert_long(AARCH64_OP_MOV_INT_CST, ir_start->operation.id);
    assert_long(TYPE_INT, ir_start->src1->type->type); // It starts of as an 32-bit integer ...
    assert_long(3, ir_start->src1->target_size);
    assert_long(4, ir_start->dst->target_size);  // ... and becomes 64-bit
    ir_start = ir_start->next;
    assert_target_op("str         r1x, [sp, 8]");
}

void test_int_cmp_with_conditional_jmp(Function *function, int cmp_operation, int jmp_operation, char *jmp_instruction) {
    char *template;

    start_ir();
    i(0, cmp_operation,  v(3), v(1), v(2));
    i(0, jmp_operation,  0,    v(3), l(1));
    i(1, IR_NOP,         0,    0,    0   );
    finish_ir(function);
    wasprintf(&template, "%-11s .L1", jmp_instruction);
    assert_target_op("cmp         r1x, r2x");
    assert_target_op(template);
}

void test_cmp_with_assignment(Function *function, int cmp_operation, char *set_instruction) {
    char *template;

    start_ir();
    i(0, cmp_operation, vsz(3, TYPE_INT), v(1), v(2));
    finish_ir(function);
    assert_target_op("cmp         r1x, r2x");
    assert_target_op(set_instruction);
}

void test_instrsel_conditionals() {
    long l;

    l = 4294967296;
    // JZ                                                              JNZ
    test_int_cmp_with_conditional_jmp(function, IR_EQ, IR_JNZ, "beq"); test_int_cmp_with_conditional_jmp(function, IR_EQ, IR_JZ, "bne");
    test_int_cmp_with_conditional_jmp(function, IR_NE, IR_JNZ, "bne"); test_int_cmp_with_conditional_jmp(function, IR_NE, IR_JZ, "beq");
    test_int_cmp_with_conditional_jmp(function, IR_LT, IR_JNZ, "blt"); test_int_cmp_with_conditional_jmp(function, IR_LT, IR_JZ, "bge");
    test_int_cmp_with_conditional_jmp(function, IR_GT, IR_JNZ, "bgt"); test_int_cmp_with_conditional_jmp(function, IR_GT, IR_JZ, "ble");
    test_int_cmp_with_conditional_jmp(function, IR_LE, IR_JNZ, "ble"); test_int_cmp_with_conditional_jmp(function, IR_LE, IR_JZ, "bgt");
    test_int_cmp_with_conditional_jmp(function, IR_GE, IR_JNZ, "bge"); test_int_cmp_with_conditional_jmp(function, IR_GE, IR_JZ, "blt");

    // TODO aarch64
    // test_fp_cmp_with_conditional_jmp(function, IR_LT, IR_JNZ, "ja" ); test_fp_cmp_with_conditional_jmp(function, IR_LT, IR_JZ, "jbe");
    // test_fp_cmp_with_conditional_jmp(function, IR_GT, IR_JNZ, "ja" ); test_fp_cmp_with_conditional_jmp(function, IR_GT, IR_JZ, "jbe");
    // test_fp_cmp_with_conditional_jmp(function, IR_LE, IR_JNZ, "jae"); test_fp_cmp_with_conditional_jmp(function, IR_LE, IR_JZ, "jb" );
    // test_fp_cmp_with_conditional_jmp(function, IR_GE, IR_JNZ, "jae"); test_fp_cmp_with_conditional_jmp(function, IR_GE, IR_JZ, "jb" );

    // Conditional assignment with 2 registers
    test_cmp_with_assignment(function, IR_EQ, "cset        r3w, eq");
    test_cmp_with_assignment(function, IR_NE, "cset        r3w, ne");
    test_cmp_with_assignment(function, IR_LT, "cset        r3w, lt");
    test_cmp_with_assignment(function, IR_GT, "cset        r3w, gt");
    test_cmp_with_assignment(function, IR_LE, "cset        r3w, le");
    test_cmp_with_assignment(function, IR_GE, "cset        r3w, ge");
}

void test_spilling() {
    Tac *tac;

    remove_reserved_physical_registers = 1;
    live_range_reserved_pregs_offset = 0; // Disable register allocation

    // src1c spill
    start_ir();
    i(0, IR_MOVE, Ssz(-2, TYPE_CHAR), vsz(1, TYPE_CHAR), 0);
    finish_spill_ir(function);
    assert_preg_op("ldrb        w15, [sp, 14]");
    assert_preg_op("strb        w15, [sp, 15]");

    // src1s spill
    start_ir();
    i(0, IR_MOVE, Ssz(-2, TYPE_SHORT), vsz(1, TYPE_SHORT), 0);
    finish_spill_ir(function);
    assert_preg_op("ldrh        w15, [sp, 12]");
    assert_preg_op("strh        w15, [sp, 14]");

    // src1i spill
    start_ir();
    i(0, IR_MOVE, Ssz(-2, TYPE_INT), vsz(1, TYPE_INT), 0);
    finish_spill_ir(function);
    assert_preg_op("ldr         w15, [sp, 8]");

    // src1q spill
    start_ir();
    i(0, IR_MOVE, S(-2), v(1), 0);
    finish_spill_ir(function);
    assert_preg_op("ldr         x15, [sp]");

    // src1c spill TODO aarch64 offsets in a spilled vreg

    // dst, src1 and src2 spill
    start_ir();
    i(0, IR_EQ, vsz(3, TYPE_INT), v(1), v(2));
    finish_spill_ir(function);
    assert_preg_op("ldr         x14, [sp, 24]");    // x14 is used for loading src1
    assert_preg_op("ldr         x15, [sp, 16]");    // x15 is used for loading src1
    assert_preg_op("cmp         x14, x15");         // x14 is used to store the result
    assert_preg_op("cset        w14, eq");
    assert_preg_op("str         w14, [sp, 12]");    // Store the result

    init_allocate_registers(); // Enable register allocation again
}

// This tests the saving and loading of callee saved registers.
// int and FP registers are loaded/saved in separate blocks since they
// can't be loaded/saved together with a pair load (ldp).
void test_saved_registers() {
    Tac *tac;

    init_codegen();

    remove_reserved_physical_registers = 1;

    callee_saved_registers[REG_R00] = 1;
    callee_saved_registers[REG_V00] = 1;

    // With x0
    start_ir();
    i(0, IR_MOVE, v(2), c(1), 0);
    finish_spill_ir(function);
    add_final_instructions(function);
    ir_start = function->ir;
    assert_preg_op("str         x0, [sp, #-16]!");
    assert_preg_op("movz        x0, 1");
    assert_preg_op("mov         x0, x0");
    assert_preg_op("ldr         x0, [sp], #16");
    assert_preg_op("ret");

    // With d0
    start_ir();
    i(0, IR_MOVE, d(1), Ssz(-1, TYPE_DOUBLE), 0);
    finish_spill_ir(function);
    init_codegen();
    add_final_instructions(function);
    ir_start = function->ir;
    assert_preg_op("str         d0, [sp, #-16]!");
    assert_preg_op("sub         sp, sp, 16");
    assert_preg_op("ldr         d0, [sp, 8]");
    assert_preg_op("fmov        d0, d0");
    assert_preg_op("add         sp, sp, 16");
    assert_preg_op("ldr         d0, [sp], #16");
    assert_preg_op("ret");

    // With both x0 and d0
    start_ir();
    i(0, IR_MOVE, v(1), c(1), 0);
    i(0, IR_MOVE, d(1), cd(1), 0);
    finish_spill_ir(function);
    init_codegen();
    add_final_instructions(function);
    ir_start = function->ir;
    assert_preg_op("str         x0, [sp, #-16]!");
    assert_preg_op("str         d0, [sp, #-16]!");
    assert_preg_op("movz        x0, 1");
    assert_preg_op("mov         x0, x0");
    assert_preg_op("adrp        x0, .LFP0");
    assert_preg_op("add         x0, x0, :lo12:.LFP0");
    assert_preg_op("ldr         d0, [x0]");
    assert_preg_op("fmov        d0, d0");
    assert_preg_op("ldr         d0, [sp], #16");
    assert_preg_op("ldr         x0, [sp], #16");
    assert_preg_op("ret");

    init_allocate_registers(); // Enable register allocation again
}

int main() {
    int verbose;

    verbose = 0;
    failures = 0;

    init_memory_management_for_translation_unit();

    function = new_function("test");
    function->type = new_type(TYPE_FUNCTION);
    function->type->function = wcalloc(1, sizeof(FunctionType));
    function->type->target = new_type(TYPE_INT);

    opt_optimize_arithmetic_operations = 1;
    string_literals = malloc(MAX_STRING_LITERALS);

    init_allocate_registers();
    init_instruction_selection_rules();

    if (verbose) printf("Running instrsel test_global_loads\n");                                test_global_loads();
    if (verbose) printf("Running instrsel test_global_stores\n");                               test_global_stores();
    if (verbose) printf("Running instrsel test_global_address_of\n");                           test_global_address_of();
    if (verbose) printf("Running instrsel test_stack_loads\n");                                 test_stack_loads();
    if (verbose) printf("Running instrsel test_stack_stores\n");                                test_stack_stores();
    if (verbose) printf("Running instrsel test_stack_address_of\n");                            test_stack_address_of();
    if (verbose) printf("Running instrsel test_indirects\n");                                   test_indirects();
    if (verbose) printf("Running instrsel test_instrsel_constant_bitshift\n");                  test_instrsel_constant_bitshift();
    if (verbose) printf("Running instrsel test_instrsel_add_sub_constant\n");                   test_instrsel_add_sub_constant();
    if (verbose) printf("Running instrsel test_is_logical_immediate_32_bit\n");                 test_is_logical_immediate_32_bit();
    if (verbose) printf("Running instrsel test_is_logical_immediate_64_bit\n");                 test_is_logical_immediate_64_bit();
    if (verbose) printf("Running instrsel test_instrsel_logical_instruction_with_constant\n");  test_instrsel_logical_instruction_with_constant();
    if (verbose) printf("Running instrsel test_constant_store_to_stack\n");                     test_constant_store_to_stack();
    if (verbose) printf("Running instrsel test_instrsel_conditionals\n");                       test_instrsel_conditionals();
    if (verbose) printf("Running instrsel test_spilling\n");                                    test_spilling();
    if (verbose) printf("Running instrsel test_saved_registers\n");                             test_saved_registers();

    if (failures) {
        printf("%d tests failed\n", failures);
        exit(1);
    }
    printf("All tests succeeded\n");
}
