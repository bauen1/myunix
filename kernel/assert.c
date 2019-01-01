#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <console.h>
#include <cpu.h>
#include <vmm.h>

void __attribute__((noreturn)) __assert_failed(const char *exp, const char *file, int line) {
	printf("%s:%i assertion failed: '%s'\n", file, line, exp);
	print_stack_trace();
	halt();
}

void __attribute__((noreturn)) __attribute__((used)) __stack_chk_fail(void) {
	// XXX: consider everything on the stack to be attacker controlled
	printf("__stack_chk_fail!\n");
	print_stack_trace(); // should be safe since we only read
	halt();
}

void __attribute__((noreturn)) __panic(const char *msg, const char *file, int line) {
	printf("PANIC %s:%i: '%s'\n", file, line, msg);
	print_stack_trace();
	halt();
}
