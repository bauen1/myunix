#include <assert.h>
#include <stdint.h>
#include <memcpy.h>

void *memcpy(void *dest, const void *src, size_t len) {
	assert(dest != NULL);
	assert(src != NULL);
	assert(len != 0);
	__asm__ __volatile__ (
		"rep movsb"
		: "=D" (dest), "=S" (src), "=c" (len)
		: "0" (dest), "1" (src), "2" (len)
		: "memory");
	return dest;
}
