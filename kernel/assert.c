#include <assert.h>
#include <stddef.h>

#include <console.h>
#include <cpu.h>

__attribute__((noreturn))
void __assert_failed(const char *exp, const char *file, int line) {
	printf("%s:%i assertion failed: '%s'\n", file, line, exp);
	print_stack_trace(-1);
	halt();
}

__attribute__((used))
__attribute__((noreturn))
void __stack_chk_fail() {
	printf("__stack_chk_fail!\n");
//	print_stack_trace(-1); // TODO: this isn't safe, since values on the stack have been modified
	halt();
}

void print_stack_trace(unsigned int max_frames) {
	// FIXME: this can page fault
	uintptr_t ebp_r = 0;
	__asm__ __volatile__ (
		"mov %%ebp, %0"
		: "=r" (ebp_r)
	);
	uintptr_t * ebp = (uintptr_t *)ebp_r;

	printf("stack trace:\n");
	for (unsigned int frame = 0; frame < max_frames; frame++) {
		uintptr_t eip = ebp[1];
		if ((ebp[0] == 0) || (eip == 0)) {
			printf("\n");
			return;
		}
		ebp = (uintptr_t *)ebp[0];
		printf(" eip: 0x%x (ebp: 0x%x)\n", eip, ebp[0]);
	}
}
