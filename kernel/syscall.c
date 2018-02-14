#include <cpu.h>
#include <isr.h>
#include <console.h>

static void *syscall_putc(registers_t *regs) {
	putc((char)regs->ebx);
	return regs;
}

static void *syscall_getc(registers_t *regs) {
	__asm__ __volatile__ ("sti");
	regs->ebx = (uint32_t)getc();
	__asm__ __volatile__ ("cli");
	return regs;
}

static void *syscall_handler(registers_t *regs) {
	switch (regs->eax) {
		case 0x00:
			return syscall_putc(regs);
		case 0x01:
			return syscall_getc(regs);
		default:
			return regs;
	}
	return regs;
}

void syscall_init() {
	isr_set_handler(0x80, syscall_handler);
}
