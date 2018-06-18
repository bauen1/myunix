#include <assert.h>
#include <console.h>
#include <cpu.h>
#include <idt.h>
#include <isr.h>
#include <isrs.h>
#include <pic.h>

static isr_handler isr_handlers[256];

void isr_set_handler(uint8_t i, isr_handler handler) {
//	printf("isr 0x%x, handler: 0x%x\n", i, (uintptr_t)handler);
	// FIXME: remove or just fix the code calling this
	assert(isr_handlers[i] == NULL);
	isr_handlers[i] = handler;
}

void isr_init() {
	// exceptions
	idt_set_gate( 0, _isr0 , 0x08, 0x8E);
	idt_set_gate( 1, _isr1 , 0x08, 0x8E);
	idt_set_gate( 2, _isr2 , 0x08, 0x8E);
	idt_set_gate( 3, _isr3 , 0x08, 0x8E);
	idt_set_gate( 4, _isr4 , 0x08, 0x8E);
	idt_set_gate( 5, _isr5 , 0x08, 0x8E);
	idt_set_gate( 6, _isr6 , 0x08, 0x8E);
	idt_set_gate( 7, _isr7 , 0x08, 0x8E);
	idt_set_gate( 8, _isr8 , 0x08, 0x8E);
	idt_set_gate( 9, _isr9 , 0x08, 0x8E);
	idt_set_gate(10, _isr10, 0x08, 0x8E);
	idt_set_gate(11, _isr11, 0x08, 0x8E);
	idt_set_gate(12, _isr12, 0x08, 0x8E);
	idt_set_gate(13, _isr13, 0x08, 0x8E);
	idt_set_gate(14, _isr14, 0x08, 0x8E);
	idt_set_gate(15, _isr15, 0x08, 0x8E);
	idt_set_gate(16, _isr16, 0x08, 0x8E);
	idt_set_gate(17, _isr17, 0x08, 0x8E);
	idt_set_gate(18, _isr18, 0x08, 0x8E);
	idt_set_gate(19, _isr19, 0x08, 0x8E);
	idt_set_gate(20, _isr20, 0x08, 0x8E);
	idt_set_gate(21, _isr21, 0x08, 0x8E);
	idt_set_gate(22, _isr22, 0x08, 0x8E);
	idt_set_gate(23, _isr23, 0x08, 0x8E);
	idt_set_gate(24, _isr24, 0x08, 0x8E);
	idt_set_gate(25, _isr25, 0x08, 0x8E);
	idt_set_gate(26, _isr26, 0x08, 0x8E);
	idt_set_gate(27, _isr27, 0x08, 0x8E);
	idt_set_gate(28, _isr28, 0x08, 0x8E);
	idt_set_gate(29, _isr29, 0x08, 0x8E);
	idt_set_gate(30, _isr30, 0x08, 0x8E);
	idt_set_gate(31, _isr31, 0x08, 0x8E);

	// IRQs
	idt_set_gate(32, _isr32, 0x08, 0x8E);
	idt_set_gate(33, _isr33, 0x08, 0x8E);
	idt_set_gate(34, _isr34, 0x08, 0x8E);
	idt_set_gate(35, _isr35, 0x08, 0x8E);
	idt_set_gate(36, _isr36, 0x08, 0x8E);
	idt_set_gate(37, _isr37, 0x08, 0x8E);
	idt_set_gate(38, _isr38, 0x08, 0x8E);
	idt_set_gate(39, _isr39, 0x08, 0x8E);
	idt_set_gate(40, _isr40, 0x08, 0x8E);
	idt_set_gate(41, _isr41, 0x08, 0x8E);
	idt_set_gate(42, _isr42, 0x08, 0x8E);
	idt_set_gate(43, _isr43, 0x08, 0x8E);
	idt_set_gate(44, _isr44, 0x08, 0x8E);
	idt_set_gate(45, _isr45, 0x08, 0x8E);
	idt_set_gate(46, _isr46, 0x08, 0x8E);
	idt_set_gate(47, _isr47, 0x08, 0x8E);

	// syscall
	idt_set_gate(0x80,_isr128,0x08,0xEE);
}

static const char *exception_name[] = {
	"division by zero",
	"debug exception",
	"NMI",
	"breakpoint",
	"into detected overflow",
	"out of bounds exception",
	"invalid opcode exception",
	"no coprocessor",
	"double fault",
	"coprocessor segment overrun",
	"invalid tss",
	"segment not present",
	"stack segment fault",
	"general protection fault",
	"page fault",
	"reserved",
	"coprocessor fault",
	"alignment check exception",
	"machine check exception",
	"SIMD Floating-Point Exception",
	"Virtualization Exception",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
};

void handle_isr(registers_t *regs) {
	unsigned int isr_num = regs->isr_num;
	if (isr_handlers[isr_num]) {
		isr_handlers[isr_num](regs);
	} else if (isr_num < 32) {
		printf("Encountered unhandled exception: '%s' !\n", exception_name[isr_num]);
		dump_regs(regs);
		halt();
	} else {
		printf("Encountered unhandled interrupt: 0x%x\n", isr_num);
		if (isr_num >= 32 && isr_num <= 47) {
			printf("IRQ: %u\n", regs->err_code);
		}
		assert(0 && "unhandled interrupt!");
	}
}
