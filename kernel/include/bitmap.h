#ifndef BITMAP_H
#define BITMAP_H 1

#include <stddef.h>
#include <stdbool.h>

typedef struct {
	char *map;
	size_t size;
} bitmap_t;

bitmap_t *bitmap_new(size_t size);
void bitmap_free(bitmap_t *bitmap);

void bitmap_set(bitmap_t *bitmap, size_t bit);
void bitmap_unset(bitmap_t *bitmap, size_t bit);
// XXX: true = bit is set
bool bitmap_test(bitmap_t *bitmap, size_t bit);

#endif
