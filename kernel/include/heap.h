#ifndef HEAP_H
#define HEAP_H 1

#include <stddef.h>

void *kmalloc(size_t size);
void kfree(void *ptr);
void *kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);

#endif
