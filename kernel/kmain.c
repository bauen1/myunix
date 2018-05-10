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
#include <fs.h>
#include <gdt.h>
#include <heap.h>
#include <idt.h>
#include <isr.h>
#include <keyboard.h>
#include <multiboot.h>
#include <pic.h>
#include <pit.h>
#include <pmm.h>
#include <process.h>
#include <ramdisk.h>
#include <string.h>
#include <syscall.h>
#include <tar.h>
#include <tty.h>
#include <vmm.h>

/* main kernel entry point */
__attribute__((used))
void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	uintptr_t mem_avail = 0;
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

	syscall_init();
	printf("[%u] [OK] syscall_init\n", (unsigned int)ticks);

	keyboard_init();
	printf("[%u] [OK] keyboard_init\n", (unsigned int)ticks);

	__asm__ __volatile__ ("sti");
	printf("[%u] [OK] sti\n", (unsigned int)ticks);

	if (mbi->flags && MULTIBOOT_INFO_MEMORY) {
		printf("mem_lower: %ukb\n", mbi->mem_lower);
		printf("mem_upper: %ukb\n", mbi->mem_upper);
		mem_avail = 1024 * (1024 + mbi->mem_upper);
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

	printf("kernel mem starts at:          0x%x\n", (uintptr_t)&_start);
	printf("kernel mem ends at:            0x%x\n", (uintptr_t)&_end);
	printf("kernel mem really ends at:     0x%x\n", real_end);

	real_end = (real_end+0xFFF) & 0xFFFFF000;
	printf("kernel mem really ends at (2): 0x%x\n", real_end);

	// TODO: calulate the highest end of free memory and pass it to pmm_init instead of mem_lower and mem_upper
	// overflow or no value
	if (mem_avail == 0) {
		mem_avail = 0xFFFFFFFF;
	}
	pmm_init((void *)real_end, mem_avail);
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
			} else if (mmap->type == 0x03) {
				printf("acpi reclaimable\n");
			} else if (mmap->type == 0x04) {
				printf("nvs\n");
			} else if (mmap->type == 0x05) {
				printf("bad ram\n");
			} else if (mmap->type == 0) {
				/* something went wrong */
				break;
			} else {
				printf("unknown (type: 0x%x)\n", mmap->type);
			}
		}
	} else {
		printf("multiboot didn't provide a memory map, can't continue\n");
		halt();
	}

	printf("blocks: %u\n", pmm_count_free_blocks());

	// mark the kernel (and modules) as used
	for (uintptr_t i = (uintptr_t)&_start & 0xFFFFF000; i < real_end; i += 0x1000) {
		pmm_set_block((i)/BLOCK_SIZE);
	}
	printf("free memory: %ukb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);

	printf("pmm block_map: 0x%x - 0x%x\n", (uintptr_t)block_map, ((uintptr_t)block_map + block_map_size / 8));

	for (uintptr_t i = (uintptr_t)block_map; i < (uintptr_t)((uintptr_t)block_map + block_map_size/8); i += BLOCK_SIZE) {
		printf("pmm block_map: 0x%x\n", i);
		pmm_set_block((i)/BLOCK_SIZE);
	}

	// special purpose
	pmm_set_block(0);

	// TODO: copy everything of intrest out of the multiboot info to a known, safe location
	// TODO: remember to free information once its no longer needed
	printf("set 0x%x: multiboot info\n", (uintptr_t)mbi);
	pmm_set_block(((uintptr_t)mbi)/BLOCK_SIZE);
	if (mbi->flags && MULTIBOOT_INFO_CMDLINE) {
		for (uintptr_t i = (uintptr_t)mbi->cmdline;
			i < ((uintptr_t)mbi->cmdline + (uintptr_t)strlen((char *)mbi->cmdline));
			i += BLOCK_SIZE) {
			printf("set 0x%x: cmdline\n", i);
			pmm_set_block(i / BLOCK_SIZE);
			map_direct_kernel(i);
		}
	}
	if (mbi->flags && MULTIBOOT_INFO_MODS) {
		for (uintptr_t i = (uintptr_t)mbi->mods_addr;
			i < ((uintptr_t)mbi->mods_addr + ((uintptr_t)mbi->mods_count * sizeof(multiboot_module_t)));
			i += BLOCK_SIZE) {
			printf("set 0x%x: modinfo\n", i);
			pmm_set_block(i / BLOCK_SIZE);
			map_direct_kernel(i);
		}
	}
	if (mbi->flags && MULTIBOOT_INFO_AOUT_SYMS) { // TODO: implement
	} else if (mbi->flags && MULTIBOOT_INFO_ELF_SHDR) { // TODO: implement
	}
	if (mbi->flags && MULTIBOOT_INFO_MEM_MAP) { // TODO: implement
		for (uintptr_t i = (uintptr_t)mbi->mmap_length;
			i < ((uintptr_t)mbi->mmap_addr + (uintptr_t)mbi->mmap_length);
			i += BLOCK_SIZE) {
			printf("set 0x%x: mmap\n", i);
			pmm_set_block(i / BLOCK_SIZE);
			map_direct_kernel(i);
		}
	}
	if (mbi->flags && MULTIBOOT_INFO_CONFIG_TABLE) {} // TODO: implement (useless?)
	if (mbi->flags && MULTIBOOT_INFO_BOOT_LOADER_NAME) {
		for (uintptr_t i = (uintptr_t)mbi->boot_loader_name;
			i < ((uintptr_t)mbi->boot_loader_name + (uintptr_t)strlen((char *)mbi->boot_loader_name));
			i += BLOCK_SIZE) {
			printf("set 0x%x: bootloader name\n", i);
			pmm_set_block(i / BLOCK_SIZE);
			map_direct_kernel(i);
		}
	}
	if (mbi->flags && MULTIBOOT_INFO_APM_TABLE) {} // TODO: implement

	// you can use pmm_alloc_* atfer here

	/* map the complete kernel directly (including modules) read-only */
	map_pages(&_start, (void *)real_end, PAGE_TABLE_PRESENT, "kern");

	/* map the code section read-only */
	map_pages(&__text_start, &__text_end, PAGE_TABLE_PRESENT, ".text");

	/* map the data section read-write */
	map_pages(&__data_start, &__data_end, PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE, ".data");

	/* map the bss section read-write */
	map_pages(&__bss_start, &__bss_end, PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE, ".bss");

	/* directly map the pmm block map */
	printf("pmm block_map: 0x%x - 0x%x\n", (uintptr_t)block_map, ((uintptr_t)block_map + block_map_size / 8));
	map_pages(block_map,
		(void *)(
			(uintptr_t)block_map + (block_map_size / 8)
		),
		PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE, "pmm_map");

	/* map the framebuffer / textbuffer */
	if ((fb_start != 0) && (fb_size != 0)) {
		map_pages((void *)fb_start, (void *)(fb_start + fb_size), PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE, "framebuffer");
//		map_pages((void *)fb_start, (void *)(fb_start + fb_size), PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE | PAGE_TABLE_CACHE_DISABLE, "framebuffer");
	}

	/* enable paging */
	vmm_enable();
	printf("[%u] [OK] vmm_enable\n", (unsigned int)ticks);

	liballoc_init();

	assert(mbi->flags && MULTIBOOT_INFO_MODS);

	process_add(kidle_init(esp));

	fs_node_t *ramdisk = NULL;
	if (mbi->mods_count > 0) {
		// we have modules
		multiboot_module_t *mods = (multiboot_module_t *)mbi->mods_addr;
		for (unsigned int i = 0; i < mbi->mods_count; i++) {
			uintptr_t mod_start = mods[i].mod_start;
			uintptr_t mod_end = mods[i].mod_end;
			if ((*(char *)mod_start) == 't') {
				printf("ramdisk at 0x%x, length 0x%x\n", mod_start, mod_end-mod_start);
				ramdisk = ramdisk_init(mod_start, mod_end-mod_start);
			} else {
				printf("process_init(mod[%u]) name='%s'\n", i, (char *)mods[i].cmdline);
				process_t *p = process_init(mod_start, mod_end);
				p->pid = i + 1;
				p->name = (char *)mods[i].cmdline;
				process_add(p);
			}
		}
	}

	process_enable();

	puts("looping forever...\n");
	for (;;) {
		putc(getc());
	}

	halt();
}
