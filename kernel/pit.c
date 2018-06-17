#include <stdint.h>

#include <console.h>
#include <cpu.h>
#include <idt.h>
#include <isr.h>
#include <pit.h>
#include <process.h>

#define PIT_0_DATA 0x40
#define PIT_1_DATA 0x41
#define PIT_2_DATA 0x42
#define PIT_COMMAND 0x43

#define FREQUENCY 1000

unsigned long ticks = 0;

static void irq0_handler(registers_t *regs, void *extra) {
	if (ticks%FREQUENCY==0) {
//		putc('.');
	}
	ticks++;
	irq_ack(regs->isr_num);
	switch_task();
}

static void pit_setup_channel_zero(int frequency) {
	int divisor = 1193182 / frequency;
	outb(PIT_COMMAND, 0x36);
	outb(PIT_0_DATA, divisor & 0xFF);
	outb(PIT_0_DATA, divisor >> 8);
}

void pit_init() {
	isr_set_handler(32 + 0, irq0_handler, NULL);
	pit_setup_channel_zero(FREQUENCY);
}

void _sleep(unsigned long miliseconds) {
	// FIXME: assert when interrupts are disabled

	unsigned long target_ticks = ticks + miliseconds;
	while (ticks < target_ticks) {
		hlt();
	}
}
