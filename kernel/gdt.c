#include <gdt.h>
#include <gdt_flush.h>
#include <stdint.h>
#include <tss_flush.h>

// FIXME: move to arch/i686 and arch/x86_64
// TODO: tss

typedef struct gdt_entry {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
	uint16_t limit;
	uintptr_t base;
} __attribute__((packed)) gdt_pointer_t;

static gdt_pointer_t gdt_pointer;
static gdt_entry_t gdt[6];

static void gdt_set_gate(uint8_t i, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
	gdt[i].limit_low = (uint16_t)(limit & 0xFFFF);
	gdt[i].base_low = (uint16_t)(base & 0xFFFF);
	gdt[i].base_middle = (uint8_t)((base >> 16) & 0xFF);
	gdt[i].access = access;
	gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (granularity & 0xF0));
	gdt[i].base_high = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_init() {
	gdt_set_gate(0, 0, 0x00000000, 0x00, 0x00); /* NULL segment */
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xC0); /* kernel segment */
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xC0); /* kernel segment */
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xF8, 0xC0); /* user segment */
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xC0); /* user segment */
	gdt_set_gate(0, 0, 0x00000000, 0x00, 0x00); /* tss segment */

	gdt_pointer.limit = sizeof(gdt) - 1;
	gdt_pointer.base = (uintptr_t)&gdt;
	gdt_flush((uintptr_t)&gdt_pointer);
}
