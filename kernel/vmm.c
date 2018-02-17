#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <boot.h>
#include <console.h>
#include <cpu.h>
#include <isr.h>
#include <paging.h>
#include <vmm.h>

__attribute__((aligned(4096))) uint32_t page_directory[1024];
__attribute__((aligned(4096))) uint32_t page_tables[1024][1024];

void map_page(void *physaddr, void *virtaddr, uint16_t flags) {
	page_tables[(uint32_t)virtaddr >> 22][(uint32_t)virtaddr >> 12 & 0x03FF] = ((uint32_t)physaddr) | flags;
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

	for (int i = 0; i < 1024; i++) {
		page_directory[i] = ((uint32_t)page_tables[i]) | PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE | PAGE_TABLE_USER;
	}

	// catch NULL pointer derefrences
	map_page((void *)0, (void *)0, 0);
}

void vmm_enable() {
	load_page_directory((uint32_t)page_directory);
	enable_paging();
}
