#ifndef CONSOLE_H
#define CONSOLE_H 1

#include <stdarg.h>
#include <fs.h>

extern fs_node_t tty_node;

void console_init(void);

char getc(void);
void putc(char c);
void puts(const char *s);

void __attribute__((format(printf, 1, 2))) printf(const char *fmt, ...);
void vprintf(const char *fmt, va_list args);

#endif
