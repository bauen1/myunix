#ifndef ARCH_GDT_FLUSH_H
#define ARCH_GDT_FLUSH_H 1

#include <stdint.h>

extern void gdt_flush(uintptr_t);

#endif
