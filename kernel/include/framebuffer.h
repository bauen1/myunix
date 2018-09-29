#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H 1

#include <stddef.h>
#include <stdint.h>

void framebuffer_putc(char c);

void framebuffer_init(uintptr_t fb_addr, uint32_t fb_pitch, uint32_t fb_width,
	uint32_t fb_height, uint8_t fb_bpp, uint8_t fb_red_fp, uint8_t fb_red_ms,
	uint8_t fb_green_fp, uint8_t fb_green_ms, uint8_t fb_blue_fp, uint8_t fb_blue_ms);
void framebuffer_enable_double_buffer();

#endif
