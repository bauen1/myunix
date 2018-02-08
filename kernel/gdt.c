#include <stdint.h>
#include <gdt.h>
#include <gdt_flush.h>
#include <tss_flush.h>
#include <string.h>

// FIXME: move to arch/i686 and arch/x86_64

typedef struct tss {
	uint32_t link __attribute__((packed));
	uint32_t esp0 __attribute__((packed));
	uint32_t ss0 __attribute__((packed));
	uint32_t esp1 __attribute__((packed));
	uint32_t ss1 __attribute__((packed));
	uint32_t esp2 __attribute__((packed));
	uint32_t ss2 __attribute__((packed));
	uint32_t cr3 __attribute__((packed));
	uint32_t eip __attribute__((packed));
	uint32_t eflags __attribute__((packed));
	uint32_t eax __attribute__((packed));
	uint32_t ecx __attribute__((packed));
	uint32_t ebx __attribute__((packed));
	uint32_t esp __attribute__((packed));
	uint32_t ebp __attribute__((packed));
	uint32_t esi __attribute__((packed));
	uint32_t edi __attribute__((packed));
	uint32_t es __attribute__((packed));
	uint32_t cs __attribute__((packed));
	uint32_t ss __attribute__((packed));
	uint32_t ds __attribute__((packed));
	uint32_t fs __attribute__((packed));
	uint32_t gs __attribute__((packed));
	uint32_t ldt __attribute__((packed));
	uint16_t trap __attribute__((packed));
	uint16_t iopb_offset __attribute__((packed));
} __attribute__((packed)) tss_t;

typedef struct gdt_entry {
	uint16_t limit_low __attribute__((packed));
	uint16_t base_low __attribute__((packed));
	uint8_t base_middle __attribute__((packed));
	uint8_t access __attribute__((packed));
	uint8_t granularity __attribute__((packed));
	uint8_t base_high __attribute__((packed));
} __attribute__((packed)) gdt_entry_t;

typedef struct {
	uint16_t limit __attribute__((packed));
	uintptr_t base __attribute__((packed));
} __attribute__((packed)) gdt_pointer_t;

static gdt_pointer_t gdt_pointer;
static gdt_entry_t gdt[6];
static tss_t tss;

static void gdt_set_gate(uint8_t i, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
	gdt[i].limit_low = (uint16_t)(limit & 0xFFFF);
	gdt[i].base_low = (uint16_t)(base & 0xFFFF);
	gdt[i].base_middle = (uint8_t)((base >> 16) & 0xFF);
	gdt[i].access = access;
	gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (granularity & 0xF0));
	gdt[i].base_high = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_init() {
	memset(&tss, 0, sizeof(tss));

	uint32_t tss_base = (uint32_t)&tss;
	uint32_t tss_limit = (uint32_t)sizeof(tss);
	tss.ss0 = 0x10;
	tss.esp0 = 0x0;
	tss.iopb_offset = sizeof(tss);
	tss.cs = 0x0b;
	tss.ss = 0x13;
	tss.ds = 0x13;
	tss.es = 0x13;
	tss.fs = 0x13;
	tss.gs = 0x13;

	gdt_set_gate(0, 0, 0x00000000, 0x00, 0x00); /* NULL segment */
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xC0); /* kernel segment */
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xC0); /* kernel segment */
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xF8, 0xC0); /* user segment */
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xC0); /* user segment */
	gdt_set_gate(5, tss_base, tss_limit, 0x89, 0x40); /* tss segment */

	gdt_pointer.limit = sizeof(gdt) - 1;
	gdt_pointer.base = (uintptr_t)&gdt;
	gdt_flush((uintptr_t)&gdt_pointer);
	tss_flush();
}

void tss_set_kstack(uintptr_t stack) {
	tss.esp0 = stack;
}
