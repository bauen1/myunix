#include <idt.h>
#include <idt_load.h>
#include <stdint.h>
#include <tty.h>

static struct idt_entry idt_entries[256] = { 0 };

static void idt_set_gate(uint8_t i, void * isr, uint16_t selector, uint8_t flags) {
	uint32_t offset = (uint32_t)isr;
	idt_entries[i].offset_low = (uint16_t)((offset)&0xFFFF);
	idt_entries[i].selector = selector;
	idt_entries[i].zero = 0;
	idt_entries[i].type_attr = flags;
	idt_entries[i].offset_high = (uint16_t)((offset >> 16) & 0xFFFF);
}

static struct {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed)) idt_p;

static isr_handler isr_handlers[256];

void idt_init() {
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

	// interrupts
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
	idt_set_gate(0x80,_isr128,0x08,0x8E);

	idt_p.limit = sizeof(idt_entries) - 1;
	idt_p.base = (uint32_t)&idt_entries;
	idt_load((uintptr_t)&idt_p);
}


void idt_set_isr_handler(uint8_t i, isr_handler handler) {
	isr_handlers[i] = handler;
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
	"bad tss",
	"segment not present",
	"stack fault",
	"general protection fault",
	"page fault",
	"unknown interrupt exception",
	"coprocessor fault",
	"alignment check exception",
	"machine check exception",
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
	"reserved",
	"reserved",
};

#include <cpu.h>
#include <pic.h>

void * handle_isr(registers_t *regs) {
	if (isr_handlers[regs->isr_num]) {
		isr_handlers[regs->isr_num](regs);
	}
	if (regs->isr_num < 32) {
		tty_puts("\nEncountered: '");
		tty_puts(exception_name[regs->isr_num]);
		tty_puts("\n");
		tty_puts("Halting the CPU\n");
		halt();
	} else {
		/* TODO: check for spurious IRQ */
		pic_send_eoi(irq_from_isr(regs->isr_num));
	}

	/* return a pointer to the new stack */
	return regs;
}
