#include <stddef.h>
#include <stdint.h>

#include <serial.h>

#include <cpu.h>
#include <isr.h>
#include <pic.h>

#define PORT 0x3F8

static void *irq4_handler(registers_t *regs) {
	serial_putc(inb(PORT));
	return regs;
}

static int serial_is_transmit_ready() {
	return inb(PORT + 5) & 0x20;
}

static int serial_is_read_ready() {
	return inb(PORT + 5) & 0x01;
}

void serial_init() {
	isr_set_handler(isr_from_irq(4), irq4_handler);

	// init serial COM0
	outb(PORT + 1, 0x00); // Disable all interrupts
	outb(PORT + 3, 0x80); // enable DLAB
	outb(PORT + 0, 0x03); // divisor low
	outb(PORT + 1, 0x00); // divisor high
	outb(PORT + 3, 0x03); // disable DLAB, 8 bits, no parity one stop
	outb(PORT + 2, 0xC7); // enable FIFO, clear them, 14 byte threshold
	outb(PORT + 4, 0x0B); // IRQs enabled
	outb(PORT + 1, 0x01); // enable data-received IRQ
//	outb(PORT + 1, 0x0F); // enable all IRQs
}

void serial_putc(char c) {
	while (serial_is_transmit_ready() == 0);
	outb(PORT, c);
}

char serial_getc() {
	while (serial_is_read_ready() == 0) {
		__asm__ __volatile__("hlt");
	};
	return inb(PORT);
}

void serial_puts(const char *str) {
	while (*str != 0) {
		serial_putc(*str++);
	}
}

