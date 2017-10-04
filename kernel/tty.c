#include <cpu.h>
#include <stdint.h>
#include <tty.h>

static unsigned short cursor_x, cursor_y;
static volatile uint16_t *vmem = (uint16_t *)TTY_VMEM_ADDR;

static void tty_update_cursor() {
	unsigned short position = (cursor_y*TTY_WIDTH)+cursor_x;
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(position&0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((position>>8)&0xFF));
}

void tty_move_cursor(unsigned int x, unsigned int y) {
	cursor_x = x;
	cursor_y = y;
	tty_update_cursor(); // FIXME: inefficient in most cases
}

void tty_clear_screen() {
	for (unsigned int y = 0; y < TTY_HEIGHT; y++) {
		for (unsigned int x = 0; x < TTY_WIDTH; x++) {
			vmem[((y * TTY_WIDTH) + x)] = (0x0F << 8) | ' ';
		}
	}
	tty_move_cursor(0, 0);
}

char tty_putchar(char c) {
	if ((c == '\b') && (cursor_x > 0)) {
		tty_move_cursor(cursor_x - 1, cursor_y);
	} else if (c == '\n') {
		tty_move_cursor(0, cursor_y + 1);
	} else if (c == '\r') {
		tty_move_cursor(0, cursor_y);
	} else if (c == '\t') {
		tty_move_cursor(cursor_x + 4, cursor_y);
	} else {
		vmem[((cursor_y * TTY_WIDTH) + cursor_x)] = (0x0F << 8) | c;
		tty_move_cursor(cursor_x + 1, cursor_y);
	}
	if (cursor_x >= TTY_WIDTH) {
		tty_move_cursor(0, cursor_y + 1);
	}
	if (cursor_y >= TTY_HEIGHT) {
		// TODO: tty_scroll();
	}
	return c;
}

void tty_puts(const char *str) {
	while (*str != 0) {
		tty_putchar(*str++);
	}
}
