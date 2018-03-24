// TODO: dynamically allocate space for the kernel directory & tables
// always keep ~32 pages preallocated and zeroed to avoid a loop-of-death
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

__attribute__((aligned(4096))) uint32_t kernel_directory[1024];
__attribute__((aligned(4096))) uint32_t kernel_tables[1024][1024];

uint32_t *get_table(uintptr_t virtaddr, uint32_t *directory) {
	return (uint32_t *)(directory[virtaddr >> 22] & ~0xFFF);
}

uint32_t *get_table_alloc(uintptr_t virtaddr, uint32_t *directory) {
	uintptr_t pt = (uintptr_t)get_table(virtaddr, directory);
	if (pt == 0) {
		pt = (uintptr_t)pmm_alloc_blocks_safe(1);
		map_direct_kernel(pt);
		memset((void *)pt, 0, BLOCK_SIZE);
		directory[virtaddr >> 22] = (uint32_t)pt |
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE | PAGE_TABLE_USER;
	}
	return (uint32_t *)pt;
}

uint32_t get_page(uint32_t *table, uintptr_t virtaddr) {
	return table[(uint32_t)virtaddr >> 12 & 0x3FF];
}

void map_page(uint32_t *table, uintptr_t virtaddr, uintptr_t physaddr, uint16_t flags) {
	table[(uint32_t)virtaddr >> 12 & 0x3FF] = ((uint32_t)physaddr | flags);
}

/* helper function */
// directly maps from including start to end
void map_pages(void *start, void *end, int flags, const char *name) {
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

// finds free (continous) virtual address space
// n in blocks
uintptr_t find_vspace(uint32_t *dir, size_t n) {
//	printf("find_vspace(0x%x, 0x%x)\n", (uintptr_t)dir, n);
	for (uintptr_t i = 0; i < 0x100000000 / BLOCK_SIZE; i++) {
		uintptr_t virtaddr = i * BLOCK_SIZE;
		uint32_t *table = get_table(virtaddr, dir);
		assert(table != NULL);
//		printf("  page[0x%x]: 0x%x\n", virtaddr, get_page(table, virtaddr));
		if (get_page(table, virtaddr) == 0) {
			uintptr_t start = virtaddr;
			uintptr_t length = 1;
			while (length < n) {
//				printf("  page[0x%x]: 0x%x\n", virtaddr + length * BLOCK_SIZE, get_page(table, virtaddr + length * BLOCK_SIZE));
				if (get_page(table, virtaddr + length*BLOCK_SIZE) == 0) {
					length++;
				} else {
					break;
				}
			}
			if (length == n) {
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

	if (regs->err_code && 0x4) {
		printf("usermode\n");
	} else {
		printf("KERNEL MODE PAGE FAULT\n");
		halt();
	}
	halt();
}

void vmm_init() {
	isr_set_handler(14, page_fault);

	for (int i = 0; i < 1024; i++) {
		uint32_t pt = (uint32_t)kernel_tables[i];
		kernel_directory[i] = pt |
			PAGE_DIRECTORY_PRESENT | PAGE_DIRECTORY_READWRITE | PAGE_DIRECTORY_USER |
			PAGE_DIRECTORY_WRITE_THROUGH | PAGE_DIRECTORY_CACHE_DISABLE;
	}

	// catch NULL pointer derefrences
	map_page(get_table(0, kernel_directory), 0, 0, 0);
}

void vmm_enable() {
	load_page_directory((uint32_t)kernel_directory);
	enable_paging();
}
