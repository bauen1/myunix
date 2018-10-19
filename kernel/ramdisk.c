#include <assert.h>
#include <stddef.h>

#include <fs.h>
#include <heap.h>
#include <string.h>

static uint32_t ramdisk_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer) {
	if (offset > node->length) {
		return 0;
	}

	if (offset + size > node->length) {
		size = node->length - offset;
	}

	if (size == 0) {
		return 0;
	}

	memcpy(buffer, (void *)(((uintptr_t)node->object) + offset), size);
	return size;
}

static uint32_t ramdisk_write(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer) {
	if (offset > node->length) {
		return 0;
	}

	if (offset + size > node->length) {
		size = node->length - offset;
	}

	if (size == 0) {
		return 0;
	}

	memcpy((void *)(((uintptr_t)node->object) + offset), buffer, size);
	return size;
}

static void ramdisk_open(fs_node_t *node, unsigned int flags) {
	(void)node;
	(void)flags;
	return;
}

static void ramdisk_close(fs_node_t *node) {
	(void)node;
	return;
}

fs_node_t *ramdisk_init(uintptr_t ptr, size_t size) {
	fs_node_t *node = fs_node_new();
	assert(node != NULL);
	memcpy((void *)node->name, "ramdisk", 8);
	node->read = ramdisk_read;
	node->write = ramdisk_write;
	node->open = ramdisk_open;
	node->close = ramdisk_close;
	node->length = size;
	node->object = (void *)ptr;
	return node;
}
