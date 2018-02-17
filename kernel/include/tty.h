#ifndef TTY_H
#define TTY_H 1

#include <stddef.h>
#include <stdint.h>

#define TTY_DEFAULT_VMEM_ADDR (0xb8000)
#define TTY_DEFAULT_HEIGHT (25)
#define TTY_DEFAULT_WIDTH (80)
#define TTY_DEFAULT_ATTRIBUTE 0x07

void tty_init(uintptr_t vmem_ptr, uint32_t w, uint32_t h, uint32_t bpp, uint32_t pitch);

void tty_clear_screen();

void tty_move_cursor(unsigned int x, unsigned int y);

void tty_putc(char c);

#endif
