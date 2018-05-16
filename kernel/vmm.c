#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <boot.h>
#include <console.h>
#include <cpu.h>
#include <isr.h>
#include <paging.h>
#include <pmm.h>
#include <string.h>
#include <vmm.h>

page_directory_t __attribute__((aligned(4096))) *kernel_directory;

page_table_t *get_table(uintptr_t virtaddr, page_directory_t *directory) {
	return (page_table_t *)((uintptr_t)(directory->tables[virtaddr >> 22]) & ~0x3FF);
}

page_table_t *get_table_alloc(uintptr_t virtaddr, page_directory_t *directory) {
	page_table_t *table = get_table(virtaddr, directory);
	if (table == 0) {
		uintptr_t v = vmm_find_dma_region(1);
		assert(v != 0);
		pmm_set_block(v);
		table = (page_table_t *)(v * BLOCK_SIZE);
		map_direct_kernel((uintptr_t)table);
		memset((void *)table, 0, BLOCK_SIZE);
		directory->tables[virtaddr >> 22] = ((uintptr_t)table |
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE | PAGE_TABLE_USER);
	}
	return table;
}

page_t get_page(page_table_t *table, uintptr_t virtaddr) {
	return table->pages[virtaddr >> 12 & 0x3FF];
}

void map_page(page_table_t *table, uintptr_t virtaddr, uintptr_t physaddr, uint16_t flags) {
	assert((virtaddr & 0xFFF) == 0);
	assert(table != NULL);
	assert(((uintptr_t)table & 0xFFF) == 0);
	table->pages[(uint32_t)virtaddr >> 12 & 0x3FF] = (page_t)((uint32_t)physaddr | flags);
}

/* helper function */
// directly maps from including start to end
void map_pages(uintptr_t start, uintptr_t end, int flags, const char *name) {
	if (name != NULL) {
		if (((uintptr_t)end - (uintptr_t)start) > 0x8000) {
			printf("%s: 0x%x - 0x%x => 0x%x - 0x%x flags: 0x%x\n", name, (uintptr_t)start, (uintptr_t)end, (uintptr_t)start, (uintptr_t)end, flags);
		}
	}
	for (uintptr_t i = ((uintptr_t)start & 0xFFFFF000); i < (uintptr_t)end; i += 0x1000) {
		if (name != NULL) {
			if (!(((uintptr_t)end - (uintptr_t)start) > 0x8000)) {
				printf("%s: 0x%x => 0x%x flags: 0x%x\n", name, i, i, flags);
			}
		}
		map_page(get_table_alloc(i, kernel_directory), i, i, flags);
	}
}


// Don't use this method it will break stuff
void map_direct_kernel(uintptr_t v) {
	map_page(get_table_alloc(v, kernel_directory), v, v, PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
}

// TODO: optimise
// WARNING: this is awfully slow
// TODO: mark found pages with PAGE_VALUE_RESERVED
uintptr_t vmm_find_dma_region(size_t size) {
	if (size == 0) {
		return 0;
	}

	for (uint32_t i = 0; i < block_map_size; i++) {
		if (block_map[i] != 0xFFFFFFFF) {
			for (uint8_t j = 0; j < 32; j++) {
				uint32_t bit = (1 << j);
				if ( ! (block_map[i] & bit)) {
					uint32_t start = i*32+j;
					uint32_t len = 0;
					while (len < size) {
						if (pmm_test_block(start + len)) {
							// block used
							break;
						} else {
							uintptr_t v_addr = (start + len) * BLOCK_SIZE;

							if (get_page(get_table(v_addr, kernel_directory), v_addr) == 0) {
								len++;
							} else {
								break;
							}
						}
					}
					if (len == size) {
						return start;
					} else {
						i = i + len / 32;
						j = j + len;
					}
				}
			}
		}
	}

	return 0;
}

// TODO: optimise the next 2 functions by walking in page table increments
// finds free (continuous) virtual address space and maps it to PAGE_VALUE_RESERVED
// n in blocks
// FIXME: allocates tables for ranges that will be too small
uintptr_t find_vspace(page_directory_t *dir, size_t n) {
	/* skip block 0 */
	for (uintptr_t i = 1; i < (0x100000000 / BLOCK_SIZE); i++) {
		uintptr_t v_addr = i * BLOCK_SIZE;
		// FIXME: depends on the fact that all kernel tables are pre-allocated to avoid calling get_table_alloc which could modify the directory 'dir'
		page_table_t *table = get_table_alloc(v_addr, dir);
		if (get_page(table, v_addr) == 0) {
			uintptr_t start = v_addr;
			uintptr_t length = 1;
			while (length < n) {
				uintptr_t v_addr2 = v_addr + length*BLOCK_SIZE;
				table = get_table_alloc(v_addr2, dir);
				if (get_page(table, v_addr2) == 0) {
					length++;
				} else {
					break;
				}
			}
			if (length == n) {
				for (uintptr_t i = start;
						i < start + (length*BLOCK_SIZE);
						i += BLOCK_SIZE) {
					// it should be impossible for get_table to return NULL if it does we're fucked anyway
					map_page(get_table(i, dir), i,
						PAGE_VALUE_RESERVED, 0);
				}
//				printf(" = 0x%x\n", start);
				return start;
			} else {
				i = i + length;
			}
		}
	}

	return -1;
}

void page_fault(registers_t *regs) {
	uintptr_t address;
	__asm__ __volatile__("mov %%cr2, %0" : "=r"(address));
	printf("! page_fault !\n");
	printf("err_code: 0x%x\n", regs->err_code);
	printf("eip: 0x%x\n", (uint32_t)(regs->eip));
	printf("address: 0x%x\n", (uint32_t)(address));

	if (regs->err_code & 0x4) {
		printf("usermode\n");
	} else {
		printf("KERNEL MODE PAGE FAULT\n");
		halt();
	}
	halt();
}

void vmm_init() {
	isr_set_handler(14, page_fault);

	uintptr_t p = pmm_alloc_blocks_safe(1);
	printf("kernel directory: 0x%x\n", p);
	kernel_directory = (page_directory_t *)p;
	memset((void *)kernel_directory, 0, BLOCK_SIZE);

	for (int i = 0; i < 1024; i++) {
		uintptr_t pt = pmm_alloc_blocks_safe(1);
		memset((void *)pt, 0, BLOCK_SIZE);
		kernel_directory->tables[i] = (uintptr_t)(pt |
			PAGE_DIRECTORY_PRESENT | PAGE_DIRECTORY_READWRITE | PAGE_DIRECTORY_USER |
			PAGE_DIRECTORY_WRITE_THROUGH | PAGE_DIRECTORY_CACHE_DISABLE);
	}

	for (unsigned int i = 0; i < 1024; i++) {
		uintptr_t pt = (uintptr_t)kernel_directory->tables[i];
		map_direct_kernel(pt & ~0xFFF);
	}

	map_direct_kernel((uintptr_t)kernel_directory);

	// catch NULL pointer derefrences
	map_page(get_table(0, kernel_directory), 0, 0, 0);
}

void vmm_enable() {
	load_page_directory((uintptr_t)kernel_directory);
	enable_paging();
}
