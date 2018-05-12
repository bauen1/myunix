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

char *strcpy(char *dest, const char *src) {
	assert(dest != NULL);
	assert(src != NULL);

	size_t i = 0;
	while (src[i] != 0) {
		dest[i] = src[i];
		i++;
	}
	return dest;
}

int memcmp(const void *v1, const void *v2, size_t len) {
	assert(v1 != NULL);
	assert(v2 != NULL);
	char *l = (char *)v1;
	char *r = (char *)v2;
	for (size_t i = 0; i < len; i++) {
		if (l[i] != r[i]) {
			return l[i] - r[i];
		}
	}
	return 0;
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
