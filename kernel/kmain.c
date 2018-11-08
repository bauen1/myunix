/* headers supplied by the c compiler */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* arch specific headers */
#include <boot.h>
#include <cpu.h>

/* all other headers */
#include <console.h>
#include <dev_null.h>
#include <framebuffer.h>
#include <fs.h>
#include <gdt.h>
#include <heap.h>
#include <idt.h>
#include <isr.h>
#include <irq.h>
#include <kernel_task.h>
#include <keyboard.h>
#include <module.h>
#include <multiboot.h>
#include <pci.h>
#include <pic.h>
#include <pit.h>
#include <pmm.h>
#include <process.h>
#include <ramdisk.h>
#include <string.h>
#include <syscall.h>
#include <tar.h>
#include <tmpfs.h>
#include <tty.h>
#include <vmm.h>


static void kmain_ls(const char *path) {
	assert(path != NULL);
	printf("> %s '%s'\n", __func__, path);

	fs_node_t *dir = kopen(path, 0);
	if (dir == NULL) {
		printf("directory %s not found!\n\n", path);
		return;
	}

	unsigned int i = 0;
	do {
		struct dirent *v = fs_readdir(dir, i);
		if (v == NULL) {
			break;
		}
		assert(v->ino == i);
		printf("%u: %s\n", v->ino, v->name);
		kfree(v);
		i++;
	} while(true);
	printf("\n");
	fs_close(&dir);
}

