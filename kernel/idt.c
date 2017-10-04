#include <idt.h>
#include <idt_load.h>
#include <stdint.h>

static struct idt_entry idt_entries[256];

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

void idt_init() {
	// exceptions
	idt_set_gate( 0, isr0 , 0x08, 0x8E);
	idt_set_gate( 1, isr1 , 0x08, 0x8E);
	idt_set_gate( 2, isr2 , 0x08, 0x8E);
	idt_set_gate( 3, isr3 , 0x08, 0x8E);
	idt_set_gate( 4, isr4 , 0x08, 0x8E);
	idt_set_gate( 5, isr5 , 0x08, 0x8E);
	idt_set_gate( 6, isr6 , 0x08, 0x8E);
	idt_set_gate( 7, isr7 , 0x08, 0x8E);
	idt_set_gate( 8, isr8 , 0x08, 0x8E);
	idt_set_gate( 9, isr9 , 0x08, 0x8E);
	idt_set_gate(10, isr10, 0x08, 0x8E);
	idt_set_gate(11, isr11, 0x08, 0x8E);
	idt_set_gate(12, isr12, 0x08, 0x8E);
	idt_set_gate(13, isr13, 0x08, 0x8E);
	idt_set_gate(14, isr14, 0x08, 0x8E);
	idt_set_gate(15, isr15, 0x08, 0x8E);
	idt_set_gate(16, isr16, 0x08, 0x8E);
	idt_set_gate(17, isr17, 0x08, 0x8E);
	idt_set_gate(18, isr18, 0x08, 0x8E);
	idt_set_gate(19, isr19, 0x08, 0x8E);
	idt_set_gate(20, isr20, 0x08, 0x8E);
	idt_set_gate(21, isr21, 0x08, 0x8E);
	idt_set_gate(22, isr22, 0x08, 0x8E);
	idt_set_gate(23, isr23, 0x08, 0x8E);
	idt_set_gate(24, isr24, 0x08, 0x8E);
	idt_set_gate(25, isr25, 0x08, 0x8E);
	idt_set_gate(26, isr26, 0x08, 0x8E);
	idt_set_gate(27, isr27, 0x08, 0x8E);
	idt_set_gate(28, isr28, 0x08, 0x8E);
	idt_set_gate(29, isr29, 0x08, 0x8E);
	idt_set_gate(30, isr30, 0x08, 0x8E);
	idt_set_gate(31, isr31, 0x08, 0x8E);

	// interrupts
	idt_set_gate(32, isr32, 0x08, 0x8E);
	idt_set_gate(33, isr33, 0x08, 0x8E);
	idt_set_gate(34, isr34, 0x08, 0x8E);
	idt_set_gate(35, isr35, 0x08, 0x8E);
	idt_set_gate(36, isr36, 0x08, 0x8E);
	idt_set_gate(37, isr37, 0x08, 0x8E);
	idt_set_gate(38, isr38, 0x08, 0x8E);
	idt_set_gate(39, isr39, 0x08, 0x8E);
	idt_set_gate(40, isr40, 0x08, 0x8E);
	idt_set_gate(41, isr41, 0x08, 0x8E);
	idt_set_gate(42, isr42, 0x08, 0x8E);
	idt_set_gate(43, isr43, 0x08, 0x8E);
	idt_set_gate(44, isr44, 0x08, 0x8E);
	idt_set_gate(45, isr45, 0x08, 0x8E);
	idt_set_gate(46, isr46, 0x08, 0x8E);
	idt_set_gate(47, isr47, 0x08, 0x8E);

	// syscall
	idt_set_gate(0x80, isr128, 0x08, 0x8E);

	idt_p.limit = sizeof(idt_entries) - 1;
	idt_p.base = (uint32_t)&idt_entries;
	idt_load((uintptr_t)&idt_p);
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
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
};

#include <tty.h>
#include <cpu.h>

void isr_handler(registers_t *regs) {
	if (regs->isr_num < 32) {
		tty_puts("\n\rEncountered: '");
		tty_puts(exception_name[regs->isr_num]);
		tty_puts("'\n\r");
	}
	tty_puts("halting the cpu");
	halt();
}
