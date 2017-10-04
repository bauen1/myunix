#include <stdint.h>

#include <assert.h>
#include <boot.h>
#include <cpu.h>
#include <multiboot.h>
#include <tty.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	if (eax != MULTIBOOT_BOOTLOADER_MAGIC) {
		halt();
	}
	*((uint32_t *)0xb8000) = 0x414f414f;

	tty_move_cursor(0, 0);

	halt();
}
