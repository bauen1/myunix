#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <framebuffer.h>
#include <string.h>
#include <console.h>

static volatile void *vmem = NULL;

static volatile uint32_t pitch, width, height, bpp;

static inline void framebuffer_setpixel(uint32_t x, uint32_t y, uint32_t v) {
	*(uint32_t *)(vmem + y*pitch + x*bpp) = v;
}

void framebuffer_init(uintptr_t fb_addr, uint32_t fb_pitch, uint32_t fb_width,
	uint32_t fb_height, uint8_t fb_bpp, uint8_t fb_red_fp, uint8_t fb_red_ms,
	uint8_t fb_green_fp, uint8_t fb_green_ms, uint8_t fb_blue_fp, uint8_t fb_blue_ms) {
	vmem = (void *)fb_addr;
	pitch = fb_pitch;
	width = fb_width;
	height = fb_height;
	bpp = fb_bpp;
	(void)fb_red_fp;
	(void)fb_red_ms;
	(void)fb_green_fp;
	(void)fb_green_ms;
	(void)fb_blue_fp;
	(void)fb_blue_ms;

	if (bpp % 8 != 0) {
		printf("unsupported bpp!\n");
	}

	for (int i = 0; i < 200; i++) {
		framebuffer_setpixel(i, i%10, 0xDEAD);
	}
}
