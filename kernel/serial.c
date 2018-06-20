#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <cpu.h>
#include <irq.h>
#include <process.h>
#include <ringbuffer.h>
#include <serial.h>

#define SERIALBUFFER_LENGTH 1024
static ringbuffer_t serial_ringbuffer;
static unsigned char serialbuffer[SERIALBUFFER_LENGTH];

#define PORT 0x3F8

static unsigned int irq4_handler(unsigned int irq, void *extra) {
	assert(extra != NULL);
	// FIXME: not checking if the irq was meant for us
	ringbuffer_write_byte((ringbuffer_t *)extra, inb(PORT));
	irq_ack(irq);
	return IRQ_HANDLED;
}

#define serial_is_transmit_ready() (inb(PORT + 5) & 0x20)
#define serial_is_read_ready() (inb(PORT + 5) & 0x01),

void serial_init() {
	ringbuffer_init(&serial_ringbuffer, serialbuffer, SERIALBUFFER_LENGTH);
	irq_set_handler(4, irq4_handler, &serial_ringbuffer);

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
	while (serial_is_transmit_ready() == 0) {
		switch_task();
	}
	outb(PORT, c);
}

char serial_getc() {
	return ringbuffer_read_byte(&serial_ringbuffer);
}
