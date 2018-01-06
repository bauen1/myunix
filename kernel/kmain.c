#include <stdint.h>

#include <assert.h>
#include <boot.h>
#include <cpu.h>
#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <multiboot.h>
#include <pic.h>
#include <pit.h>
#include <tty.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	if (eax != MULTIBOOT_BOOTLOADER_MAGIC) {
		halt();
	}

	tty_clear_screen();

	gdt_init();
	tty_puts("[OK] gdt_init\n");

	pic_init();
	tty_puts("[OK] pic_init\n");

	idt_install();
	tty_puts("[OK] idt_install\n");

	isr_init();
	tty_puts("[OK] isr_init\n");

	pit_init();
	tty_puts("[OK] pit_init\n");

//	keyboard_init();
//	tty_puts("[OK] keyboard_init\n");

	__asm__ __volatile__ ("sti");
	tty_puts("[OK] sti\n");

	tty_puts("int $0x80\n");
	__asm__ __volatile__ ("int $0x80");

	tty_puts("cmdline: '");
	tty_puts((char *)mbi->cmdline);
	tty_puts("'\n");

	tty_puts("looping forever...\n");
	for (;;) {
		__asm__ __volatile__ ("hlt");
	}

	halt();
}
