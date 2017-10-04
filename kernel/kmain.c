#include <stdint.h>

#include <assert.h>
#include <boot.h>
#include <cpu.h>
#include <gdt.h>
#include <idt.h>
#include <multiboot.h>
#include <pic.h>
#include <tty.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	if (eax != MULTIBOOT_BOOTLOADER_MAGIC) {
		halt();
	}

	tty_clear_screen();

	gdt_init();
	tty_puts("[OK] gdt_init\n\r");

	pic_init();
	tty_puts("[OK] pic_init\n\r");

	idt_init();
	tty_puts("[OK] idt_init\n\r");

	__asm__ __volatile__ ("sti");
	tty_puts("[OK] sti\n\r");

	__asm__ __volatile__ ("int $0");

	tty_puts("hello world :)\n\r");
	tty_puts("cmdline: '");
	tty_puts((char *)mbi->cmdline);
	tty_puts("'\n\r");

	halt();
}
