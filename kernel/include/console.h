#ifndef CONSOLE_H
#define CONSOLE_H 1

#include <fs.h>
fs_node_t tty_node;

void console_init();

char getc();
void putc(char c);
void puts(const char *s);

__attribute__((format(printf, 1, 2)))
void printf(const char *fmt, ...);

#endif
