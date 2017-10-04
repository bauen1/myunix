#include <cpu.h>
#include <stdint.h>
#include <tty.h>


void tty_move_cursor(unsigned int x, unsigned int y) {
	unsigned short position = (y*80)+x;
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(position&0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((position>>8)&0xFF));
}
