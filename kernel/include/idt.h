#ifndef IDT_H
#define IDT_H 1

#include <stdint.h>
#include <isrs.h>

struct idt_entry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t zero;
	uint8_t type_attr;
	uint16_t offset_high;
} __attribute__((packed));

void idt_init();

typedef void (*isr_handler)(registers_t *regs);

void idt_set_isr_handler(uint8_t i, isr_handler handler);

#endif
