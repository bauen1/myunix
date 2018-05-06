#include <stddef.h>

#include <fs.h>
#include <heap.h>
#include <string.h>

uint32_t ramdisk_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	if (offset > node->length) {
		return 0;
	}

	if (offset + size > node->length) {
		size = node->length - offset;
	}

	memcpy(buffer, (void *)(node->object + offset), size);
	return size;
}

uint32_t ramdisk_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	if (offset > node->length) {
		return 0;
	}

	if (offset + size > node->length) {
		size = node->length - offset;
	}
	memcpy((void *)(node->object + offset), buffer, size);
	return size;
}

void ramdisk_open(fs_node_t *node, unsigned int flags) {
	(void)node;
	(void)flags;
	return;
}
void ramdisk_close(fs_node_t *node) {
	(void)node;
	return;
}

fs_node_t *ramdisk_init(uintptr_t ptr, size_t size) {
	fs_node_t *node = kcalloc(1, sizeof(fs_node_t));
	memcpy((void *)node->name, "ramdisk", 8);
	node->read = ramdisk_read;
	node->write = ramdisk_write;
	node->open = ramdisk_open;
	node->close = ramdisk_close;
	node->length = size;
	node->object = (void *)ptr;
	return node;
}
