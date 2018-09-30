#ifndef ARCH_PAGING_H
#define ARCH_PAGING_H 1

#include <stdint.h>

extern void load_page_directory(uint32_t page_directory);
extern void enable_paging();

#endif