// TODO: move the multiboot specific stuff to it's own file
/* main kernel entry point */
void __attribute__((used)) kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	(void)esp;
	uintptr_t mem_avail = 0;
	uintptr_t real_end = (uintptr_t)&_end;
	uintptr_t fb_start = 0;
	uintptr_t fb_size = 0;

	assert(eax == MULTIBOOT_BOOTLOADER_MAGIC);

	console_init();
	printf("early console init!\n");

	if ((mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) && (mbi->framebuffer_addr != 0)) {
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
			printf(" addr: 0x%x%08x\n", (uint32_t)(mbi->framebuffer_addr >> 32), (uint32_t)(mbi->framebuffer_addr & 0xFFFFFFFF));
		} else {
			printf("unknown framebuffer type: 0x%x\n", mbi->framebuffer_type);
			halt();
		}
	} else {
		printf("no framebuffer found, trying to initialize i386 vga text mode\n");
		fb_start = TTY_DEFAULT_VMEM_ADDR;
		fb_size = TTY_DEFAULT_HEIGHT * 2 * TTY_DEFAULT_WIDTH;
		tty_init((uintptr_t)fb_start,
			TTY_DEFAULT_WIDTH,
			TTY_DEFAULT_HEIGHT,
			8 * 2,
			TTY_DEFAULT_WIDTH * 2);
	}

	multiboot_dump(mbi);

	gdt_init();
	printf("[%u] [OK] gdt_init\n", (unsigned int)timer_ticks);

	idt_install();
	printf("[%u] [OK] idt_install\n", (unsigned int)timer_ticks);

	isr_init();
	printf("[%u] [OK] isr_init\n", (unsigned int)timer_ticks);

	pic_init();
	printf("[%u] [OK] pic_init\n", (unsigned int)timer_ticks);

	irq_init();
	printf("[%u] [OK] irq_init\n", (unsigned int)timer_ticks);

	pit_init();
	printf("[%u] [OK] pit_init\n", (unsigned int)timer_ticks);

	if (mbi->flags & MULTIBOOT_INFO_MODS) {
		printf("[%u] %u modules\n", (unsigned int)timer_ticks, mbi->mods_count);
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
				assert(mod_start <= mod_end);
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

	if (mbi->flags & MULTIBOOT_INFO_CMDLINE) {
		printf("[%u] cmdline: '%s'\n", (unsigned int)timer_ticks, (char *)mbi->cmdline);
	}

	real_end = (real_end+0xFFF) & 0xFFFFF000;
	printf("kernel mem starts at:      0x%x\n", (uintptr_t)&_start);
	printf("kernel mem ends at:        0x%x\n", (uintptr_t)&_end);
	printf("kernel mem really ends at: 0x%x\n", real_end);

	if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
		uintptr_t mem_avail_old = mem_avail;
		for (multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbi->mmap_addr;
			((uint32_t)mmap) < (mbi->mmap_addr + mbi->mmap_length);
			mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size + sizeof(mmap->size))) {
			if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
				uintptr_t mmap_end = mmap->addr + mmap->len;
				if (mmap_end > mem_avail) {
					mem_avail = mmap->addr + mmap->len;
				}
			}
		}
		if (mem_avail != mem_avail_old) {
			printf("mem_avail_old: 0x%x; mem_avail: 0x%x\n", mem_avail_old, mem_avail);
		}
	} else if (mbi->flags & MULTIBOOT_INFO_MEMORY) {
		printf("Memory map not available, falling back to mem_upper\n");
		mem_avail = 1024 * (1024 + mbi->mem_upper);
	}

	// overflow or no value
	if (mem_avail == 0) {
		mem_avail = 0xFFFFFFFF;
	}
	pmm_init((void *)real_end, mem_avail);
	printf("[%u] [OK] pmm_init\n", (unsigned int)timer_ticks);

	if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
		for (multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbi->mmap_addr;
			((uint32_t)mmap) < (mbi->mmap_addr + mbi->mmap_length);
			mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size + sizeof(mmap->size))) {
			printf("memory addr: 0x%x%8x len: 0x%x%8x ",
				(uint32_t)(mmap->addr >> 32),
				(uint32_t)(mmap->addr & 0xFFFFFFFF),
				(uint32_t)(mmap->len >> 32),
				(uint32_t)(mmap->len & 0xFFFFFFFF)
				);
			if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
				if ((uint32_t)(mmap->addr >> 32) != 0) {
					printf("available but out of 32bit range!\n");
					continue;
				} else {
					printf("available\n");
				}
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

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);

	// mark the kernel (and modules) as used
	assert(((uintptr_t)&_start & 0xFFF) == 0);
	for (uintptr_t i = (uintptr_t)&_start; i < real_end; i += BLOCK_SIZE) {
		pmm_set_block((i) / BLOCK_SIZE);
	}

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);

	printf("pmm block_map: 0x%x - 0x%x\n", (uintptr_t)block_map,
		((uintptr_t)block_map + block_map_size / 32));

	for (uintptr_t i = 0; i < (block_map_size / 32); i += BLOCK_SIZE) {
		pmm_set_block(((uintptr_t)block_map + i) / BLOCK_SIZE);
	}

	// special purpose
	pmm_set_block(0);

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);

	// TODO: copy everything of interest out of the multiboot info to a known, safe location
	// TODO: remember to free information once its no longer needed
	printf("set 0x%8x: multiboot info\n", (uintptr_t)mbi);
	pmm_set_block(((uintptr_t)mbi) / BLOCK_SIZE);
	if (mbi->flags & MULTIBOOT_INFO_CMDLINE) {
		if (mbi->cmdline != 0) {
			for (uintptr_t i = (uintptr_t)mbi->cmdline;
				i < ((uintptr_t)mbi->cmdline + (uintptr_t)strlen((char *)mbi->cmdline));
				i += BLOCK_SIZE) {
				printf("set 0x%8x: cmdline\n", i);
				pmm_set_block(i / BLOCK_SIZE);
			}
		}
	}
	if (mbi->flags & MULTIBOOT_INFO_MODS) {
		multiboot_module_t *mods = (multiboot_module_t *)mbi->mods_addr;
		for (unsigned int i = 0; i < mbi->mods_count; i++) {
			uintptr_t mods_i_block = (uintptr_t)(&mods[i]) / BLOCK_SIZE;
			printf("set 0x%8x: modinfo[%u]\n", mods_i_block, i);
			pmm_set_block(mods_i_block);
			if (mods[i].cmdline != 0) {
				for (uintptr_t j = (uintptr_t)mods[i].cmdline;
					j < (uintptr_t)strlen((char *)mods[i].cmdline);
					j += BLOCK_SIZE) {
					uintptr_t v_addr = mods[i].cmdline + j;
					printf("set 0x%8x: modinfo[%u].cmdline\n", v_addr, i);
					pmm_set_block(v_addr / BLOCK_SIZE);
				}
			}

			printf("set 0x%8x - 0x%8x: mod\n", mods[i].mod_start, mods[i].mod_end);

			assert(mods[i].mod_start <= mods[i].mod_end);
			for (uintptr_t v = mods[i].mod_start; v <= mods[i].mod_end; v += BLOCK_SIZE) {
				pmm_set_block(v / BLOCK_SIZE);
			}
		}
	}
	if (mbi->flags & MULTIBOOT_INFO_AOUT_SYMS) {
		// TODO: implement (if needed)
	} else if (mbi->flags & MULTIBOOT_INFO_ELF_SHDR) {
		// TODO: implement
	}
	if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
		for (uintptr_t i = (uintptr_t)mbi->mmap_addr;
			i < ((uintptr_t)mbi->mmap_addr + (uintptr_t)mbi->mmap_length);
			i += BLOCK_SIZE) {
			printf("set 0x%8x: mmap\n", i);
			pmm_set_block(i / BLOCK_SIZE);
		}
	}
	if (mbi->flags & MULTIBOOT_INFO_CONFIG_TABLE) {
		// TODO: implement (useless?)
	}
	if (mbi->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
		for (uintptr_t i = (uintptr_t)mbi->boot_loader_name;
			i < ((uintptr_t)mbi->boot_loader_name + (uintptr_t)strlen((char *)mbi->boot_loader_name));
			i += BLOCK_SIZE) {
			printf("set 0x%8x: bootloader name\n", i);
			pmm_set_block(i / BLOCK_SIZE);
		}
	}
	if (mbi->flags & MULTIBOOT_INFO_APM_TABLE) {
		// TODO: implement (if needed)
	}

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);
	// you can use pmm_alloc_* atfer here
	vmm_init();
	printf("[%u] [OK] vmm_init\n", (unsigned int)timer_ticks);
	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);

	// directly map the multiboot structure
	map_direct_kernel(((uintptr_t)mbi) & ~0xFFF);
	if (mbi->flags & MULTIBOOT_INFO_CMDLINE) {
		if (mbi->cmdline != 0) {
			for (uintptr_t i = (uintptr_t)mbi->cmdline;
				i < ((uintptr_t)mbi->cmdline + (uintptr_t)strlen((char *)mbi->cmdline));
				i += BLOCK_SIZE) {
				map_direct_kernel(i & ~0xFFF);
			}
		}
	}
	if (mbi->flags & MULTIBOOT_INFO_MODS) {
		multiboot_module_t *mods = (multiboot_module_t *)mbi->mods_addr;
		for (unsigned int i = 0; i < mbi->mods_count; i++) {
			map_direct_kernel(((uintptr_t)(&mods[i])) & ~0xFFF);
			if (mods[i].cmdline != 0) {
				for (uintptr_t j = (uintptr_t)mods[i].cmdline;
					j < (uintptr_t)strlen((char *)mods[i].cmdline);
					j += BLOCK_SIZE) {
					uintptr_t v_addr = mods[i].cmdline + j;
					map_direct_kernel(v_addr & ~0xFFF);
				}
			}

			map_pages(mods[i].mod_start, mods[i].mod_end, PAGE_PRESENT, "mod");
		}
	}
	if (mbi->flags & MULTIBOOT_INFO_AOUT_SYMS) {
		// TODO: implement
	} else if (mbi->flags & MULTIBOOT_INFO_ELF_SHDR) {
		// TODO: implement
	}
	if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
		for (uintptr_t i = (uintptr_t)mbi->mmap_addr;
			i < ((uintptr_t)mbi->mmap_addr + (uintptr_t)mbi->mmap_length);
			i += BLOCK_SIZE) {
			map_direct_kernel(i & ~0xFFF);
		}
	}
	if (mbi->flags & MULTIBOOT_INFO_CONFIG_TABLE) {
		// TODO: implement (useless?)
	}
	if (mbi->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
		for (uintptr_t i = (uintptr_t)mbi->boot_loader_name;
			i < ((uintptr_t)mbi->boot_loader_name + (uintptr_t)strlen((char *)mbi->boot_loader_name));
			i += BLOCK_SIZE) {
			map_direct_kernel(i & ~0xFFF);
		}
	}
	if (mbi->flags & MULTIBOOT_INFO_APM_TABLE) {
		// TODO: implement
	}

	// FIXME: we're not mapping the isrs section, yet it still somehow works, this is a disaster waiting to happen
	/* map the code section read-only */
	map_pages((uintptr_t)&__text_start, (uintptr_t)&__text_end,
		PAGE_PRESENT,                        ".text      ");

	/* map the data section read-write */
	map_pages((uintptr_t)&__data_start, (uintptr_t)&__data_end,
		PAGE_PRESENT | PAGE_READWRITE, ".data      ");

	/* map the bss section read-write */
	map_pages((uintptr_t)&__bss_start,  (uintptr_t)&__bss_end,
		PAGE_PRESENT | PAGE_READWRITE, ".bss       ");

	/* these mappings may overwrite the mappings above */

	/* map the shared user section read-write */
	map_pages((uintptr_t)&__start_user_shared, (uintptr_t)&__stop_user_shared,
		PAGE_PRESENT | PAGE_READWRITE, "user_shared");

	map_pages((uintptr_t)&__start_mod_info, (uintptr_t)&__stop_mod_info,
		PAGE_PRESENT, "mod_info   ");

	/* directly map the pmm block map */
	map_pages((uintptr_t)block_map, (uintptr_t)block_map + (block_map_size / 32), PAGE_PRESENT | PAGE_READWRITE, "pmm_map    ");

	/* map the framebuffer / textbuffer */
	if ((fb_start != 0) & (fb_size != 0)) {
		map_pages(fb_start, (fb_start + fb_size), PAGE_PRESENT | PAGE_READWRITE, "framebuffer");
	} else {
		printf("no framebuffer found, not mapping\n");
	}

	/* enable paging */
	vmm_enable();
	printf("[%u] [OK] vmm_enable\n", (unsigned int)timer_ticks);

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);

	liballoc_init();
	printf("[%u] [OK] liballoc_init\n", (unsigned int)timer_ticks);

	syscall_init();
	printf("[%u] [OK] syscall_init\n", (unsigned int)timer_ticks);

	framebuffer_enable_double_buffer();
	printf("[%u] [OK] tripple framebuffer enabled\n", (unsigned int)timer_ticks);

	keyboard_init();
	printf("[%u] [OK] keyboard_init\n", (unsigned int)timer_ticks);

	/* enable interrupts */
	interrupts_enable();
	printf("[%u] [OK] enable interrupts\n", (unsigned int)timer_ticks);

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);

	/* scan for device and initialise them */
	pci_print_all();

	/* initialize all drivers */
	modules_init();

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);
	assert(mbi->flags & MULTIBOOT_INFO_MODS);
	printf("we have modules!\n");

	fs_node_t *ramdisk = NULL;
	if (mbi->mods_count > 0) {
		// we have modules
		multiboot_module_t *mods = (multiboot_module_t *)mbi->mods_addr;
		for (unsigned int i = 0; i < mbi->mods_count; i++) {
			uintptr_t mod_start = mods[i].mod_start;
			uintptr_t mod_end = mods[i].mod_end;
			char *s = (char *)mods[i].cmdline;
			unsigned int type = 0; // 0: random program; 1: ramdisk
			if (s != NULL) {
				size_t j = 0;
				while (s[j]) {
					if (s[j] == ' ') {
						break;
					} else if (s[j] == 'i') { // FIXME: breaks for /hello/initrd/test_program
						if (memcmp((void *)(s + j), (void *)"initrd", 6) == 0) {
							type = 1;
							break;
						}
					} else if (s[j] == 'I') { // FIXME: ^
						if (memcmp((void *)(s + j), (void *)"INITRD", 6) == 0) {
							type = 1;
							break;
						}
					}
					j++;
				}
			} else {
				type = 0;
			}

			switch(type) {
				case 1:
					printf("ramdisk at 0x%x, length 0x%x\n", mod_start, mod_end-mod_start);
					ramdisk = ramdisk_init(mod_start, mod_end-mod_start);
					break;
				default:
					break;
			}
		}
	}

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);
	if (ramdisk == NULL) {
		printf("no ramdisk found! unable to continue !\n");
		halt();
	}

	process_init();

	printf("free %u kb\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);
	fs_node_t *tar_root = mount_tar(ramdisk);
	assert(tar_root != NULL);
	fs_mount_root(tar_root);

	fs_mount_t *tmpfs_mount = fs_mount_submount(fs_root_mount, mount_tmpfs(), "tmp");
	(void)tmpfs_mount;

	{
		// test if mount was successful and kopen works
		fs_node_t *dir = kopen("/tmp", 0);
		assert(dir != NULL);
		assert(dir == fs_root_mount->mounts[0]->node);
		fs_create(dir, "test22", 0);
		fs_node_t *file = kopen("/tmp/test22", 0);
		assert(file != NULL);
		char *str = "hello world!\n";
		fs_write(file, 0, strlen(str), str);
		fs_close(&dir);
		fs_close(&file);

	}

	kmain_ls("/");
	kmain_ls("/test_dir");
	kmain_ls("/test/../");
	kmain_ls("/tinycc");
	kmain_ls("/tinycc/bin");

	{
		fs_node_t *test = kopen("/tmp/abc/file_1", 0);
		if (test != NULL) {
			printf("Found file!!!\n");
			printf("file name: '%s'\n", test->name);
			fs_close(&test);
		}

		kmain_ls("/tmp/abc");
	}

	printf("%s: exec('/init')\n", __func__);
	{
		fs_node_t *f = kopen("/init", 0);
		if (f == NULL) {
			printf("could not find init!\n");
			assert(0);
		}
		char * const argv[2] = { "init", NULL };
		process_t *p = process_spawn_init(f, 1, argv);
		fs_close(&f); // this should free fs_node(init)
		p->pid = 1;
		p->name = strndup(argv[0], strlen(argv[0]));
		assert(p->name != NULL);
		process_add(p);
	}
	printf("%s: done\n", __func__);

	printf("%u kb free\n", pmm_count_free_blocks() * BLOCK_SIZE / 1024);
	// TODO: free anything left lying around that won't be needed (eg. multiboot info)

	return tasking_enable();
}
