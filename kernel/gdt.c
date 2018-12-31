#include <stdint.h>

#include <gdt.h>
#include <gdt_flush.h>
#include <string.h>
#include <tss_flush.h>

gdt_pointer_t gdt_pointer;
__attribute__((section("user_shared"))) __attribute__((aligned(4096))) gdt_entry_t gdt[6];
__attribute__((section("user_shared"))) __attribute__((aligned(4096))) tss_t tss;

static void gdt_set_gate(uint8_t i, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
	gdt[i].limit_low = (uint16_t)(limit & 0xFFFF);
	gdt[i].base_low = (uint16_t)(base & 0xFFFF);
	gdt[i].base_middle = (uint8_t)((base >> 16) & 0xFF);
	gdt[i].access = access;
	gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (granularity & 0xF0));
	gdt[i].base_high = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_init(void) {
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
