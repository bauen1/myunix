#include <stddef.h>
#include <stdint.h>

#include <console.h>
#include <cpu.h>
#include <tty.h>

static unsigned short cursor_x, cursor_y;
static unsigned short width, height;
static unsigned short bytes_per_char, bytes_per_line;
static volatile void *vmem = NULL;

void tty_init(uintptr_t vmem_ptr, uint32_t w, uint32_t h, uint32_t bpp, uint32_t pitch) {
	width = w;
	height = h;
	bytes_per_char = bpp;
	bytes_per_line = pitch;
	vmem = (void *)vmem_ptr;
	tty_clear_screen();

	printf("tty framebuffer:\n");
	printf(" addr: 0x%x\n", vmem_ptr);
	printf(" width: %u height: %u\n", w, h);
	printf(" framebuffer_bpp: 0x%x\n", bpp);
	printf(" bytes per text line: 0x%x\n", pitch);
}

static void tty_update_cursor() {
	unsigned short position = (cursor_y*width)+cursor_x;
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(position&0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((position>>8)&0xFF));
}

static inline void tty_clear_line(unsigned int y) {
	for (unsigned int x = 0; x <= width; x++) {
		((uint16_t *)vmem)[((y * TTY_WIDTH) + x)] = (0x07 << 8) | ' ';
	}
}

static void tty_scroll() {
	for (unsigned int y = 0; y < height; y++) {
		for (unsigned int x = 0; x <= width; x++) {
			((uint16_t *)vmem)[((y * TTY_WIDTH) + x)] = ((uint16_t *)vmem)[(((y + 1) * TTY_WIDTH) + x)];
		}
	}
	tty_clear_line(height);
}

void tty_move_cursor(unsigned int x, unsigned int y) {
	cursor_x = x;
	cursor_y = y;
	tty_update_cursor();
}

void tty_clear_screen() {
	for (unsigned int y = 0; y < height; y++) {
		tty_clear_line(y);
	}
	tty_move_cursor(0, 0);
}

char tty_putc(char c) {
	if (vmem == NULL) {
		return 0;
	}
	unsigned short old_x = cursor_x;
	unsigned short old_y = cursor_y;

	if ((c == '\b') && (cursor_x > 0)) {
		cursor_x -= 1;
	} else if (c == '\n') {
		tty_putc('\r');
		cursor_x = 0;
		cursor_y += 1;
	} else if (c == '\r') {
		cursor_x = 0;
	} else if (c == '\t') {
		cursor_x += 4;
	} else {
		((uint16_t *)vmem)[((cursor_y * TTY_WIDTH) + cursor_x)] = (0x07 << 8) | c;
		cursor_x += 1;
	}
	if ((old_x != cursor_x) || (old_y != cursor_y)) {
		if (cursor_x >= width) {
			cursor_x = 0;
			cursor_y += 1;
		}
		if (cursor_y >= height) {
			tty_scroll();
			cursor_x = 0;
			cursor_y = height - 1;
		}
		tty_update_cursor();
	}
	return c;
}
