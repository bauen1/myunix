#include <stdint.h>

#include <console.h>
#include <cpu.h>
#include <irq.h>
#include <pit.h>
#include <task.h>

#define PIT_0_DATA 0x40
#define PIT_1_DATA 0x41
#define PIT_2_DATA 0x42
#define PIT_COMMAND 0x43
#define PIT_CLOCKRATE 1193182

static void timer_phase(unsigned int frequency) {
	unsigned int divisor = PIT_CLOCKRATE / frequency;
	outb(PIT_COMMAND, 0x36);
	outb(PIT_0_DATA, divisor & 0xFF);
	outb(PIT_0_DATA, divisor >> 8);
}

uint64_t timer_ticks = 0;

static unsigned int irq0_handler(unsigned int irq, void *extra) {
	(void)extra;
	// XXX: we don't check if the IRQ was meant for us

	timer_ticks++;
	irq_ack(irq);
	scheduler_wakeup();

	return IRQ_HANDLED;
}

void pit_init(void) {
	irq_set_handler(0, irq0_handler, NULL);
	timer_phase(FREQUENCY);
}
