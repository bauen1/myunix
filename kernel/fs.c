#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <list.h>
#include <console.h>
#include <fs.h>
#include <heap.h>
#include <string.h>

/**
root node of the mount graph
*/
fs_mount_t *fs_root_mount = NULL;

fs_node_t *fs_alloc_node() {
	fs_node_t *node = kcalloc(1, sizeof(fs_node_t));
	if (node == NULL) {
		printf("out of memory!\n");
		return NULL;
	}
	node->__refcount = 0;
	return node;
}

void fs_free_node(fs_node_t **node) {
	assert(*node != fs_root_mount->node);
	if ((*node)->close) {
		(*node)->close(*node);
	}
	kfree(*node);
	*node = NULL;
}

void fs_open(fs_node_t *node, unsigned int flags) {
	if (!node) {
		return;
	}

	if (node->__refcount >= 0) {
		// TODO: lock
		node->__refcount++;
	}

	if (node->open) {
		node->open(node, flags);
	}
}

void fs_close(fs_node_t *node) {
	assert(node != NULL); // crash on double-free
	assert(node != fs_root_mount->node);

	if (node->__refcount == -1) {
		// special case ( TODO: check for FS_NODE_MOUNTPOINT instead
		return;
	}

	// TODO: lock
	node->__refcount--;

	if (node->__refcount == 0) {
		fs_free_node(&node);
	}
	return;
}

uint32_t fs_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer) {
	if ((node != NULL) && (node->read != NULL)) {
		return node->read(node, offset, size, buffer);
	}
	return -1;
}

uint32_t fs_write(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer) {
	if ((node != NULL) && (node->write != NULL)) {
		return node->write(node, offset, size, buffer);
	}
	return -1;
}

struct dirent *fs_readdir(struct fs_node *node, uint32_t i) {
	if (!node) {
		return NULL;
	}

	if ((node->flags & FS_NODE_DIRECTORY) && (node->readdir != NULL)) {
		return node->readdir(node, i);
	}

	return NULL;
}

struct fs_node *fs_finddir(struct fs_node *node, char *name) {
	if (!node) {
		return NULL;
	}

	if ((node->flags & FS_NODE_DIRECTORY) && (node->finddir != NULL)) {
		return node->finddir(node, name);
	}

	return NULL;
}

void fs_create (fs_node_t *node, char *name, uint16_t permissions) {
	if (!node) {
		return;
	}

	if ((node->flags & FS_NODE_DIRECTORY) && (node->create != NULL)) {
		return node->create(node, name, permissions);
	}

	return;
}

int fs_unlink (fs_node_t *node, char *name) {
	if (node == NULL) {
		return -1;
	}

	if ((node->flags & FS_NODE_DIRECTORY) && (node->unlink != NULL)) {
		return node->unlink(node, name);
	}

	return -1;
}

void fs_mkdir(fs_node_t *node, char *name, uint16_t permission) {
	if (node == NULL) {
		return;
	}
	if ((node->flags & FS_NODE_DIRECTORY) && (node->mkdir != NULL)) {
		return node->mkdir(node, name, permission);
	}

	return;
}

void fs_mount_root(fs_node_t *node) {
	// TODO: implement root remount
	assert(fs_root_mount == NULL);
	fs_root_mount = kcalloc(1, sizeof(fs_mount_t));
	fs_root_mount->path = "/";
	fs_root_mount->node = node;
	fs_root_mount->mounts = NULL;
}

fs_node_t *kopen(char *path, unsigned int flags) {
	if (node == NULL) {
		return NULL;
	}
	fs_open(node, flags);
	return node;
}
