#ifndef ARCH_CPU_H
#define ARCH_CPU_H 1

#include <stdint.h>

typedef struct registers {
	uint32_t old_directory __attribute__((packed));
	uint32_t gs __attribute__((packed));
	uint32_t fs __attribute__((packed));
	uint32_t es __attribute__((packed));
	uint32_t ds __attribute__((packed));

	uint32_t edi __attribute__((packed));
	uint32_t esi __attribute__((packed));
	uint32_t ebp __attribute__((packed));
	uint32_t useless_esp __attribute__((packed));
	uint32_t ebx __attribute__((packed));
	uint32_t edx __attribute__((packed));
	uint32_t ecx __attribute__((packed));
	uint32_t eax __attribute__((packed));

	uint32_t isr_num __attribute__((packed));
	uint32_t err_code __attribute__((packed));

	uint32_t eip __attribute__((packed));
	uint32_t cs __attribute__((packed));
	uint32_t eflags __attribute__((packed));
	uint32_t esp __attribute__((packed));
	uint32_t ss __attribute__((packed));
} __attribute__((packed)) registers_t;

/* _halt is found in boot.s */
__attribute__((noreturn)) extern void _halt();
__attribute__((noreturn)) void halt();

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);
void outl(uint16_t port, uint32_t data);

void iowait();

#define hlt() __asm__ __volatile__ ("hlt")

void dump_regs(registers_t *regs);

void interrupts_disable(void);
void interrupts_enable(void);

#endif
