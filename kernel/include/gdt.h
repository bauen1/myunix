#ifndef GDT_H
#define GDT_H 1

#include <stddef.h>
#include <stdint.h>

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

extern gdt_pointer_t gdt_pointer;
extern gdt_entry_t gdt[6];
extern tss_t tss;

void gdt_init(void);

void tss_set_kstack(uintptr_t stack);

#endif
