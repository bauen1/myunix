#include <stdbool.h>
#include <stdint.h>

#include <console.h>
#include <cpu.h>
#include <vmm.h>

inline uint8_t inb(uint16_t port) {
	uint8_t data;
	__asm__ __volatile__("inb %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline uint16_t inw(uint16_t port) {
	uint16_t data;
	__asm__ __volatile__("inw %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline uint32_t inl(uint16_t port) {
	uint32_t data;
	__asm__ __volatile__("inl %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline void outb(uint16_t port, uint8_t data) {
	__asm__ __volatile__("outb %0, %1" : : "a" (data), "dN" (port));
}

inline void outw(uint16_t port, uint16_t data) {
	__asm__ __volatile__("outw %0, %1" : : "a" (data), "dN" (port));
}

inline void outl(uint16_t port, uint32_t data) {
	__asm__ __volatile__("outl %0, %1" : : "a" (data), "dN" (port));
}


inline void iowait() {
	__asm__ __volatile__("outb %0, $0x80" : : "a"(0));
}

inline __attribute__((noreturn)) void halt() {
	puts("halting the cpu\n");
	_halt();
}

void interrupts_disable(void) {
	__asm__ __volatile__("cli");
}

void interrupts_enable(void) {
	__asm__ __volatile__("sti");
}

void dump_regs(registers_t *regs) {
	printf("========== REGISTERS BEGIN ==========\n");
	printf("es:  0x%8x fs:  0x%8x\n", regs->es, regs->fs);
	printf("ds:  0x%8x cs:  0x%8x\n", regs->ds, regs->cs);
	printf("ss:  0x%8x           \n", regs->ss);
	printf("edi: 0x%8x esi: 0x%8x\n", regs->edi, regs->esi);
	printf("ebp: 0x%8x ebx: 0x%8x\n", regs->ebp, regs->ebx);
	printf("edx: 0x%8x ecx: 0x%8x\n", regs->edx, regs->ecx);
	printf("eax: 0x%8x           \n", regs->eax);
	printf("isr_num: 0x%8x\n", regs->isr_num);
	printf("err_code: 0x%8x\n", regs->err_code);
	printf("eip: 0x%8x eflags: 0x%8x\n", regs->eip, regs->eflags);
	printf("esp: 0x%8x old_directory: 0x%8x\n", regs->esp, regs->old_directory);
	printf("========== REGISTERS END   ==========\n");
}

static bool is_mapped(uintptr_t v_addr) {
	if (kernel_directory == NULL) {
		return true;
	} else {
		uintptr_t v = kernel_directory->physical_tables[v_addr >> 22];
		if (!(v & PAGE_TABLE_PRESENT)) {
			return false;
		} else {
			page_table_t *table = get_table(v_addr, kernel_directory);
			page_t page = get_page(table, v_addr);
			return (page & PAGE_PRESENT);
		}
	}
}

static bool is_mapped_uint32(uintptr_t addr) {
	bool success = true;
	for (size_t i = 0; i < sizeof(uint32_t); i++) {
		success &= is_mapped(addr + i);
	}
	return success;
}

// FIXME: Only works if ptr is aligned
static bool try_read_uint32(page_directory_t *pdir, uintptr_t ptr, uint32_t *out) {
	page_table_t *table = get_table(ptr, pdir);
	if (table == NULL) {
		printf("%s: table = NULL\n", __func__);
		return false;
	}
	page_t page = get_page(table, ptr);
	if (! (page & PAGE_PRESENT)) {
		printf("%s: not present!\n", __func__);
		return false;
	} else if (! (page & PAGE_USER)) {
		printf("%s: not user!\n", __func__);
		return false;
	} else {
		bool success;
		uintptr_t kaddr = find_vspace(kernel_directory, 1);
		if (kaddr == 0) {
			printf("%s: find_vspace: 0!\n", __func__);
			return false;
		}

		map_page(get_table(kaddr, kernel_directory), kaddr, page, PAGE_PRESENT);
		invalidate_page(kaddr);
		if (!is_mapped_uint32(kaddr)) {
			success = false;
			printf("%s: not mapped!\n", __func__);
		} else {
			success = true;
			*out = *(uint32_t *)(kaddr + (ptr & 0xFFF));
		}
		map_page(get_table(kaddr, kernel_directory), kaddr, 0, 0);
		invalidate_page(kaddr);
		return success;
	}
}

/* TODO: if possible parse eflags and mark any interrupt frames (and switches to/from userspace) */
__attribute__((no_sanitize_undefined)) void print_stack_trace(void) {
	uintptr_t ebp_r = 0;
	__asm__ __volatile__("mov %%ebp, %0" : "=r" (ebp_r));
	printf("========== STACKTRACE BEGIN =========\n");

	const uint32_t *ebp = (uint32_t *)ebp_r;
	for (unsigned int frame = 0; /**/ ; frame++) {
		printf(" [frame %4u]: ", frame);
		if (!is_mapped_uint32((uintptr_t)&ebp[0])) {
			printf(" -\n");
			break;
		}

		const uint32_t frame_ebp = ebp[0];
		printf("ebp: 0x%8x ", frame_ebp);
		if (!is_mapped_uint32((uintptr_t)&ebp[1])) {
			printf("-\n");
			break;
		}

		const uint32_t frame_eip = ebp[1];
		printf("eip: 0x%8x\n", frame_eip);

		if ((frame_ebp == 0) || (frame_eip == 0)) {
			printf("\n");
			break;
		} else {
			ebp = (uint32_t *)frame_ebp;
		}
	}

	printf("========== STACKTRACE END   =========\n");
}

void print_user_stack_trace(page_directory_t *pdir, registers_t *regs) {
	printf("========== USER STACKTRACE BEGIN ====\n");
	uintptr_t ptr = regs->ebp;
	for (unsigned int frame = 0; /**/ ; frame++) {
		printf(" [frame %4u]: ", frame);
		uint32_t frame_ebp;
		if (!try_read_uint32(pdir, ptr, &frame_ebp)) {
			printf(" -\n");
			break;
		} else {
			printf("ebp: 0x%8x ", frame_ebp);
		}
		uint32_t frame_eip;
		if (!try_read_uint32(pdir, ptr + sizeof(uint32_t) * 1, &frame_eip)) {
			printf(" -\n");
			break;
		} else {
			printf("eip: 0x%8x\n", frame_eip);
		}

		if ((frame_ebp == 0) || (frame_eip == 0)) {
			printf("\n");
			break;
		} else {
			ptr = frame_ebp;
		}
	}

	printf("========== USER STACKTRACE END   ====\n");
}
