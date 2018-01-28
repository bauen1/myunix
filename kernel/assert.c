#include <assert.h>

#include <console.h>
#include <cpu.h>

__attribute__((noreturn))
void __assert_failed(const char *exp, const char *file, int *line) {
	printf("%s:%i assertion failed: '%s'\n", file, line, exp);
	halt();
}
