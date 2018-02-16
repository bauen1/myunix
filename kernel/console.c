#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <itoa.h>
#include <serial.h>
#include <tty.h>

void console_init() {
	serial_init();
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
	char *s;

	while (*fmt != 0) {
		if (*fmt == '%') {
			fmt++;
			if (*fmt == 0) {
				break;
			}
			unsigned int width = 0;
			while ((*fmt >= '0') && (*fmt <= '9')) {
				width = width * 10 + (*fmt - '0');
				fmt++;
			}
			switch (*fmt) {
				case '%':
					putc('%');
					break;
				case 's':
					s = va_arg(args, char *);
					if (s == NULL) {
						puts("(null)");
					} else {
						puts(s);
					}
					break;
				case 'i':
				case 'd':
					itoa(va_arg(args, int), &buf[0], 10, width);
					puts(buf);
					break;
				case 'u':
					utoa(va_arg(args, unsigned int), &buf[0], 10, width);
					puts(buf);
					break;
				case 'x':
					utoa(va_arg(args, unsigned int), &buf[0], 16, width);
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
