#include <stddef.h>
#include <stdint.h>

#include <serial.h>
#include <tty.h>

void console_init() {
	serial_init();
	tty_init();
}

char getc() {
	return serial_getc();
}

void putc(char c) {
	tty_putc(c);
	serial_putc(c);
}

void puts(const char *s) {
	for (int i = 0; s[i] != 0; i++) {
		putc(s[i]);
	}
}
