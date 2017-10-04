#include <stdint.h>

#include <assert.h>
#include <boot.h>
#include <cpu.h>
#include <gdt.h>
#include <multiboot.h>
#include <pic.h>
#include <tty.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	if (eax != MULTIBOOT_BOOTLOADER_MAGIC) {
		halt();
	}

	gdt_init();

	pic_init();

	tty_clear_screen();

	*((uint32_t *)0xb8000) = 0x414f414f;

	tty_move_cursor(0, 0);

	halt();
}
