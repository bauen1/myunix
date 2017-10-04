#include <cpu.h>
#include <stdint.h>
#include <tty.h>

static volatile uint16_t *vmem = (uint16_t *)TTY_VMEM_ADDR;

void tty_move_cursor(unsigned int x, unsigned int y) {
	unsigned short position = (y*80)+x;
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(position&0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((position>>8)&0xFF));
}

void tty_clear_screen() {
	for (unsigned int y = 0; y < TTY_HEIGHT; y++) {
		for (unsigned int x = 0; x < TTY_WIDTH; x++) {
			vmem[((y * TTY_WIDTH) + x)] = (0x0F << 8) | ' ';
		}
	}
}
