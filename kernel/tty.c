#include <stddef.h>
#include <stdint.h>

#include <cpu.h>
#include <tty.h>

void tty_init() {
	tty_clear_screen();
}

static unsigned short cursor_x, cursor_y;
static volatile uint16_t *vmem = (uint16_t *)TTY_VMEM_ADDR;

static void tty_update_cursor() {
	unsigned short position = (cursor_y*TTY_WIDTH)+cursor_x;
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(position&0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((position>>8)&0xFF));
}

static inline void tty_clear_line(unsigned int y) {
	for (unsigned int x = 0; x <= TTY_WIDTH; x++) {
		vmem[((y * TTY_WIDTH) + x)] = (0x0F << 8) | ' ';
	}
}

static void tty_scroll() {
	if (cursor_y >= TTY_HEIGHT) {
		for (unsigned int y = 0; y < TTY_HEIGHT; y++) {
			for (unsigned int x = 0; x <= TTY_WIDTH; x++) {
				vmem[((y * TTY_WIDTH) + x)] = vmem[(((y + 1) * TTY_WIDTH) + x)];
			}
		}
		tty_clear_line(TTY_HEIGHT);
		tty_move_cursor(0, TTY_HEIGHT - 1);
	}
}

void tty_move_cursor(unsigned int x, unsigned int y) {
	cursor_x = x;
	cursor_y = y;
	tty_update_cursor(); // FIXME: inefficient in most cases
}

void tty_clear_screen() {
	for (unsigned int y = 0; y < TTY_HEIGHT; y++) {
		tty_clear_line(y);
	}
	tty_move_cursor(0, 0);
}

char tty_putc(char c) {
	if ((c == '\b') && (cursor_x > 0)) {
		tty_move_cursor(cursor_x - 1, cursor_y);
	} else if (c == '\n') {
		tty_putc('\r');
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
		tty_scroll();
	}
	return c;
}

void tty_puts(const char *str) {
	while (*str != 0) {
		tty_putc(*str++);
	}
}
