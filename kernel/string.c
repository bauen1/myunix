#include <assert.h>
#include <stddef.h>

__attribute__((pure))
size_t strlen(const char *str) {
	assert(str != NULL);
	size_t len = 0;
	while (str[len] != 0) {
		len++;
	}
	return len;
}

void *memset(void *dest, int c, size_t len) {
	assert(dest != NULL);
	assert(len != 0);
	char *d = (char *)dest;
	while (len--) {
		*d++ = (char)c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t len) {
	assert(dest != NULL);
	assert(src != NULL);
	assert(len != 0);
	char *d = (char *)dest;
	char *s = (char *)src;

	while (len--) {
		*d++ = *s++;
	}
	return dest;
}
