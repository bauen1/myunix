#ifndef IDT_H
#define IDT_H 1

#include <stdint.h>

struct idt_entry {
	uint16_t offset_low __attribute__((packed));
	uint16_t selector __attribute__((packed));
	uint8_t zero __attribute__((packed));
	uint8_t type_attr __attribute__((packed));
	uint16_t offset_high __attribute__((packed));
} __attribute__((packed));

void idt_install();

void idt_set_gate(uint8_t i, void * isr, uint16_t selector, uint8_t flags);

#endif
