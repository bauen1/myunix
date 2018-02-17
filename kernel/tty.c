#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <console.h>
#include <cpu.h>
#include <tty.h>

static unsigned short cursor_x, cursor_y;
static unsigned short width, height;
static uintptr_t bpp, pitch;
static volatile void *vmem = NULL;

void tty_init(uintptr_t vmem_ptr, uint32_t w, uint32_t h, uint32_t fb_bpp, uint32_t fb_pitch) {
	width = w;
	height = h;
	bpp = fb_bpp / 8;
	pitch = fb_pitch;
	vmem = (void *)vmem_ptr;

	assert(fb_bpp == 0x10);

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
	for (unsigned int x = 0; x < width; x++) {
		uint8_t *where = (uint8_t *)((uintptr_t)vmem + y * pitch + x * bpp);
		where[0] = ' ';
		where[1] = TTY_DEFAULT_ATTRIBUTE;
	}
}

/* copy one line to another */
static void tty_copy_line(unsigned int s, unsigned int d) {
	for (unsigned int x = 0; x < width; x++) {
		uint16_t *dst = (uint16_t *)((uintptr_t)vmem + d * pitch + x * bpp);
		uint16_t *src = (uint16_t *)((uintptr_t)vmem + s * pitch + x * bpp);
		*dst = *src;
	}
}

static void tty_scroll() {
	for (unsigned int y = 0; y < (height-1); y++) {
		tty_copy_line(y + 1, y);
	}

	tty_clear_line(height - 1);
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

void tty_putc(char c) {
	if (vmem == NULL) {
		return;
	}

	assert(cursor_x < width);
	assert(cursor_y < height);
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
		uint8_t *where = (uint8_t *)((uintptr_t)vmem + cursor_y * pitch + cursor_x * bpp);
		where[0] = c;
		where[1] = TTY_DEFAULT_ATTRIBUTE;
		cursor_x += 1;
	}

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
