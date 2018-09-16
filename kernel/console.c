#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <console.h>
#include <framebuffer.h>
#include <itoa.h>
#include <keyboard.h>
#include <serial.h>
#include <tty.h>
#include <fs.h>

#define ensure_int_disabled \
	uint32_t _eflags = 0; \
	__asm__ __volatile__("pushf\npop %0" : "=r"(_eflags)); \
	__asm__ __volatile__("cli"); \
	{

#define restore_int }\
	if (_eflags & (1<<9)) { __asm__ __volatile__("sti"); };

static uint32_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buf) {
	(void)offset;
	assert(node == &tty_node);

	for (uintptr_t i = 0; i < size; i++) {
		((char *)buf)[i] = getc();
	}
	return size;
}

static uint32_t tty_write(fs_node_t *node, uint32_t offset, uint32_t size, void *buf) {
	(void)offset;
	assert(node == &tty_node);

	for (uintptr_t i = 0; i < size; i++) {
		putc(((char *)buf)[i]);
	}
	return size;
}

fs_node_t tty_node = {
	.name = "tty",
	.object = NULL,
	.length = 0,
	.read = tty_read,
	.write = tty_write,
	.__refcount = -1, // ensure it is never closed
};

void console_init() {
	serial_init();
}

char getc() {
	char c;

	uint32_t eflags = 0;
	__asm__ __volatile__("pushf\n"
				"pop %0\n"
				"cli"
			: "=r" (eflags));

	c = keyboard_getc();
	//char c = serial_getc();
	if (c == '\r') {
		c = '\n';
	}

	if (eflags & ( 1 << 9) ) {
		__asm__ __volatile__("sti");
	}

	return c;
}

void putc(char c) {
	uint32_t eflags = 0;
	__asm__ __volatile__("pushf\n"
				"pop %0\n"
				"cli"
			: "=r" (eflags));

	tty_putc(c);
	framebuffer_putc(c);
	serial_putc(c);

	if (eflags & ( 1 << 9) ) {
		__asm__ __volatile__("sti");
	}
}

void puts(const char *s) {
	if (s == NULL) {
		puts("(null)");
	} else {
		for (int i = 0; s[i] != 0; i++) {
			putc(s[i]);
		}
	}
}

void vprintf(const char *fmt, va_list args) {
	char buf[256]; // probably too much
	char *s;
	int tmp;

	while (*fmt != 0) {
		if (*fmt == '%') {
			fmt++;
			if (*fmt == 0) {
				assert(0 && "% immideatly followed by NULL, most likely a bug");
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
				case 'c':
					tmp = (int)va_arg(args, int);
					putc(tmp);
					break;
				case 's':
					s = va_arg(args, char *);
					puts(s);
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
				case 'p':
					putc('0');
					putc('x');
					utoa(va_arg(args, unsigned int), &buf[0], 16, width);
					puts(buf);
					break;
				case 'x':
					utoa(va_arg(args, unsigned int), &buf[0], 16, width);
					puts(buf);
					break;
				case 0:
					assert(0 && "this should not happen");
				default:
					putc(*fmt);
			}
		} else {
			putc(*fmt);
		}
		fmt++;
	}
}

void printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	vprintf(fmt, args);

	va_end(args);
}
