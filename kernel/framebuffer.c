#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <console.h>
#include <framebuffer.h>
#include <string.h>
#include <8x8_font.h>

static volatile void *vmem = NULL;

static volatile uint32_t pitch, width, height, bpp;

static unsigned int cursor_x, cursor_y;

// assumes 24bpp
static inline void framebuffer_setpixel(const unsigned int x, const unsigned int y, uint32_t v) {
	assert(x <= width);
	assert(y <= height);
	uint8_t *where = (uint8_t *)((uintptr_t)(vmem) + y * pitch + x * (bpp / 8));
	where[0] = (v >> 0) & 0xFF;
	where[1] = (v >> 1) & 0xFF;
	where[2] = (v >> 2) & 0xFF;
}

static void framebuffer_clear(uint32_t v) {
	for (unsigned int y = 0; y < height; y++) {
		for (unsigned int x = 0; x < width; x++) {
			framebuffer_setpixel(x, y, v);
		}
	}
}

static inline void put_c_at(const char c, const unsigned int x, const unsigned int y) {
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			if (font8x8_basic[(unsigned char)c][i] & (1 << j)) {
				framebuffer_setpixel(x + j, y + i, 0xFFFFFF);
			} else {
				/* do nothing */
				framebuffer_setpixel(x + j, y + i, 0xDE00EE);
			}
		}
	}
}

static void framebuffer_scroll() {
	for (unsigned int y = 0; y < (height - 8); y++) {
		memcpy((void *)((uintptr_t)vmem + y * pitch), (void *)((uintptr_t)vmem + (y + 8) * pitch), pitch);
	}

	for (unsigned int x = 0; x < width; x++) {
		for (unsigned int y = 0; y < 8; y++) {
			framebuffer_setpixel(x, height - y - 1, 0xDE00EE);
		}
	}
}

void framebuffer_putc(const char c) {
	if (vmem == NULL) {
		return;
	}

	if ((c == '\b') && (cursor_x > 0)) {
		cursor_x--;
	} else if (c == '\n') {
		framebuffer_putc('\r');
		cursor_x = 0;
		cursor_y++;
	} else if (c == '\r') {
		cursor_x = 0;
	} else if (c == '\t') {
		cursor_x += 4;
	} else {
		put_c_at(c, cursor_x*8, cursor_y*8);
		cursor_x++;
	}

	if (cursor_x >= (width / 8)) {
		cursor_x = 0;
		cursor_y += 1;
	}
	if (cursor_y >= (height / 8)) {
		framebuffer_scroll();
		cursor_x = 0;
		cursor_y = (height / 8) - 1;
	}

}

void framebuffer_init(uintptr_t fb_addr, uint32_t fb_pitch, uint32_t fb_width,
	uint32_t fb_height, uint8_t fb_bpp, uint8_t fb_red_fp, uint8_t fb_red_ms,
	uint8_t fb_green_fp, uint8_t fb_green_ms, uint8_t fb_blue_fp, uint8_t fb_blue_ms) {
	vmem = (void *)fb_addr;
	pitch = fb_pitch;
	width = fb_width;
	height = fb_height;
	bpp = fb_bpp;

	if (bpp % 8 != 0) {
		printf("unsupported bpp!\n");
	}

	cursor_x = 0;
	cursor_y = 0;

	framebuffer_clear(0x00DE00EE);

	printf("VESA framebuffer\n");
	printf(" addr: 0x%x\n", fb_addr);
	printf(" pitch: 0x%x\n", fb_pitch);
	printf(" width: 0x%x\n", fb_width);
	printf(" height: 0x%x\n", fb_height);
	printf(" bpp: 0x%x\n", fb_bpp);

	printf(" red_field_position: 0x%x\n", fb_red_fp);
	printf(" red_mask_size: 0x%x\n", fb_red_ms);
	printf(" green_field_position: 0x%x\n", fb_green_fp);
	printf(" green_mask_size: 0x%x\n", fb_green_ms);
	printf(" blue_field_position: 0x%x\n", fb_blue_fp);
	printf(" blue_mask_size: 0x%x\n", fb_blue_ms);
	printf("\n");
}
