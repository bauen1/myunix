#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <console.h>
#include <fs.h>
#include <heap.h>

fs_node_t *fs_root;

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
	}
	kfree(*node);
	*node = NULL;
}

void fs_open(fs_node_t *node, unsigned int flags) {
	if (!node) {
		return;
	}

	if (node->__refcount >= 0) {
		// lock
		node->__refcount++;
	}

	if (node->open) {
		node->open(node, flags);
	}
}

void fs_close(fs_node_t *node) {
	assert(node != NULL); // crash on double-free
	assert(node != fs_root);

	if (node->__refcount == -1) {
		// special case ( TODO: check for FS_NODE_MOUNTPOINT instead
		return;
	}

	// TODO: lock
	node->__refcount--;

	if (node->__refcount == 0) {
		printf("node refcount = 0\nfs_free_node: %p\n", node);
		fs_free_node(&node);
	}
	return;
}

uint32_t fs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	if (!node) {
		return -1;
	}

	if (node->read) {
		return (uint32_t)node->read(node, offset, size, buffer);
	} else {
		return -1;
	}
}

uint32_t fs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	if (!node) {
		return -1;
	}

	if (node->write) {
		return (uint32_t)node->write(node, offset, size, buffer);
	} else {
		return -1;
	}
}

struct dirent *fs_readdir(struct fs_node *node, uint32_t i) {
	if (!node) {
		return NULL;
	}

	if ((node->flags & FS_NODE_DIRECTORY) && (node->readdir != NULL)) {
		return node->readdir(node, i);
	} else {
		return NULL;
	}
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

void fs_unlink (fs_node_t *node, char *name) {
	if (!node) {
		return;
	}

	if (node->unlink) {
		return node->unlink(node, name);
	} else {
		return;
	}
}

void fs_mkdir(fs_node_t *node, char *name, uint16_t permission) {
	if (node == NULL) {
		return;
	}

	if (node->mkdir) {
		return node->mkdir(node, name, permission);
	} else {
		return;
	}
}

void fs_mount_root(fs_node_t *node) {
	fs_root = node;
}

