#include <idt.h>
#include <idt_load.h>
#include <stdint.h>

static struct idt_entry idt_entries[256];

static void idt_set_gate(uint8_t i, void * isr, uint16_t selector, uint8_t flags) {
	uint32_t offset = (uint32_t)isr;
	idt_entries[i].offset_low = (uint16_t)((offset)&0xFFFF);
	idt_entries[i].selector = selector;
	idt_entries[i].zero = 0;
	idt_entries[i].type_attr = flags;
	idt_entries[i].offset_high = (uint16_t)((offset >> 16) & 0xFFFF);
}

static struct {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed)) idt_p;

void idt_init() {
	idt_set_gate( 0, isr0 , 0x08, 0x8E);

	idt_p.limit = sizeof(idt_entries) - 1;
	idt_p.base = (uint32_t)&idt_entries;
	idt_load((uintptr_t)&idt_p);
}

#include <tty.h>
#include <cpu.h>

void isr_handler(registers_t *registers) {
	tty_puts("encountered interrupt, halting the cpu");
	halt();
}
