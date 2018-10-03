#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <bitmap.h>
#include <heap.h>

static_assert(sizeof(char) == 1);

bitmap_t *bitmap_new(size_t size) {
	bitmap_t *bitmap = kcalloc(1, sizeof(bitmap_t));
	if (bitmap == NULL) {
		return NULL;
	}

	assert((size % 8) == 0);
	bitmap->size = size;
	bitmap->map = kcalloc(1, sizeof(char) * (bitmap->size) / 8);
	if (bitmap->map != NULL) {
		return bitmap;
	} else {
		kfree(bitmap);
		return NULL;
	}
}

void bitmap_free(bitmap_t *bitmap) {
	assert(bitmap != NULL);
	kfree(bitmap->map);
	kfree(bitmap);
}

void bitmap_set(bitmap_t *bitmap, size_t bit) {
	assert(bitmap != NULL);
	assert(bit < bitmap->size);
	bitmap->map[bit / 8] |= 1 << (bit % 8);
}

void bitmap_unset(bitmap_t *bitmap, size_t bit) {
	assert(bitmap != NULL);
	assert(bit < bitmap->size);
	bitmap->map[bit / 8] &= ~(1 << (bit % 8));
}

// XXX: true = bit is set
bool bitmap_test(bitmap_t *bitmap, size_t bit) {
	assert(bitmap != NULL);
	assert(bit < bitmap->size);
	return bitmap->map[bit / 8] & (1 << (bit % 8));
}
