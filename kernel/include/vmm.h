#ifndef VMM_H
#define VMM_H 1

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

enum page_directory_flags {
	PAGE_TABLE_PRESENT       = 0x01,
	PAGE_TABLE_READWRITE     = 0x02,
	PAGE_TABLE_USER          = 0x04,
	PAGE_TABLE_WRITE_THROUGH = 0x08,
	PAGE_TABLE_CACHE_DISABLE = 0x10,
	PAGE_TABLE_ACCESSED      = 0x20,
	// reserved              = 0x40,
	PAGE_TABLE_SIZE          = 0x80,
};

enum page_flags {
	PAGE_PRESENT       = 0x01,
	PAGE_READWRITE     = 0x02,
	PAGE_USER          = 0x04,
	PAGE_WRITE_THROUGH = 0x08,
	PAGE_CACHE_DISABLE = 0x10,
	PAGE_ACCESSED      = 0x20,
	PAGE_DIRTY         = 0x40,
	// pat bit         = 0x80,
};

#define PAGE_VALUE_GUARD 0xFFFFF000
#define PAGE_VALUE_RESERVED 0xFFFF100

typedef uint32_t page_t;
typedef struct page_table {
	page_t pages[1024] __attribute__((packed)) __attribute__((aligned(4096)));
} page_table_t;
static_assert(sizeof(page_table_t) == PAGE_SIZE);

typedef struct page_directory {
	// this is a pointer to the real page directory
	uintptr_t *physical_tables;
	// these are pointers to where ever the page_tables might be mapped in kernel space
	// if you need to access the tables, use this
	page_table_t *tables[1024];
	// real address of physical_tables
	uintptr_t physical_address;
	/* reference count, only used for userspace directories */
	int32_t __refcount;
} page_directory_t;

extern page_directory_t *kernel_directory;

/* page_directory_t helpers */
page_directory_t *page_directory_reference(page_directory_t *pdir);
page_directory_t *page_directory_new();
void page_directory_free(page_directory_t *pdir);

void invalidate_page(uintptr_t virtaddr);
page_table_t *get_table(uintptr_t virtaddr, page_directory_t *directory);
page_table_t *get_table_alloc(uintptr_t virtaddr, page_directory_t *directory);
page_t get_page(page_table_t *table, uintptr_t virtaddr);
void set_page(page_table_t *table, uintptr_t virtaddr, page_t page);
// XXX: behaviour undefined when (virtaddr & 0xFFF) != 0
void map_page(page_table_t *table, uintptr_t virtaddr, uintptr_t physaddr, enum page_flags flags);
void map_pages(uintptr_t start, uintptr_t end, enum page_flags flags, const char *name);

// directly map a range into the kernel directory
// XXX: don't use unless absolutely needed
void map_direct_kernel(uintptr_t v);

uintptr_t find_vspace(page_directory_t *dir, size_t n); // size in blocks
uintptr_t vmm_find_dma_region(size_t size);
void *dma_malloc(size_t m);

/* debug helpers */
void dump_directory(page_directory_t *directory);

void vmm_init();
void vmm_enable();

#endif
