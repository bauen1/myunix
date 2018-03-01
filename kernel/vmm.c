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
#include <vmm.h>

__attribute__((aligned(4096))) uint32_t kernel_directory[1024];
__attribute__((aligned(4096))) uint32_t kernel_tables[1024][1024];

uint32_t *get_table(uintptr_t virtaddr, uint32_t *directory) {
	return (uint32_t *)(directory[virtaddr >> 22] & ~0xFFF);
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
		map_page(get_table(i, kernel_directory), i, i, flags);
	}
}

void *page_fault(registers_t *regs) {
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

	// TODO: dynamically alloc
	for (int i = 0; i < 1024; i++) {
		kernel_directory[i] = ((uint32_t)kernel_tables[i]) | PAGE_DIRECTORY_PRESENT | PAGE_DIRECTORY_READWRITE | PAGE_DIRECTORY_USER;
	}

	// catch NULL pointer derefrences
	map_page(get_table_alloc(0, kernel_directory), 0, 0, 0);
}

void vmm_enable() {
	load_page_directory((uint32_t)kernel_directory);
	enable_paging();
}
