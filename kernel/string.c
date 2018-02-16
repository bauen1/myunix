#include <assert.h>
#include <stddef.h>

__attribute__((pure))
size_t strlen(const char *str) {
	size_t len = 0;
	while (str[len] != 0) {
		len++;
	}
	return len;
}

void *memset(void *dest, int c, size_t len) {
	for (size_t i = 0; i < len; i++) {
		((char *)dest)[i] = c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t len) {
	for (size_t i = 0; i < len; i++) {
		((char *)dest)[i] = ((char *)src)[i];
	}
	return dest;
}
