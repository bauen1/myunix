#ifndef CPU_H
#define CPU_H 1

#include <stdint.h>

typedef struct registers {
	uint32_t gs __attribute__((packed));
	uint32_t fs __attribute__((packed));
	uint32_t es __attribute__((packed));
	uint32_t ds __attribute__((packed));

	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax __attribute__((packed)); /* pushed by pusha */

	uint32_t isr_num __attribute__((packed));
	uint32_t err_code __attribute__((packed));

	uint32_t eip __attribute__((packed));
	uint32_t cs __attribute__((packed));
	uint32_t eflags __attribute__((packed));
	uint32_t usersp __attribute__((packed));
	uint32_t ss __attribute__((packed));
} __attribute__((packed)) registers_t;

/* _halt is found in boot.s */
__attribute__((noreturn)) extern void _halt();
#define halt() ((void)_halt())

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);
void outl(uint16_t port, uint32_t data);

void io_wait();

#define hlt() __asm__ __volatile__ ("hlt")

#endif
