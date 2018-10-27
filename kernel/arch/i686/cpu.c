#include <stdint.h>
#include <cpu.h>

#include <console.h>

inline uint8_t inb(uint16_t port) {
	uint8_t data;
	__asm__ __volatile__("inb %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline uint16_t inw(uint16_t port) {
	uint16_t data;
	__asm__ __volatile__("inw %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline uint32_t inl(uint16_t port) {
	uint32_t data;
	__asm__ __volatile__("inl %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline void outb(uint16_t port, uint8_t data) {
	__asm__ __volatile__("outb %0, %1" : : "a" (data), "dN" (port));
}

inline void outw(uint16_t port, uint16_t data) {
	__asm__ __volatile__("outw %0, %1" : : "a" (data), "dN" (port));
}

inline void outl(uint16_t port, uint32_t data) {
	__asm__ __volatile__("outl %0, %1" : : "a" (data), "dN" (port));
}


inline void iowait() {
	__asm__ __volatile__("outb %0, $0x80" : : "a"(0));
}

inline __attribute__((noreturn)) void halt() {
	puts("halting the cpu\n");
	_halt();
}

void dump_regs(registers_t *regs) {
	printf("===================================================================================================================\n");
	printf("es:      0x%8x  fs: 0x%8x  es: 0x%8x  ds: 0x%8x  cs: 0x%8x  ss: 0x%8x\n", regs->gs, regs->fs, regs->es, regs->ds, regs->cs, regs->ss);
	printf("edi:     0x%8x esi: 0x%8x ebp: 0x%8x ebx: 0x%8x edx: 0x%8x ecx: 0x%8x eax: 0x%8x\n", regs->edi, regs->esi, regs->ebp, regs->ebx, regs->edx, regs->ecx, regs->eax);
	printf("isr_num: 0x%8x err_code: 0x%8x\n", regs->isr_num, regs->err_code);
	printf("eip:     0x%8x eflags: 0x%8x esp: 0x%8x\n", regs->eip, regs->eflags, regs->esp);
	printf("old_directory: 0x%8x\n", regs->old_directory);
	printf("===================================================================================================================\n");
}

void interrupts_disable(void) {
	__asm__ __volatile__("cli");
}

void interrupts_enable(void) {
	__asm__ __volatile__("sti");
}
