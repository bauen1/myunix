#ifndef STRING_H
#define STRING_H 1

#include <stddef.h>

#include <memcpy.h>

size_t __attribute__((pure)) strlen(const char *str);
void *memset(void *dest, int c, size_t len);
char *strncpy(char *dest, const char *src, size_t n);
int memcmp(const void *v1, const void *v2, size_t len);
char *strndup(const char *s, size_t n);

#endif
