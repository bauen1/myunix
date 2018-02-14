#include <stdint.h>
#include <cpu.h>

#include <console.h>

inline uint8_t inb(uint16_t port) {
	uint8_t data;
	__asm__ __volatile__ ("inb %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline uint16_t inw(uint16_t port) {
	uint16_t data;
	__asm__ __volatile__ ("inw %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline uint32_t inl(uint16_t port) {
	uint32_t data;
	__asm__ __volatile__ ("inl %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline void outb(uint16_t port, uint8_t data) {
	__asm__ __volatile("outb %0, %1" : : "a" (data), "dN" (port));
}

inline void outw(uint16_t port, uint16_t data) {
	__asm__ __volatile("outw %0, %1" : : "a" (data), "dN" (port));
}

inline void outl(uint16_t port, uint32_t data) {
	__asm__ __volatile("outl %0, %1" : : "a" (data), "dN" (port));
}


inline void io_wait() {
	__asm__ __volatile__("outb %0, $0x80" : : "a"(0));
}

__attribute__((noreturn))
inline void halt() {
	puts("halting the cpu\n");
	_halt();
}

void dump_regs(registers_t *regs) {
	printf("========\n");
	printf("es: 0x%x fs: 0x%x es: 0x%x ds: 0x%x cs: 0x%x ss: 0x%x\n", regs->gs, regs->fs, regs->es, regs->ds, regs->cs, regs->ss);
	printf("edi: 0x%x esi: 0x%x ebp: 0x%x esp: 0x%x ebx: 0x%x edx: 0x%x ecx: 0x%x eax: 0x%x\n", regs->edi, regs->esi, regs->ebp, regs->esp, regs->ebx, regs->edx, regs->ecx, regs->eax);
	printf("isr_num: 0x%x err_code: 0x%x\n", regs->isr_num, regs->err_code);
	printf("eip: 0x%x eflags: 0x%x usersp: 0x%x\n", regs->eip, regs->eflags, regs->usersp);
	printf("========\n");
}
