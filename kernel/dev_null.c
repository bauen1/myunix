#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <fs.h>
#include <heap.h>
#include <string.h>

static uint32_t null_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	(void)node;
	(void)offset;
	(void)size;
	(void)buffer;
	return 0;
}

static uint32_t null_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	(void)node;
	(void)offset;
	(void)size;
	(void)buffer;
	return 0;
}

static uint32_t zero_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	(void)node;
	(void)offset;
	memset(buffer, 0, size);
	return size;
}

fs_node_t *null_create() {
	fs_node_t *f = kcalloc(1, sizeof(fs_node_t));
	assert(f != NULL);
	strncpy(f->name, "null", 255);
	f->flags = FS_NODE_CHARDEVICE;
	f->read = null_read;
	f->write = null_write;
	return f;
}

fs_node_t *zero_create() {
	fs_node_t *f = kcalloc(1, sizeof(fs_node_t));
	assert(f != NULL);
	strncpy(f->name, "zero", 255);
	f->flags = FS_NODE_CHARDEVICE;
	f->read = zero_read;
	f->write = null_write;
	return f;
}
