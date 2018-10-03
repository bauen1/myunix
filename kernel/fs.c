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

	if (node->__refcount == -1) {
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
	assert(node != NULL);
	node->__refcount = -1; // make node immortal
	fs_root_mount = kcalloc(1, sizeof(fs_mount_t));
	fs_root_mount->path = "/";
	fs_root_mount->node = node;
	fs_root_mount->mounts = NULL;
}

// FIXME: allocate mount->path
void fs_mount(fs_node_t *node, const char *subpath) {
	assert(node != NULL);
	assert(subpath != NULL);
	fs_mount_t *mount = kcalloc(1, sizeof(fs_mount_t));
	const size_t subpath_strlen = strlen(subpath);
	mount->path = kmalloc((subpath_strlen + 1) * sizeof(char));
	assert(mount->path != NULL);
	strncpy(mount->path, subpath, subpath_strlen + 1);
	mount->node = node;
	mount->mounts = NULL;
/*	fs_mount_t *mount = kcalloc(1, sizeof(fs_mount_t));
	mount->path = subpath;
	mount->node = node;
	mount->mounts = NULL;
	// TODO: implement mounting inside something thats not the root mount
*/
}

void fs_unmount(const char *path) {
	(void)path;
}

/* tokenize the path */
static char *split_path(const char *path) {
	assert(path != NULL);
	printf("%s(path: '%s')\n", __func__, path);

	const size_t path_len = strlen(path);
	char *p = kmalloc(sizeof(char) * (path_len + 1));
	assert(p != NULL);

	for (size_t i = 0; i < path_len; i++) {
		if (path[i] == '/') {
			if (i == 0) {
				// ignore the first /
				continue;
			} else {
				assert(p[i - 1] != 0);
				p[i] = 0;
			}
		} else {
			p[i] = path[i];
		}
	}
	p[path_len] = 0;
	p[path_len + 1] = 0;

	return p;
}

// TODO: implement canonicalize_path correctly
static char *canonicalize_path(const char *cwd, const char *relative_path) {
	(void)cwd;
	printf("%s(relative_path: '%s')\n", __func__, relative_path);

	const size_t relp_len = strlen(relative_path) + 1;
	const size_t path_len = relp_len + 1; // + 1 so it can be passed to split_path
	char *path = kmalloc(sizeof(char) * path_len);
	assert(path != NULL);
	if (*relative_path == '/') {
		memcpy(path, (relative_path + 1), relp_len);
		if (*path == 0) {
			*path = '/';
		}
	} else {
		memcpy(path, relative_path, relp_len);
	}
	path[strlen(path) + 2] = 0;
	return path;
}

/*
proc search_for_mount(mount_node, path, segments)
  for v in mount_node
    if v.matches(segments.subpath) then
      return v;
*/
static fs_mount_t *findmount(fs_mount_t *parent, char *subpath, size_t subpath_length) {
//	printf("%s(parent: %p, subpath: '%s', subpath_length: %u)\n", __func__, parent, subpath, (uintptr_t)subpath_length);

	if (parent->mounts == NULL) {
		return NULL;
	}

	char *path = kmalloc(subpath_length + 1);
	for (size_t i = 0; i < subpath_length; i++) {
		if (subpath[i] == 0) {
			path[i] = '/';
		} else {
			path[i] = subpath[i];
		}
	}

	fs_mount_t **mounts = parent->mounts;
	while (*mounts != NULL) {
		fs_mount_t *mount = *mounts;
		if (!memcmp(mount->path, subpath, subpath_length)) {
			kfree(path);
			return mount;
		}
		mounts++;
	}

	kfree(path);
	return NULL;
}

static fs_node_t *findfile_recursive(const char *real_path, char *path, size_t path_len, fs_mount_t *mount, fs_node_t *node, unsigned int level) {
//	printf("%s(real_path: '%s', path: '%s', path_len: %u, mount: %p, node: %p, level: %u)\n", __func__, real_path, path, (uintptr_t)path_len, mount, node, level);
	assert(level <= 10);
	char *s = path;
	size_t remaining_len = path_len;
	while (s < path + path_len) {
		fs_node_t *next_node = fs_finddir(node, s);
		if (node != mount->node) { // TODO: FIXME: ugly workaround to not free the mount node
			fs_free_node(&node);
		}
		node = next_node;
		if (node == NULL) {
			printf("fs_finddir: NULL\n");
			return NULL;
		}
		if (node->flags & FS_NODE_SYMLINK) {
			assert("TODO: implement" && 0);
			return NULL;
		}

		fs_mount_t *mount2 = findmount(mount, path, (path_len - remaining_len) + strlen(s));
		if (mount2 != NULL) {
			fs_free_node(&node);
			size_t s_len = strlen(s) + 1;
			s += s_len;
			remaining_len -= s_len;
			node =  findfile_recursive(real_path, s, remaining_len, mount2, mount2->node, level + 1);
			remaining_len = -1;
			break;
		}

		size_t s_len = strlen(s) + 1;
		s += s_len;
		remaining_len -= s_len;
	}
	assert(remaining_len + 1 == 0);
	return node;
}

/*
proc findfile(path)
  segments = split_path(path)
  mount = fs_root_mount
  node = fs_root_mount->node
  return findfile_recursive(path, segments, mount, node)
*/
static fs_node_t *findfile(const char *relative_path) {
	char *real_path = canonicalize_path("", relative_path);
	char *path = split_path(real_path);
	fs_node_t *f = findfile_recursive(real_path, path, strlen(real_path), fs_root_mount, fs_root_mount->node, 0);
	kfree(real_path);
	kfree(path);
	return f;
}

fs_node_t *kopen(const char *path, unsigned int flags) {
//	printf("%s(path: '%s', flags:%u)\n", __func__, path, flags);
	fs_node_t *node;

	if (!memcmp(path, "/", 2)) {
		if (fs_root_mount != NULL) {
			node = fs_root_mount->node;
		} else {
			node = NULL;
		}
	} else {
		node = findfile(path);
	}

	if (node == NULL) {
		return NULL;
	}
	fs_open(node, flags);
	return node;
}
