#include <assert.h>

#include <console.h>
#include <cpu.h>

__attribute__((noreturn))
void __assert_failed(const char *exp, const char *file, int line) {
	printf("%s:%i assertion failed: '%s'\n", file, line, exp);
	printf("halting the cpu...\n");
	halt();
}

__attribute__((noreturn))
void __stack_chk_fail() {
	printf("__stack_chk_fail!\n");
	printf("halting the cpu...\n");
	halt();
}
