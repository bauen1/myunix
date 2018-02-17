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
	uintptr_t fb_start = 0;
	uintptr_t fb_size = 0;

	assert(eax == MULTIBOOT_BOOTLOADER_MAGIC);

	console_init();
	printf("early console init!\n");

	if ((mbi->flags && MULTIBOOT_INFO_FRAMEBUFFER_INFO) && (mbi->framebuffer_addr != 0)) {
		if (mbi->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
			fb_start = mbi->framebuffer_addr;
			fb_size = (mbi->framebuffer_height + 1) * mbi->framebuffer_pitch;
			tty_init((uintptr_t)mbi->framebuffer_addr,
				mbi->framebuffer_width,
				mbi->framebuffer_height,
				mbi->framebuffer_pitch,
				mbi->framebuffer_bpp);
		} else if (mbi->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
			fb_start = mbi->framebuffer_addr;
			fb_size = (mbi->framebuffer_height + 1) * mbi->framebuffer_pitch;
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
			fb_start = mbi->framebuffer_addr;
			fb_size = (mbi->framebuffer_height + 1) * mbi->framebuffer_pitch;
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
		fb_start = TTY_DEFAULT_VMEM_ADDR;
		fb_size = TTY_DEFAULT_HEIGHT * 2 * TTY_DEFAULT_WIDTH;
		tty_init((uintptr_t)fb_start,
			TTY_DEFAULT_WIDTH,
			TTY_DEFAULT_HEIGHT,
			8 * 2,
			TTY_DEFAULT_WIDTH * 2);
		printf("no framebuffer found assuming default i386 vga text mode\n");
	}

	printf("mbi->flags: 0x%x\n", (uint32_t)mbi->flags);

	if (mbi->flags && MULTIBOOT_INFO_BOOT_LOADER_NAME) {
		printf("mbi->boot_loader_name: '%s'\n", (char *)mbi->boot_loader_name);
	}

	if (mbi->flags && MULTIBOOT_INFO_ELF_SHDR) {
		printf("mbi->u.elf_sec.num: 0x%x\n", mbi->u.elf_sec.num);
		printf("mbi->u.elf_sec.size: 0x%x\n", mbi->u.elf_sec.size);
		printf("mbi->u.elf_sec.addr: 0x%x\n", mbi->u.elf_sec.addr);
		printf("mbi->u.elf_sec.shndx: 0x%x\n", mbi->u.elf_sec.shndx);
	}

	if (mbi->flags && MULTIBOOT_INFO_VIDEO_INFO) {
		printf("mbi->vbe_control_info: 0x%x\n", mbi->vbe_control_info);
		printf("mbi->vbe_mode_info: 0x%x\n", mbi->vbe_mode_info);
		printf("mbi->vbe_interface_seg: 0x%x\n", mbi->vbe_interface_seg);
		printf("mbi->vbe_interface_off: 0x%x\n", mbi->vbe_interface_off);
		printf("mbi->vbe_interface_len: 0x%x\n", mbi->vbe_interface_len);
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
				uintptr_t mod_start = mods[i].mod_start;
				uintptr_t mod_end = mods[i].mod_end;

				printf("mods[%u].mod_start: 0x%x\n", i, mods[i].mod_start);
				printf("mods[%u].mod_end: 0x%x\n", i, mods[i].mod_end);
				if (mods[i].cmdline != 0) {
					printf("mods[%u].cmdline: '%s'\n", i, (char *)mods[i].cmdline);
				}
				printf("mods[%u].mod_add: 0x%x\n", i, mods[i].pad);

				/* some sanity checks */
				assert(mod_start < mod_end);
				if (mod_start == mod_end) {
					printf("WARNING: module size = 0\n");
				}

				if (((uintptr_t)&mods[i] + sizeof(multiboot_module_t)) > real_end) {
					/* apparently some bootloaders like to put this behind the module */
					real_end = (uintptr_t)&mods[i] + sizeof(multiboot_module_t);
				}

				if (mod_end > real_end) {
					real_end = mod_end;
				}
			}
		}
	}

	if (mbi->flags && MULTIBOOT_INFO_CMDLINE) {
		printf("[%u] cmdline: '%s'\n", (unsigned int)ticks, (char *)mbi->cmdline);
	}

	printf("kernel mem starts at:      0x%x\n", (uintptr_t)&_start);
	printf("kernel mem really ends at: 0x%x\n", real_end);

	pmm_init((void *)real_end, mbi->mem_lower*1024 + mbi->mem_upper*1024);
	printf("[%u] [OK] pmm_init\n", (unsigned int)ticks);
	vmm_init();
	printf("[%u] [OK] vmm_init\n", (unsigned int)ticks);

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

	/* block zero is for special purpose */
	pmm_set_block(0);

	printf("blocks: %u\n", pmm_count_free_blocks());

	// mark the kernel as used
	for (uintptr_t i = (uintptr_t)&_start & 0xFFFFF000; i < (uintptr_t)&real_end; i += 0x1000) {
		pmm_set_block((i)/BLOCK_SIZE);
	}
	printf("free memory: %ukb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);




	syscall_init();

	vmm_enable();
	printf("[%u] [OK] vmm_enable\n", (unsigned int)ticks);

	puts("looping forever...\n");
	for (;;) {
		putc(getc());
	}

	halt();
}
