#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <boot.h>
#include <console.h>
#include <cpu.h>
#include <isr.h>
#include <paging.h>
#include <process.h>
#include <pmm.h>
#include <string.h>
#include <vmm.h>

page_directory_t __attribute__((aligned(4096))) *kernel_directory;

page_table_t *get_table(uintptr_t virtaddr, page_directory_t *directory) {
	assert(directory != NULL);
	// FIXME: we should probably assert that it is mapped as present
	return (page_table_t *)((uintptr_t)(directory->tables[virtaddr >> 22]) & ~0x3FF);
}

page_table_t *get_table_alloc(uintptr_t virtaddr, page_directory_t *directory) {
	assert(directory != NULL);
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
	assert(table != NULL);
	return table->pages[virtaddr >> 12 & 0x3FF];
}

void map_page(page_table_t *table, uintptr_t virtaddr, uintptr_t physaddr, uint16_t flags) {
	assert(table != NULL);
	assert((virtaddr & 0xFFF) == 0);
	assert(((uintptr_t)table & 0xFFF) == 0);
	// FIXME: assert physaddr and flags
	table->pages[(uint32_t)virtaddr >> 12 & 0x3FF] = (page_t)((uint32_t)physaddr | flags);
	// FIXME: should probably call invlpg here
}

/* helper function */
// directly maps from (including) start to end to kernel space
void map_pages(uintptr_t start, uintptr_t end, int flags, const char *name) {
	if (name != NULL) {
		printf("%s: 0x%x - 0x%x => 0x%x - 0x%x flags: 0x%x\n", name, (uintptr_t)start, (uintptr_t)end, (uintptr_t)start, (uintptr_t)end, flags);
	}
	for (uintptr_t i = (start & 0xFFFFF000); i < end; i += BLOCK_SIZE) {
		map_page(get_table(i, kernel_directory), i, i, flags);
	}
}


