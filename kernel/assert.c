#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <console.h>
#include <cpu.h>
#include <vmm.h>

void __attribute__((noreturn)) __assert_failed(const char *exp, const char *file, int line) {
	printf("%s:%i assertion failed: '%s'\n", file, line, exp);
	print_stack_trace(-1);
	halt();
}

void __attribute__((noreturn)) __attribute__((used)) __stack_chk_fail() {
	printf("__stack_chk_fail!\n");
	print_stack_trace(-1); // should be safe since we only read
	halt();
}

static bool is_mapped(uintptr_t v_addr) {
	uintptr_t v = kernel_directory->tables[v_addr >> 22];
	if (!(v & PAGE_DIRECTORY_PRESENT)) {
		printf("PAGE TABLE NOT PRESENT!\n");
		return false;
	}

	page_table_t *table = get_table(v_addr, kernel_directory);
	page_t page = get_page(table, v_addr);
	if (!(page & PAGE_TABLE_PRESENT)) {
		printf("PAGE NOT MAPPED!\n");
		return false;
	}
	return true;
}

void print_stack_trace(unsigned int max_frames) {
	uintptr_t ebp_r = 0;
	__asm__ __volatile__ (
		"mov %%ebp, %0"
		: "=r" (ebp_r)
	);
	uintptr_t * ebp = (uintptr_t *)ebp_r;

	printf("stack trace:\n");
	for (unsigned int frame = 0; frame < max_frames; frame++) {
		if (!is_mapped((uintptr_t)(&ebp[0]))) {
			return;
		}
		if (!is_mapped((uintptr_t)&ebp[1])) {
			return;
		}
		uintptr_t eip = ebp[1];
		if ((ebp[0] == 0) || (eip == 0)) {
			printf("\n");
			return;
		}
		ebp = (uintptr_t *)ebp[0];
		if (!is_mapped((uintptr_t)(&ebp[0]))) {
			return;
		}
		printf(" eip: 0x%x (ebp: 0x%x)\n", eip, ebp[0]);
	}
}
