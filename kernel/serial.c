#include <stddef.h>
#include <stdint.h>

#include <serial.h>

#include <cpu.h>
#include <isr.h>
#include <pic.h>

#include <ringbuffer.h>

#define SERIALBUFFER_LENGTH 1024
static ringbuffer_t serial_ringbuffer;
static unsigned char serialbuffer[SERIALBUFFER_LENGTH];

#define PORT 0x3F8

static void *irq4_handler(registers_t *regs) {
	ringbuffer_write_byte(&serial_ringbuffer, inb(PORT));
	return regs;
}

#define serial_is_transmit_ready() (inb(PORT + 5) & 0x20)
#define serial_is_read_ready() (inb(PORT + 5) & 0x01),

void serial_init() {
	ringbuffer_init(&serial_ringbuffer, serialbuffer, SERIALBUFFER_LENGTH);
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
	return ringbuffer_read_byte(&serial_ringbuffer);
}
