#ifndef GDT_H
#define GDT_H 1

#include <stddef.h>

void gdt_init();

void tss_set_kstack(uintptr_t stack);

#endif
