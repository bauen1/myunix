#ifndef HEAP_H
#define HEAP_H 1

#include <stddef.h>

void liballoc_init(void);

void * __attribute__((malloc)) kmalloc(size_t size);
void * __attribute__((malloc)) kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);

#endif
