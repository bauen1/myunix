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

	tty_putchar('h');
	tty_putchar('i');
	tty_putchar('\n');
	tty_putchar('\r');

	tty_puts("hello world :)\n\r");

	halt();
}
