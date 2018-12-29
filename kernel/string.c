#include <assert.h>
#include <stddef.h>
#include <heap.h>

size_t __attribute__((pure)) strlen(const char *str) {
	assert(str != NULL);
	size_t len = 0;
	while (str[len] != 0) {
		len++;
	}
	return len;
}

char *strncpy(char *dest, const char *src, size_t n) {
	assert(dest != NULL);
	assert(src != NULL);
	size_t i = 0;
	for (i = 0; (i < n) && (src[i]); i++) {
		dest[i] = src[i];
	}
	for (; i < n; i++) {
		dest[i] = 0;
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

char *strndup(const char *s, size_t n) {
	const size_t s_len = strlen(s);
	const size_t len = s_len > n ? n : s_len;
	char *s2 = kmalloc(sizeof(char) * (len + 1));
	if (s2 == NULL) {
		return NULL;
	}
	strncpy(s2, s, len);
	s2[len + 1] = 0;
	return s2;
}
