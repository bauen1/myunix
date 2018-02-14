#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include <boot.h>
#include <cpu.h>

#include <console.h>
#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <multiboot.h>
#include <pic.h>
#include <pit.h>
#include <pmm.h>
#include <syscall.h>
#include <vmm.h>
#include <tty.h>
#include <framebuffer.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	(void)esp;

	uintptr_t real_end = (uintptr_t)&_end;

	assert(eax == MULTIBOOT_BOOTLOADER_MAGIC);

	console_init();
	printf("early console init!\n");

	if ((mbi->flags && MULTIBOOT_INFO_FRAMEBUFFER_INFO) && (mbi->framebuffer_addr != 0)) {
		if (mbi->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
			tty_init((uintptr_t)mbi->framebuffer_addr,
				mbi->framebuffer_width,
				mbi->framebuffer_height,
				mbi->framebuffer_pitch,
				mbi->framebuffer_bpp);
			printf("tty framebuffer:\n");
			printf(" width: %i height: %i\n", mbi->framebuffer_width, mbi->framebuffer_height);
			printf(" framebuffer_bpp: 0x%x\n", mbi->framebuffer_bpp);
			printf(" bytes per text line: 0x%x\n", mbi->framebuffer_pitch);
		} else if (mbi->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
			framebuffer_init((uintptr_t)mbi->framebuffer_addr,
				mbi->framebuffer_pitch,
				mbi->framebuffer_width,
				mbi->framebuffer_height,
				mbi->framebuffer_bpp,
				mbi->framebuffer_red_field_position,
				mbi->framebuffer_red_mask_size,
				mbi->framebuffer_green_field_position,
				mbi->framebuffer_green_mask_size,
				mbi->framebuffer_blue_field_position,
				mbi->framebuffer_blue_mask_size);
			printf("VESA framebuffer\n");
			printf(" addr: 0x%x%08x\n", (uint32_t)(mbi->framebuffer_addr >> 32), (uint32_t)mbi->framebuffer_addr);
			printf(" pitch: 0x%x\n", (uint32_t)mbi->framebuffer_pitch);
			printf(" width: 0x%x\n", (uint32_t)mbi->framebuffer_width);
				printf(" height: 0x%x\n", (uint32_t)mbi->framebuffer_height);
			printf(" bpp: 0x%x\n", (uint32_t)mbi->framebuffer_bpp);
		} else if (mbi->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED) {
			printf("indexed framebuferr!!!!\n");
			halt();
		} else {
			printf("unknown framebuffer type: 0x%x\n", mbi->framebuffer_type);
			halt();
		}
	} else {
		tty_init((uintptr_t)0xb8000, 80, 25, 16, 80*2);
		printf("no framebuffer found assuming default i386 vga text mode\n");
	}


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

	if (mbi->flags && MULTIBOOT_INFO_MEMORY) {
		printf("mem_lower: %ikb\n", mbi->mem_lower);
		printf("mem_upper: %ikb\n", mbi->mem_upper);
	} else {
		printf("mem_* not provided by bootloader, kernel init impossible.!\n");
		halt();
	}

	if (mbi->flags && MULTIBOOT_INFO_MODS) {
		printf("[%i] %i modules\n", (int)ticks, mbi->mods_count);
		if (mbi->mods_count > 0) {
			// we have modules
		}
	}

	if (mbi->flags && MULTIBOOT_INFO_CMDLINE) {
		printf("[%i] cmdline: '%s'\n", (int)ticks, (char *)mbi->cmdline);
	}

	pmm_init((void *)real_end, mbi->mem_lower*1024 + mbi->mem_upper*1024);

	if (mbi->flags && MULTIBOOT_INFO_MEM_MAP) {
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
				for (uintptr_t i = 0; i < mmap->len; i += 0x1000) {
					pmm_unset_block(mmap->addr + i);
				}
			} else if (mmap->type == MULTIBOOT_MEMORY_RESERVED) {
				printf("reserved\n");
				for (uintptr_t i = 0; i < mmap->len; i += 0x1000) {
					pmm_set_block(mmap->addr + i);
				}
			} else {
				printf("type: 0x%x\n", mmap->type);
			}
		}
	}

	printf("0x%x free blocks\n", pmm_count_free_blocks());

	// mark the kernel as used
	for (uintptr_t i = (uintptr_t)&_start & 0xFFFFF000; i < (uintptr_t)&real_end; i += 0x1000) {
		pmm_set_block(i);
	}

	printf("0x%x free blocks\n", pmm_count_free_blocks());

	vmm_init();



	vmm_enable();
	printf("[%i] [OK] vmm_enable\n", (int)ticks);

	puts("looping forever...\n");
	for (;;) {
		putc(getc());
	}

	halt();
}
