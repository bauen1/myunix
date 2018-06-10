#include <cpu.h>
#include <pic.h>

void pic_init() {
	outb(PIC1_COMMAND, 0x11);
	iowait();
	outb(PIC2_COMMAND, 0x11);
	iowait();

	outb(PIC1_DATA, 0x20); /* map IRQ0-7 to ISR32-39 */
	iowait();
	outb(PIC2_DATA, 0x28); /* map IRQ8-15 to ISR40-47 */
	iowait();

	outb(PIC1_DATA, 0x04); /* cascade mode */
	iowait();
	outb(PIC2_DATA, 0x02);
	iowait();

	outb(PIC1_DATA, 0x01);
	iowait();
	outb(PIC2_DATA, 0x01);
	iowait();
/*
	outb(PIC1_DATA, 0x00);
	iowait();
	outb(PIC2_DATA, 0x00);
	iowait();
*/
}

void pic_send_eoi(unsigned int irq) {
	if (irq >= 8) {
		outb(PIC2_COMMAND, PIC_EOI);
	}

	outb(PIC1_COMMAND, PIC_EOI);
}
