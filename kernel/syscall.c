#include <console.h>
#include <cpu.h>
#include <isr.h>
#include <pit.h>
#include <process.h>

static void syscall_putc(registers_t *regs) {
	putc((char)regs->ebx);
}

static void syscall_getc(registers_t *regs) {
	__asm__ __volatile__ ("sti");
	regs->ebx = (uint32_t)getc();
	__asm__ __volatile__ ("cli");
}

static void syscall_sleep(registers_t *regs) {
	unsigned long target = ticks + regs->ebx;
	while (ticks <= target) {
		switch_task();
	}
	regs->eax = 0;
}

static void syscall_dumpregs(registers_t *regs) {
	dump_regs(regs);
}

static void syscall_handler(registers_t *regs) {
	switch (regs->eax) {
		case 0x00:
			syscall_putc(regs);
			break;
		case 0x01:
			syscall_getc(regs);
			break;
		case 0x02:
			syscall_sleep(regs);
			break;
		case 0xFF:
			syscall_dumpregs(regs);
			break;
		default:
			break;
	}
}

void syscall_init() {
	isr_set_handler(0x80, syscall_handler);
}
