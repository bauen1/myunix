#include <stddef.h>
#include <stdint.h>

#include <serial.h>
#include <tty.h>

void console_init() {
	serial_init();
	tty_init();
}

char getc() {
	char c = serial_getc();
	if (c == '\r') {
		return '\n';
	} else {
		return c;
	}
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

void printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	char buf[256]; // probably too much

	while (*fmt != 0) {
		if (*fmt == '%') {
			fmt++;
			if (*fmt == 0) {
				break;
			}
			switch (*fmt) {
				case '%':
					putc('%');
					break;
				case 's':
					puts(va_arg(args, char *));
					break;
				case 'd':
					itoa(va_arg(args, int), &buf[0], 10);
					puts(buf);
					break;
				case 'x':
					itoa(va_arg(args, int), &buf[0], 16);
					puts(buf);
					break;
				default:
					putc(*fmt);
			}
		} else {
			putc(*fmt);
		}
		fmt++;
	}

	va_end(args);
}
