#ifndef STRING_H
#define STRING_H 1

#include <stddef.h>

__attribute__((pure))
size_t strlen(const char *str);

void *memset(void *dest, int c, size_t len);

void *memcpy(void *dest, const void *src, size_t n);
char *strcpy(char *dest, const char *src);

int memcmp(const void *v1, const void *v2, size_t len);

#endif
