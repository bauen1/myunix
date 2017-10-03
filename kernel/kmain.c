#include <stdint.h>

#include <assert.h>
#include <boot.h>
#include <multiboot.h>

void kmain(struct multiboot_info *mbi, uint32_t eax, uintptr_t esp) {
	*((uint32_t *)0xb8000) = 0x414f414f;
	_halt();
}
