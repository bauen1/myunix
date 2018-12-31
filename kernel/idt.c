#include <stdint.h>

#include <idt.h>
#include <idt_load.h>
#include <string.h>

__attribute__((section("user_shared"))) __attribute__((aligned(4096))) struct idt_entry idt_entries[256] = { 0 };

void idt_set_gate(uint8_t i, void * isr, uint16_t selector, uint8_t flags) {
	uint32_t offset = (uint32_t)isr;
	idt_entries[i].offset_low = (uint16_t)((offset)&0xFFFF);
	idt_entries[i].selector = selector;
	idt_entries[i].zero = 0;
	idt_entries[i].type_attr = flags;
	idt_entries[i].offset_high = (uint16_t)((offset >> 16) & 0xFFFF);
}

static struct {
	uint16_t limit __attribute__((packed));
	uint32_t base __attribute__((packed));
} __attribute__((packed)) idt_p;

void idt_install(void) {
	idt_p.limit = sizeof(idt_entries) - 1;
	idt_p.base = (uint32_t)&idt_entries;
	memset(&idt_entries, 0, sizeof(idt_entries));

	idt_load((uintptr_t)&idt_p);
}
