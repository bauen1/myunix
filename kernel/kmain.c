#include <stdint.h>
#include <stddef.h>

#include <assert.h>
#include <boot.h>
#include <cpu.h>

#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <multiboot.h>
#include <pic.h>
#include <pit.h>

#include <console.h>

#include <vmm.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	(void)esp;

	assert(eax == MULTIBOOT_BOOTLOADER_MAGIC);

	console_init();


	gdt_init();
	printf("[%i] [OK] gdt_init\n", (int)ticks);

	pic_init();
	printf("[%i] [OK] pic_init\n", (int)ticks);

	idt_install();
	printf("[%i] [OK] idt_install\n", (int)ticks);

	isr_init();
	printf("[%i] [OK] isr_init\n", (int)ticks);

	pit_init();
	printf("[%i] [OK] pit_init\n", (int)ticks);


	__asm__ __volatile__ ("sti");
	printf("[%i] [OK] sti\n", (int)ticks);

	if (mbi->flags && 0x01) { // are mem_* valid ?
		printf("mem_lower: %ikb\n", mbi->mem_lower);
		printf("mem_upper: %ikb\n", mbi->mem_upper);
	}

	if (mbi->flags && 0x08) {
		printf("[%i] %i modules\n", (int)ticks, mbi->mods_count);
		if (mbi->mods_count > 0) {
			// we have modules
		}
	}

	printf("[%i] int $0x80\n", (int)ticks);
	__asm__ __volatile__ ("int $0x80");

	printf("[%i] cmdline: '%s'\n", (int)ticks, (char *)mbi->cmdline);

	assert(mbi->flags && 0x01);
	if (mbi->flags && 0x20) {
		for (multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbi->mmap_addr;
			((uint32_t)mmap) < (mbi->mmap_addr + mbi->mmap_length);
			mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size + sizeof(mmap->size))) {
			printf("memory addr: 0x%x%08x len: 0x%x%08x ",
				(uint32_t)(mmap->addr >> 32),
				(uint32_t)(mmap->addr & 0xFFFFFFFF),
				(uint32_t)(mmap->len >> 32),
				(uint32_t)(mmap->len & 0xFFFFFFFF)
				);
			if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
				printf("available\n");
			} else if (mmap->type == MULTIBOOT_MEMORY_RESERVED) {
				printf("reserved\n");
			} else {
				printf("type: 0x%x\n", mmap->type);
			}
		}
	}

	vmm_init();



	vmm_enable();
	printf("[%i] [OK] vmm_enable\n", (int)ticks);

	puts("looping forever...\n");
	for (;;) {
		putc(getc());
	}

	halt();
}
