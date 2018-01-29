#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <vmm.h>
#include <paging.h>
#include <console.h>
#include <boot.h>
#include <isr.h>
#include <cpu.h>

__attribute__((aligned(4096))) uint32_t page_directory[1024];
__attribute__((aligned(4096))) uint32_t page_tables[1024][1024];

void map_page(void *physaddr, void *virtaddr, uint16_t flags) {
	page_tables[(uint32_t)virtaddr >> 22][(uint32_t)virtaddr >> 12 & 0x03FF] = ((uint32_t)physaddr) | flags;
}

static void *page_fault(registers_t *regs) {
	uintptr_t address;
	__asm__ __volatile__("mov %%cr2, %0" : "=r"(address));
	printf("page_fault\neip: 0x%x\nerr_code: 0x%x\naddress: 0x%x\n", regs->eip, regs->err_code, address);
	halt();
}

void vmm_init() {
	isr_set_handler(14, page_fault);

	for (int i = 0; i < 1024; i++) {
		for (int j = 0; j < 1024; j++) {
			page_tables[i][j] = ((i * 1024 * 0x1000) + (j * 0x1000)) | PAGE_DIRECTORY_PRESENT | PAGE_DIRECTORY_READWRITE;
		}
		page_directory[i] = ((uint32_t)page_tables[i]) | PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE;
	}

	load_page_directory((uint32_t)page_directory);
	enable_paging();
}
