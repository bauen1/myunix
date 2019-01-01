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
#include <ringbuffer.h>
#include <atomic.h>

static char buffer_getc(void);

static uint32_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buf) {
	(void)offset;
	assert(node == &tty_node);

	for (uintptr_t i = 0; i < size; i++) {
		((char *)buf)[i] = buffer_getc();
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
	.__refcount = -1, // immortal because it isn't allocated
};

static unsigned char buffer[2048];
static ringbuffer_t text_buffer;

void console_init(void) {
	serial_init();
	ringbuffer_init(&text_buffer, buffer, sizeof(buffer));
}

static void buffer_fill(void) {
	char tmp_buffer[1024];
	size_t i = 0;
	while (i < sizeof(tmp_buffer)) {
		char c = getc();
		assert(c != 0);
		switch(c) {
			case '\b':
			case 127:
				if (i > 0) {
					i--;
				}
				putc('\b');
				break;
			case '\n':
			default:
				tmp_buffer[i] = c;
				i++;
				putc(c);
				break;
		}
		if (c == '\n') {
			break;
		}
	}
	for (size_t i = 0; i < sizeof(tmp_buffer); i++) {
		char c = tmp_buffer[i];
		ringbuffer_write_byte(&text_buffer, c);
		if (c == '\n') {
			break;
		}
	}
}

static char buffer_getc(void) {
	if (ringbuffer_unread(&text_buffer) == 0) {
		buffer_fill();
	}

	assert(ringbuffer_unread(&text_buffer) > 0);
	return (char)ringbuffer_read_byte(&text_buffer);
}

char getc(void) {
	char c;

	c = serial_getc();
	if (c == '\r') {
		c = '\n';
	}

	return c;
}

void putc(char c) {
	static spin_t console_write_lock;
	spin_lock(console_write_lock);
	tty_putc(c);
	framebuffer_putc(c);
	serial_putc(c);
	spin_unlock(console_write_lock);
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

static mutex_t print_mutex;

void vprintf(const char *fmt, va_list args) {
	char buf[256]; // probably too much
	char *s;
	int tmp;

	mutex_lock(&print_mutex);
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
				case 'l':
					fmt++;
					if (*fmt == 'u') {
						utoa(va_arg(args, unsigned long), buf, 10, width);
					} else if ((*fmt == 'i') || (*fmt == 'd')) {
						utoa(va_arg(args, signed long), buf, 10, width);
					} else {
						assert(0 && "not implemented format character or end of string");
					}
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
				default:
					assert(0 && "not implemented format character or end of string");
					break;
			}
		} else {
			putc(*fmt);
		}
		fmt++;
	}
	mutex_unlock(&print_mutex);
}

void __attribute__((format(printf,1,2))) printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	vprintf(fmt, args);

	va_end(args);
}
