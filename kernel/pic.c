#include <cpu.h>
#include <pic.h>

void pic_init() {
	outb(PIC1_COMMAND, 0x11);
	io_wait();
	outb(PIC2_COMMAND, 0x11);
	io_wait();

	outb(PIC1_DATA, 0x20); /* map IRQ0-7 to ISR32-39 */
	io_wait();
	outb(PIC2_DATA, 0x28); /* map IRQ8-15 to ISR40-47 */
	io_wait();

	outb(PIC1_DATA, 0x04); /* cascade mode */
	io_wait();
	outb(PIC2_DATA, 0x02);
	io_wait();

	outb(PIC1_DATA, 0x01);
	io_wait();
	outb(PIC2_DATA, 0x01);
	io_wait();
}
