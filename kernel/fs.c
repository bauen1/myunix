#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <fs.h>

fs_node_t *fs_root;

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

void fs_open(fs_node_t *node, unsigned int flags) {
	if (!node) {
		return;
	}
	if (node->open) {
		node->open(node, flags);
	}
}

void fs_close(fs_node_t *node) {
	if (!node) {
		// double free ?
		return;
	}

	if (node->close) {
		node->close(node);
	}
	return;
}

// FIXME: check if directory
struct dirent *fs_readdir(struct fs_node *node, uint32_t i) {
	if (!node) {
		return NULL;
	}

	if (node->readdir) {
		return node->readdir(node, i);
	} else {
		return NULL;
	}
}

// FIXME: check if directory
struct fs_node *fs_finddir(struct fs_node *node, char *name) {
	if (!node) {
		return NULL;
	}

	if (node->finddir) {
		return node->finddir(node, name);
	} else {
		return NULL;
	}
}

void fs_create (fs_node_t *node, char *name, uint16_t permissions) {
	if (!node) {
		return;
	}

	if (node->create) {
		return node->create(node, name, permissions);
	} else {
		return;
	}
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

