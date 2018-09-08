#include <console.h>
#include <multiboot.h>

void multiboot_dump(struct multiboot_info *mbi) {
	printf("mbi->flags: 0x%x\n", mbi->flags);
	// TODO: parse flags
	printf("mbi->mem_lower: 0x%x\n", mbi->mem_lower);
	printf("mbi->mem_upper: 0x%x\n", mbi->mem_upper);
	printf("mbi->boot_device: 0x%x\n", mbi->boot_device);
	printf("mbi->cmdline: %p ('%s')\n", (uintptr_t)mbi->cmdline, (char *)mbi->cmdline);
	printf("mbi->mods_count: %u\n", mbi->mods_count);
	printf("mbi->mods_addr: %p\n", (uintptr_t)mbi->mods_addr);
	// TODO: union u
	printf("mbi->mmap_length: 0x%x\n", mbi->mmap_length);
	printf("mbi->mmap_addr: %p\n", (uintptr_t)mbi->mmap_addr);
	// TODO: implement the rest, parse memory map, etc ...

/*
	if (mbi->flags & MULTIBOOT_INFO_BOOT_LOADER_NAME) {
		printf("mbi->boot_loader_name: '%s'\n", (char *)mbi->boot_loader_name);
	}

	if (mbi->flags & MULTIBOOT_INFO_ELF_SHDR) {
		printf("mbi->u.elf_sec.num: 0x%x\n", mbi->u.elf_sec.num);
		printf("mbi->u.elf_sec.size: 0x%x\n", mbi->u.elf_sec.size);
		printf("mbi->u.elf_sec.addr: 0x%x\n", mbi->u.elf_sec.addr);
		printf("mbi->u.elf_sec.shndx: 0x%x\n", mbi->u.elf_sec.shndx);
	}

	if (mbi->flags & MULTIBOOT_INFO_VIDEO_INFO) {
		printf("mbi->vbe_control_info: 0x%x\n", mbi->vbe_control_info);
		printf("mbi->vbe_mode_info: 0x%x\n", mbi->vbe_mode_info);
		printf("mbi->vbe_interface_seg: 0x%x\n", mbi->vbe_interface_seg);
		printf("mbi->vbe_interface_off: 0x%x\n", mbi->vbe_interface_off);
		printf("mbi->vbe_interface_len: 0x%x\n", mbi->vbe_interface_len);
	}


	if (mbi->flags & MULTIBOOT_INFO_MEMORY) {
		printf("mem_lower: %ukb\n", mbi->mem_lower);
		printf("mem_upper: %ukb\n", mbi->mem_upper);
		mem_avail = 1024 * (1024 + mbi->mem_upper);
	}


*/
}
