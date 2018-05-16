#ifndef CONSOLE_H
#define CONSOLE_H 1

#include <fs.h>
extern fs_node_t tty_node;

void console_init();

char getc();
void putc(char c);
void puts(const char *s);

void __attribute__((format(printf, 1, 2))) printf(const char *fmt, ...);

#endif
