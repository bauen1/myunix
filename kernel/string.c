#include <stddef.h>
#include <string.h>

__attribute__((pure))
size_t strlen(const char *str) {
	size_t len = 0;
	while (str[len] != 0) {
		len++;
	}
	return len;
}

void *memset(void *dest, int c, size_t len) {
	size_t i;
	for (i = 0; i < len; i++) {
		((char *)dest)[i] = c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
	while (n--) {
		(*(char *)dest++) = (*(char *)src++);
	}
	return dest;
}
