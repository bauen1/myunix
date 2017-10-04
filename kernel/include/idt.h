#ifndef IDT_H
#define IDT_H 1

#include <stdint.h>

struct idt_entry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t zero;
	uint8_t type_attr;
	uint16_t offset_high;
} __attribute__((packed));

void idt_init();

#include <isrs.h>

#endif
