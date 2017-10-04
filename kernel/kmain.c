#include <stdint.h>

#include <assert.h>
#include <boot.h>
#include <cpu.h>
#include <multiboot.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	if (eax != MULTIBOOT_BOOTLOADER_MAGIC) {
		halt();
	}
	*((uint32_t *)0xb8000) = 0x414f414f;

	// move the cursor to 0 0
	outb(0x3D4, 0x0F);
	outb(0x3D5, 0x00);
	outb(0x3D4, 0x0E);
	outb(0x3D5, 0x00);

	halt();
}
