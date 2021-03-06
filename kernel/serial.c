#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <cpu.h>
#include <irq.h>
#include <process.h>
#include <ringbuffer.h>
#include <serial.h>

#define PORT 0x3F8
#define serial_is_transmit_ready() (inb(PORT + 5) & 0x20)
#define serial_is_read_ready() (inb(PORT + 5) & 0x01),

#define SERIALBUFFER_LENGTH 1024
static ringbuffer_t serial_ringbuffer;
static unsigned char serialbuffer[SERIALBUFFER_LENGTH];

static unsigned int irq4_handler(unsigned int irq, void *extra) {
	assert(extra != NULL);
	if (inb(PORT + 5) & 1) { // byte received ?
		char c = inb(PORT);
		ringbuffer_write_byte((ringbuffer_t *)extra, c);
		irq_ack(irq);
		return IRQ_HANDLED;
	}

	return IRQ_IGNORED;
}

void serial_init() {
	ringbuffer_init(&serial_ringbuffer, serialbuffer, SERIALBUFFER_LENGTH);
	irq_set_handler(4, irq4_handler, &serial_ringbuffer);

	// init serial COM0
	outb(PORT + 1, 0x00); // Disable all interrupts
	outb(PORT + 3, 0x80); // enable DLAB
	// 38400 baud
	outb(PORT + 0, 0x03); // divisor low
	outb(PORT + 1, 0x00); // divisor high
	outb(PORT + 3, 0x03); // disable DLAB, 8 bits, no parity one stop
	outb(PORT + 2, 0xC7); // enable FIFO, clear them, 14 byte threshold
	outb(PORT + 4, 0x0B); // IRQs enabled
	outb(PORT + 1, 0x01); // enable data-received IRQ
//	outb(PORT + 1, 0x0F); // enable all IRQs
}

void serial_putc(char c) {
	if (c == '\n') {
		serial_putc('\r');
	}
	while (serial_is_transmit_ready() == 0) {
		yield();
	}
	outb(PORT, c);
}

char serial_getc() {
	return ringbuffer_read_byte(&serial_ringbuffer);
}
