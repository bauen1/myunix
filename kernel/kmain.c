/* headers supplied by the c compiler */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* arch specific headers */
#include <boot.h>
#include <cpu.h>

/* all other headers */
#include <console.h>
#include <framebuffer.h>
#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <multiboot.h>
#include <pic.h>
#include <pit.h>
#include <pmm.h>
#include <syscall.h>
#include <tty.h>
#include <vmm.h>

/* main kernel entry point */
void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	(void)esp; /* unused, temporary stack */

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
		} else if (mbi->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED) {
			printf("indexed framebuffer, no supporting driver, trying tty anyway.\n");
			// try to init anyway
			tty_init((uintptr_t)mbi->framebuffer_addr, mbi->framebuffer_width, mbi->framebuffer_height, mbi->framebuffer_pitch, mbi->framebuffer_bpp);
			printf("indexed framebuffer\n");
			printf(" addr: 0x%x%08x\n", (uint32_t)(mbi->framebuffer_addr >> 32), (uint32_t)(mbi->framebuffer_addr && 0xFFFFFFFF));
		} else {
			printf("unknown framebuffer type: 0x%x\n", mbi->framebuffer_type);
			halt();
		}
	} else {
		tty_init((uintptr_t)0xb8000, 80, 25, 16, 80*2);
		printf("no framebuffer found assuming default i386 vga text mode\n");
	}


	gdt_init();
	printf("[%u] [OK] gdt_init\n", (unsigned int)ticks);

	pic_init();
	printf("[%u] [OK] pic_init\n", (unsigned int)ticks);

	idt_install();
	printf("[%u] [OK] idt_install\n", (unsigned int)ticks);

	isr_init();
	printf("[%u] [OK] isr_init\n", (unsigned int)ticks);

	pit_init();
	printf("[%u] [OK] pit_init\n", (unsigned int)ticks);

	__asm__ __volatile__ ("sti");
	printf("[%u] [OK] sti\n", (unsigned int)ticks);

	if (mbi->flags && MULTIBOOT_INFO_MEMORY) {
		printf("mem_lower: %ukb\n", mbi->mem_lower);
		printf("mem_upper: %ukb\n", mbi->mem_upper);
	} else {
		printf("mem_* not provided by bootloader, kernel init impossible.!\n");
		halt();
	}

	if (mbi->flags && MULTIBOOT_INFO_MODS) {
		printf("[%u] %u modules\n", (unsigned int)ticks, mbi->mods_count);
		if (mbi->mods_count > 0) {
			// we have modules
			multiboot_module_t *mods = (multiboot_module_t *)mbi->mods_addr;
			for (unsigned int i = 0; i < mbi->mods_count; i++) {
				printf("mods[%i].mod_start: 0x%x\n", i, mods[i].mod_start);
				printf("mods[%i].mod_end: 0x%x\n", i, mods[i].mod_end);
				if (mods[i].cmdline != 0) {
					printf("mods[%i].cmdline: '%s'\n", i, (char *)mods[i].cmdline);
				}
				printf("mods[%i].mod_add: 0x%x\n", i, mods[i].pad);
			}
		}
	}

	if (mbi->flags && MULTIBOOT_INFO_CMDLINE) {
		printf("[%u] cmdline: '%s'\n", (unsigned int)ticks, (char *)mbi->cmdline);
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
				for (uintptr_t i = 0; i < mmap->len; i += BLOCK_SIZE) {
					pmm_unset_block((mmap->addr + i) / BLOCK_SIZE);
				}
			} else if (mmap->type == MULTIBOOT_MEMORY_RESERVED) {
				printf("reserved\n");
				for (uintptr_t i = 0; i < mmap->len; i += BLOCK_SIZE) {
					pmm_set_block((mmap->addr + i) / BLOCK_SIZE);
				}
			} else if (mmap->type == 0x03) {
				printf("acpi reclaimable\n");
				for (uintptr_t i = 0; i < mmap->len; i += BLOCK_SIZE) {
					pmm_set_block((mmap->addr + i) / BLOCK_SIZE);
				}
			} else if (mmap->type == 0x04) {
				printf("nvs\n");
				for (uintptr_t i = 0; i < mmap->len; i += BLOCK_SIZE) {
					pmm_set_block((mmap->addr + i) / BLOCK_SIZE);
				}
			} else if (mmap->type == 0x05) {
				printf("bad ram\n");
				for (uintptr_t i = 0; i < mmap->len; i += BLOCK_SIZE) {
					pmm_set_block((mmap->addr + i) / BLOCK_SIZE);
				}
			} else if (mmap->type == 0) {
				/* something went wrong */
				break;
			} else {
				printf("unknown (type: 0x%x)\n", mmap->type);
				for (uintptr_t i = 0; i < mmap->len; i += BLOCK_SIZE) {
					pmm_set_block((mmap->addr + i) / BLOCK_SIZE);
				}
			}
		}
	}

	printf("0x%x free blocks\n", pmm_count_free_blocks());

	// mark the kernel as used
	for (uintptr_t i = (uintptr_t)&_start & 0xFFFFF000; i < (uintptr_t)&real_end; i += 0x1000) {
		pmm_set_block((i)/BLOCK_SIZE);
	}

	printf("after marking kernel 0x%x free blocks\n", pmm_count_free_blocks());

	vmm_init();
	printf("[%u] [OK] vmm_init\n", (unsigned int)ticks);

	/* block zero is for special purpose */
	pmm_set_block(0);

	syscall_init();

	vmm_enable();
	printf("[%u] [OK] vmm_enable\n", (unsigned int)ticks);

	puts("looping forever...\n");
	for (;;) {
		putc(getc());
	}

	halt();
}
