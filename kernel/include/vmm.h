#ifndef VMM_H
#define VMM_H 1

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

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

#define PAGE_VALUE_GUARD 0xFFFFF000

extern __attribute__((aligned(4096))) uint32_t kernel_directory[1024];
extern __attribute__((aligned(4096))) uint32_t kernel_tables[1024][1024];

uint32_t *get_table(uintptr_t virtaddr, uint32_t *directory);
uint32_t *get_table_alloc(uintptr_t virtaddr, uint32_t *directory);
uint32_t get_page(uint32_t *table, uintptr_t virtaddr);
// behaviour undefined when (virtaddr & 0xFFF) != 0
void map_page(uint32_t *table, uintptr_t virtaddr, uintptr_t physaddr, uint16_t flags);
void map_pages(void *start, void *end, int flags, const char *name);

// directly map a range into the kernel directory
void map_direct_kernel(uintptr_t v);

uintptr_t find_vspace(uint32_t *dir, size_t n); // size in blocks

void vmm_init();

void vmm_enable();

#endif
