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

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	(void)esp;

	assert(eax == MULTIBOOT_BOOTLOADER_MAGIC);

	console_init();

	puts("Hello world - console api\n");

	gdt_init();
	printf("[%i] [OK] gdt_init\n", ticks);

	pic_init();
	printf("[%i] [OK] pic_init\n", ticks);

	idt_install();
	printf("[%i] [OK] idt_install\n", ticks);

	isr_init();
	printf("[%i] [OK] isr_init\n", ticks);

	pit_init();
	printf("[%i] [OK] pit_init\n", ticks);


	__asm__ __volatile__ ("sti");
	printf("[%i] [OK] sti\n", ticks);

	if (mbi->flags && 0x01) { // are mem_* valid ?
		printf("mem_lower: %ikb\n", mbi->mem_lower);
		printf("mem_upper: %ikb\n", mbi->mem_upper);
	}

	if (mbi->flags && 0x08) {
		printf("[%i] %i modules\n", ticks, mbi->mods_count);
		if (mbi->mods_count > 0) {
			// we have modules
		}
	}

	if (mbi->flags && 0x20) {
		multiboot_memory_map_t *mmap;
		printf("mbi->mmap_addr: 0x%x\n", mbi->mmap_addr);
		printf("mbi->mmap_length: 0x%x\n", mbi->mmap_length);
		for (mmap = (multiboot_memory_map_t *)mbi->mmap_addr;
			(uintptr_t)mmap < (mbi->mmap_addr + mbi->mmap_length);
			mmap = (multiboot_memory_map_t *)((uintptr_t)mmap + mmap->size + sizeof(mmap->size))) {
			printf(" addr: 0x%08x%08x len: 0x%08x%08x type:0x%x\n",
				(mmap->addr >> 32),
				mmap->addr & 0xFFFFFFFF,
				(mmap->len >> 32),
				mmap->len  & 0xFFFFFFFF,
				mmap->type);
		}
	}

	printf("[%i] int $0x80\n", ticks);
	__asm__ __volatile__ ("int $0x80");

	printf("[%i] cmdline: '%s'\n", ticks, (char *)mbi->cmdline);

	vmm_init();
	printf("[%i] [OK] vmm_init\n", ticks);

	puts("looping forever...\n");
	for (;;) {
		putc(getc());
	}

	halt();
}
