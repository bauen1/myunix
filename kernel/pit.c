#include <stdint.h>

#include <console.h>
#include <cpu.h>
#include <irq.h>
#include <pit.h>
#include <process.h>

#define PIT_0_DATA 0x40
#define PIT_1_DATA 0x41
#define PIT_2_DATA 0x42
#define PIT_COMMAND 0x43

#define FREQUENCY 1000

unsigned long ticks = 0;

static unsigned int irq0_handler(unsigned int irq, void *extra) {
	(void)extra;
//	if (ticks % FREQUENCY == 0) {
//		putc('.');
//	}
	ticks++;
	// FIXME: we aren't checking if the IRQ was ment for us (but pit should be the only thing on IRQ0 anyway)
	irq_ack(irq);
	switch_task();
	return IRQ_HANDLED;
}

static void pit_setup_channel_zero(int frequency) {
	int divisor = 1193182 / frequency;
	outb(PIT_COMMAND, 0x36);
	outb(PIT_0_DATA, divisor & 0xFF);
	outb(PIT_0_DATA, divisor >> 8);
}

void pit_init() {
	irq_set_handler(0, irq0_handler, NULL);
	pit_setup_channel_zero(FREQUENCY);
}

void _sleep(unsigned long miliseconds) {
	// FIXME: assert when interrupts are disabled

	unsigned long target_ticks = ticks + miliseconds;
	while (ticks < target_ticks) {
		hlt();
	}
}