// Don't use this method it will break stuff
void map_direct_kernel(uintptr_t v) {
//	if ((v & 0xFFF) != 0) {
//		printf("WARN: map_direct_kernel pointer not aligned! (0x%x)\n", v);
//	}
//	page_table_t *table = get_table(v, kernel_directory);
//	page_t o_page = get_page(table, v);
//	if (o_page != 0) {
//		if ((o_page & ~0xFFF) == v) {
//			if (o_page == (v | PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE)) {
//				printf(" mapped using same value 0x%x => 0x%x\n", v, v);
//			} else {
//				printf("already mapped ?!! v: 0x%x page: 0x%x\n", v, o_page);
//			}
//		} else {
//			printf("we're in deep trouble!\n");
//			printf("v: 0x%x page: 0x%x\n", v, get_page(table, v));
//			assert(0);
//		}
//	}
//
	map_page(get_table(v, kernel_directory), v, v, PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
	__asm__ __volatile__ ("invlpg (%0)" : : "b" (v) : "memory");
}

// TODO: optimise
// WARNING: this is awfully slow
// TODO: mark found pages with PAGE_VALUE_RESERVED
// TODO: optimise
inline uintptr_t vmm_find_dma_region(size_t size) {
	assert(size != 0);

	for (uint32_t i = 0; i < block_map_size; i++) {
		if (block_map[i] == 0xFFFFFFFF) {
			continue;
		}

		for (uint8_t j = 0; j < 32; j++) {
			uint32_t bit = (1 << j);
			if ( ! (block_map[i] & bit)) {
				uint32_t start = i*32+j;
				uint32_t len = 0;
				while (len < size) {
					uintptr_t v_addr = (start + len) * BLOCK_SIZE;
					if (pmm_test_block(start + len) ||
						(get_page(get_table(v_addr, kernel_directory), v_addr) != 0)) {
						// block used
						break;
					} else {
						len++;
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

	printf("CRITICAL: NO DMA REGION OF SIZE %u FOUND!!\n", (uintptr_t)size);
	printf("pmm_count_free_blocks(): 0x%x\n", pmm_count_free_blocks());
	return 0;
}

void *dma_malloc(size_t m) {
	assert(m != 0);
	size_t n = (BLOCK_SIZE - 1 + m) / BLOCK_SIZE;
	uintptr_t v = vmm_find_dma_region(n);
	assert(v != 0);
	for (size_t i = 0; i < n; i++) {
		pmm_set_block(v + i);
		map_direct_kernel((v + i) * BLOCK_SIZE);
		memset((void *)((v + i) * BLOCK_SIZE), 0, BLOCK_SIZE);
	}

	return (void *)(v * BLOCK_SIZE);
}


// TODO: optimise the next 2 functions by walking in page table increments
// finds free (continuous) virtual address space and maps it to PAGE_VALUE_RESERVED
// n in blocks
// FIXME: allocates tables for ranges that will be too small
// returns 0 in case of failure
uintptr_t find_vspace(page_directory_t *dir, size_t n) {
	assert(dir != NULL);
	assert(n != 0);
//	printf("find_vspace(0x%x, 0x%x);\n", (uintptr_t)dir, (uintptr_t)n);
	/* skip block 0 */
	for (uintptr_t i = 1; i < (0x100000000 / BLOCK_SIZE); i++) {
		uintptr_t v_addr = i * BLOCK_SIZE;
		// FIXME: depends on the fact that all kernel tables are pre-allocated to avoid calling get_table_alloc which could modify the directory 'dir'
		page_table_t *table = get_table_alloc(v_addr, dir);
		if (get_page(table, v_addr) != 0) {
			// page mapped, skip
			continue;
		}

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
				// get_table can't return NULL here
				map_page(get_table(i, dir), i, PAGE_VALUE_RESERVED, 0);
			}
//			printf(" = 0x%x\n", start);
			return start;
		} else {
			i = i + length;
		}
	}

	return -1;
}

static void dump_table(page_table_t *table, uintptr_t table_addr, char *prefix) {
	assert(table != NULL);
	assert(prefix != NULL); // technically valid, but most likely a bug
//	assert((table_addr & 0xFFFF) == 0); // FIXME: implement alignment check
	for (uintptr_t page_i = 0; page_i < 1024; page_i++) {
		page_t page = table->pages[page_i];
		if (page != 0) {
			uintptr_t v_addr = (table_addr) | (page_i << 12);

			printf("%s0x%8x => 0x%8x ", prefix, v_addr, (uintptr_t)page);
			if (((uintptr_t)page) & PAGE_TABLE_PRESENT) {
				printf("present ");
			}
			if (((uintptr_t)page) & PAGE_TABLE_USER) {
				printf("user");
			}
			printf("\n");
		}
	}
}

static void dump_directory(page_directory_t *directory) {
	assert(directory != NULL);
	printf("--- directory: 0x%x ---\n", (uintptr_t)directory);
	for (uintptr_t table_i = 0; table_i < 1024; table_i++) {
		uintptr_t table_p = directory->tables[table_i];
		if (table_p == 0) {
			// skip
			continue;
		}

		printf(" 0x%8x table: 0x%8x\n", table_i << 22, table_p);
		if (table_p & PAGE_DIRECTORY_PRESENT) {
			page_table_t *table = (page_table_t *)(table_p & ~0x3FF);
			dump_table(table, table_i << 22, "  ");
		}
	}
}

static void page_fault(registers_t *regs) {
	uintptr_t address;
	__asm__ __volatile__("mov %%cr2, %0" : "=r"(address));
	printf("! page_fault !\n");
	printf("err_code: 0x%x\n", regs->err_code);
	printf("eip: 0x%x\n", (uint32_t)(regs->eip));
	printf("address: 0x%x\n", (uint32_t)(address));

	if (regs->err_code & 0x4) {
		printf("usermode\n");
		printf("current_process: 0x%x\n", (uintptr_t)current_process);
		printf("current_process->kstack: 0x%x\n", current_process->kstack);
		printf("current_process->kstack_size: 0x%x\n", (uintptr_t)current_process->kstack_size);
		printf("current_process->regs: 0x%x\n", (uintptr_t)current_process->regs);
		printf("current_process->esp: 0x%x\n", current_process->esp);
		printf("current_process->ebp: 0x%x\n", current_process->ebp);
		printf("current_process->eip: 0x%x\n", current_process->eip);
		printf("current_process->pdir: 0x%x\n", (uintptr_t)current_process->pdir);
		printf("current_process->name: '%s'\n", current_process->name);
		printf("current_process->pid: 0x%x\n", current_process->pid);
		printf("current_process->fd_table: 0x%x\n", (uintptr_t)current_process->fd_table);
		printf("\n");
		dump_directory(current_process->pdir);
	} else {
		printf("KERNEL MODE PAGE FAULT\n");
		dump_directory(kernel_directory);
		print_stack_trace(200);
		halt();
	}
	halt();
}

void vmm_init() {
	// XXX: a lot of code depends on all the kernel tables being pre-allocated to avoid calling get_table_alloc (which could result in an infinite loop)
	isr_set_handler(14, page_fault);

	uintptr_t p = pmm_alloc_blocks_safe(1);
	printf("kernel directory: 0x%x\n", p);
	kernel_directory = (page_directory_t *)p;
	memset((void *)kernel_directory, 0, BLOCK_SIZE);

	for (int i = 0; i < 1024; i++) {
		uintptr_t pt = pmm_alloc_blocks_safe(1);
		memset((void *)pt, 0, BLOCK_SIZE);
		// XXX: Don't map page tables with USER bit set, since no usercode should ever need to use the kernel directory
		kernel_directory->tables[i] = (uintptr_t)(pt |
			PAGE_DIRECTORY_PRESENT | PAGE_DIRECTORY_READWRITE |
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
