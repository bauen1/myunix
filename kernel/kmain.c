#include <stdint.h>
#include <stddef.h>

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

#include <console.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	if (eax != MULTIBOOT_BOOTLOADER_MAGIC) {
		halt();
	}

	console_init();


	puts("Hello world - console api\n");

	gdt_init();
	puts("[OK] gdt_init\n");

	pic_init();
	puts("[OK] pic_init\n");

	idt_install();
	puts("[OK] idt_install\n");

	isr_init();
	puts("[OK] isr_init\n");

	pit_init();
	puts("[OK] pit_init\n");

//	keyboard_init();
//	puts("[OK] keyboard_init\n");

	__asm__ __volatile__ ("sti");
	puts("[OK] sti\n");

	puts("int $0x80\n");
	__asm__ __volatile__ ("int $0x80");

	puts("cmdline: '");
	puts((char *)mbi->cmdline);
	puts("'\n");

	puts("looping forever...\n");
	for (;;) {
		__asm__ __volatile__ ("hlt");
	}

	halt();
}
