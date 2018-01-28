#ifndef STRING_H
#define STRING_H 1

#include <stddef.h>

__attribute__((pure))
size_t strlen(const char *str);

void *memset(void *dest, int c, size_t len);

void *memcpy(void *dest, const void *src, size_t n);

#endif
