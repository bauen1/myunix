#ifndef SERIAL_H
#define SERIAL_H 1

void serial_init();
void serial_putc(char c);
char serial_getc();
void serial_puts(const char *str);

#endif
