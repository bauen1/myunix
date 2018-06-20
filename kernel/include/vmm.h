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
#define PAGE_VALUE_RESERVED 0xFFFF100

typedef uint32_t page_t;
typedef struct page_table {
	page_t pages[1024] __attribute__((packed)) __attribute__((aligned(4096)));
} page_table_t;
typedef struct page_directory {
	/* page_table_t */
	uintptr_t tables[1024] __attribute__((packed)) __attribute__((aligned(4096)));
} page_directory_t;

extern page_directory_t *kernel_directory;

page_table_t *get_table(uintptr_t virtaddr, page_directory_t *directory);
page_table_t *get_table_alloc(uintptr_t virtaddr, page_directory_t *directory);
page_t get_page(page_table_t *table, uintptr_t virtaddr);
// behaviour undefined when (virtaddr & 0xFFF) != 0
void map_page(page_table_t *table, uintptr_t virtaddr, uintptr_t physaddr, uint16_t flags);
void map_pages(uintptr_t start, uintptr_t end, int flags, const char *name);

// directly map a range into the kernel directory
void map_direct_kernel(uintptr_t v);

uintptr_t find_vspace(page_directory_t *dir, size_t n); // size in blocks
uintptr_t vmm_find_dma_region(size_t size);
void *dma_malloc(size_t m);

void vmm_init();

void vmm_enable();

/* misc. helpers */

void dump_directory(page_directory_t *directory);

#endif
