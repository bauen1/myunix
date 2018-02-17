#ifndef VMM_H
#define VMM_H 1

#include <stdint.h>

#define PAGE_DIRECTORY_PRESENT 0x01
#define PAGE_DIRECTORY_READWRITE 0x02
#define PAGE_DIRECTORY_USER 0x04
#define PAGE_DIRECTORY_WRITE_THROUGH 0x08
#define PAGE_DIRECTORY_CACHE_DISABLE 0x10
#define PAGE_DIRECTORY_ACCESSED 0x20
#define PAGE_DIRECTOY_PAGE_SIZE 0x80

#define PAGE_TABLE_PRESENT 0x01
#define PAGE_TABLE_READWRITE 0x02
#define PAGE_TABLE_USER 0x04
#define PAGE_TABLE_WRITE_THROUGH 0x08
#define PAGE_TABLE_CACHE_DISABLE 0x10
#define PAGE_TABLE_ACCESSED 0x20
#define PAGE_TABLE_DIRTY 0x40

extern __attribute__((aligned(4096))) uint32_t page_directory[1024];
extern __attribute__((aligned(4096))) uint32_t page_tables[1024][1024];

void map_page(void *physaddr, void *virtaddr, uint16_t flags);

void vmm_init();

void vmm_enable();

#endif
