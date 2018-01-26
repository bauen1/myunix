#ifndef TTY_H
#define TTY_H 1

#define TTY_VMEM_ADDR (0xb8000)
#define TTY_HEIGHT (25)
#define TTY_WIDTH (80)

void tty_init();

void tty_clear_screen();

void tty_move_cursor(unsigned int x, unsigned int y);

char tty_putchar(char c);

void tty_puts(const char *str);

#endif
