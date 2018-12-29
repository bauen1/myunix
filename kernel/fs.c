#include <assert.h>
#include <stdbool.h>
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

/* misc. helpers */
/* strlen for path elements */
static size_t path_element_size(const char *path) {
	size_t i = 0;
	while ((path[i] != 0) && (path[i] != '/')) {
		i++;
	}
	return i;
}

/* XXX: free the return value if not NULL */
static char *tokenize_path(const char *path) {
	assert(path != NULL);

	const size_t path_len = strlen(path);
	char *s = kmalloc(sizeof(char) * (path_len + 2));
	if (s == NULL) {
		return NULL;
	}

	// TODO: make this a bit more efficient
	size_t i = 0;
	size_t j = 0;
	while (i < path_len) {
		if (path[i] == '/') {
			if ((j != 0) && (s[j - 1] != 0)) {
				s[j] = 0;
				j++;
			}
			i++;
			continue;
		} else {
			s[j] = path[i];
			j++;
			i++;
		}
	}

	for (; j < path_len + 2; j++) {
		s[j] = 0;
	}

	return s;
}

/* XXX: list_free on the return value if not NULL */
static list_t *tokenize_path_list(const char *path) {
	assert(path != NULL);

	list_t *elements = list_init();
	if (elements == NULL) {
		return NULL;
	}
	const size_t path_len = strlen(path);
	size_t i = 0;
	while (i < path_len) {
		const size_t j = path_element_size(&path[i]);
		char *s = strndup(&path[i], j);
		assert(s != NULL); // TODO: handle
		if (strlen(s) == 0) {
			kfree(s);
		} else {
			list_insert(elements, s);
		}
		i += j + 1;
	}
	return elements;
}

static list_t *canonicalize_path_push_path(const char *path, list_t *elements) {
	assert(path != NULL);
	assert(elements != NULL);

	char *path_t = tokenize_path(path);
	assert(path_t != NULL);
	char *s = path_t;

	/* push tokens and resolv '.' and '..' */
	while (*s != 0) {
		const size_t j = strlen(s) + 1;
		if (!memcmp(s, "..", 3)) {
			node_t *tail = elements->tail;
			/* TODO: properly handle underflow */
			assert(tail != NULL);
			kfree(tail->value);
			list_delete(elements, tail);
			kfree(tail);
		} else if (!memcmp(s, ".", 2)) {
			;
			/* do-nothing */
		} else {
			char *s_n = kmalloc(sizeof(char) * j);
			/* TODO: handle out of memory graefully */
			assert(s_n != NULL);
			strncpy(s_n, s, j);
			list_insert(elements, s_n);
		}
		s += j;
	}

	kfree(path_t);
	return elements;
}

// XXX: for performance reasons, parent must already be canonical
static list_t *canonicalize_path_list(const char *parent, const char *relative_path) {
	assert(parent != NULL);
	assert(relative_path != NULL);

	list_t *elements = list_init();
	assert(elements != NULL);

	const size_t path_l = strlen(relative_path);
	if ((path_l > 0) && (relative_path[0] != '/')) {
		/* relative path, tokenize and push parent path */
		char *p_t = tokenize_path(parent);
		assert(p_t != NULL);
		char *s = p_t;
		while (*s != 0) {
			const size_t j = strlen(s) + 1;
			char *sn = kmalloc(sizeof(char) * j);
			assert(sn != NULL);
			strncpy(sn, s, j);
			list_insert(elements, sn);
			s += j;
		}
		kfree(p_t);
	}

	return canonicalize_path_push_path(relative_path, elements);
}

/* XXX: end has to be non-null and it has to appear in the list after start */
static list_t *copy_elements(node_t *start, node_t *end) {
	list_t *elements = list_init();
	if (elements == NULL) {
		return NULL;
	}

	for (node_t *node = start; node != end; node = node->next) {
		assert(node != NULL);
		const char *value = node->value;
		const size_t value_len = strlen(value) + 1;
		assert(value_len != 1);
		char *new_value = kmalloc(sizeof(char) * value_len);
		assert(new_value != NULL);
		memcpy(new_value, value, value_len);
		list_insert(elements, new_value);
	}

	return elements;
}

/* fs_node_t helpers */
fs_node_t *fs_node_new() {
	fs_node_t *node = kcalloc(1, sizeof(fs_node_t));
	if (node == NULL) {
		return NULL;
	}
	node->__refcount = 1;
	return node;
}

static void fs_node_free(fs_node_t **node) {
	assert(node != NULL);
	assert(*node != NULL);
	assert((*node)->__refcount == 0);

	if ((*node)->close) {
		(*node)->close(*node);
	}

	kfree(*node);
	*node = NULL;
}

void fs_node_release(fs_node_t **node) {
	assert(node != NULL);
	assert((*node) != NULL);

	if ((*node)->__refcount > 0) {
		(*node)->__refcount--;
		if ((*node)->__refcount == 0) {
			fs_node_free(node);
		}
	}
}

fs_node_t *fs_node_reference(fs_node_t *node) {
	assert(node != NULL);
	assert(node->__refcount != 0);

	if (node->__refcount > 0) {
		node->__refcount++;
	}
	return node;
}

/* fs_mount_t helpers */
fs_mount_t *fs_mount_new() {
	fs_mount_t *mount = kcalloc(1, sizeof(fs_mount_t));
	if (mount == NULL) {
		return NULL;
	}
	mount->__refcount = 1;
	mount->mounts = list_init();
	if (mount->mounts == NULL) {
		kfree(mount);
		return NULL;
	}
	return mount;
}

static void fs_mount_free(fs_mount_t **mount) {
	assert(mount != NULL);
	assert(*mount != NULL);
	assert((*mount)->__refcount == 0);
	assert((*mount)->mounts != NULL);
	assert((*mount)->mounts->length == 0);

	list_free((*mount)->mounts);
	memset(*mount, 0, sizeof(fs_mount_t));
	kfree(*mount);
	*mount = NULL;
	return;
}

fs_mount_t *fs_mount_reference(fs_mount_t *mount) {
	if (mount->__refcount > 0) {
		mount->__refcount++;
	}
	return mount;
}

void fs_mount_release(fs_mount_t **mount) {
	assert(mount != NULL);
	assert(*mount != NULL);
	assert((*mount)->__refcount != 0);
	if ((*mount)->__refcount > 0) {
		(*mount)->__refcount--;
		if ((*mount)->__refcount == 0) {
			fs_mount_free(mount);
		}
	}
}

/* vfs helpers */
void fs_open(fs_node_t *node, unsigned int flags) {
	assert(node != NULL);

	fs_node_reference(node);

	if (node->open) {
		node->open(node, flags);
	}
}

void fs_close(fs_node_t **node) {
	assert(node != NULL);

	fs_node_release(node);
}

uint32_t fs_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer) {
	assert(node != NULL);
	if ((node != NULL) && (node->read != NULL)) {
		return node->read(node, offset, size, buffer);
	}
	return -1;
}

uint32_t fs_write(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer) {
	assert(node != NULL);
	if ((node != NULL) && (node->write != NULL)) {
		return node->write(node, offset, size, buffer);
	}
	return -1;
}

struct dirent *fs_readdir(struct fs_node *node, uint32_t i) {
	assert(node != NULL);
	if ((node->flags & FS_NODE_DIRECTORY) && (node->readdir != NULL)) {
		return node->readdir(node, i);
	}
	return NULL;
}

struct fs_node *fs_finddir(struct fs_node *node, char *name) {
	assert(node != NULL);
	if ((node->flags & FS_NODE_DIRECTORY) && (node->finddir != NULL)) {
		return node->finddir(node, name);
	}
	return NULL;
}

void fs_create (fs_node_t *node, char *name, uint16_t permissions) {
	assert(node != NULL);
	if ((node->flags & FS_NODE_DIRECTORY) && (node->create != NULL)) {
		return node->create(node, name, permissions);
	}
	return;
}

int fs_unlink (fs_node_t *node, char *name) {
	assert(node != NULL);
	if ((node->flags & FS_NODE_DIRECTORY) && (node->unlink != NULL)) {
		return node->unlink(node, name);
	}
	return -1;
}

int fs_readlink(fs_node_t *node, char *buf, size_t size) {
	assert(node != NULL);
	assert(buf != NULL);

	if ((node->flags & FS_NODE_SYMLINK) && (node->readlink != NULL)) {
		return node->readlink(node, buf, size);
	}
	return -1;
}

void fs_mkdir(fs_node_t *node, char *name, uint16_t permission) {
	assert(node != NULL);
	if ((node->flags & FS_NODE_DIRECTORY) && (node->mkdir != NULL)) {
		return node->mkdir(node, name, permission);
	}
	return;
}

// TODO: implement root remount
void fs_mount_root(fs_node_t *node) {
	assert(fs_root_mount == NULL);
	assert(node != NULL);

	fs_mount_t *mount = fs_mount_new();
	assert(mount != NULL);
	assert(mount->mounts != NULL);

	fs_root_mount = mount;
	fs_root_mount->node = fs_node_reference(node);
	fs_root_mount->element = kmalloc(sizeof(char) * 2);
	fs_root_mount->element[0] = '/';
	fs_root_mount->element[1] = 0;
}

static fs_mount_t *fs_mount_submount(fs_mount_t *parent, fs_node_t *node, const char *path_element) {
	assert(parent != NULL);
	assert(parent->mounts != NULL);
	assert(node != NULL);

	fs_mount_t *mount = fs_mount_new();
	assert(mount != NULL);

	mount->element = strndup(path_element, strlen(path_element));
	if (mount->element == NULL) {
		fs_mount_release(&mount);
		return NULL;
	}

	// TODO: we should probably call fs_open instead
	mount->node = fs_node_reference(node);

	list_insert(parent->mounts, fs_mount_reference(mount));

	return mount;
}

void fs_unmount(const char *path) {
	(void)path;
	printf("%s: TODO: implement!\n", __func__);
	assert(0);
}

static fs_mount_t *findmount(const fs_mount_t *parent, const char *subpath) {
	assert(parent != NULL);
	assert(parent->mounts != NULL);
	assert(subpath != NULL);

	const size_t subpath_len = strlen(subpath);

	for list_each(node, parent->mounts) {
		fs_mount_t *mount = (fs_mount_t *)node->value;
		if (!memcmp(mount->element, subpath, subpath_len + 1)) {
			return mount;
		}
	}

	return NULL;
}

static fs_mount_t *walk_mount_graph(list_t *elements, fs_mount_t *mount) {
	node_t *node = elements->head;
	while (node != NULL) {
		fs_mount_t *new_mount = findmount(mount, node->value);
		if (new_mount == NULL) {
			break;
		} else {
			mount = new_mount;
			node_t *tmp = node;
			node = node->next;
			list_delete(elements, tmp);
			kfree(tmp);
		}
	}
	return mount;
}

static fs_node_t *findfile_recursive(list_t *elements, fs_node_t *root_node, unsigned int level) {
	assert(elements != NULL);
	assert(root_node != NULL);

	if (level >= 10) {
		printf("%s: recursive limit of 10 exceeded!\n", __func__);
		return NULL;
	}

	fs_node_t *node = fs_node_reference(root_node);
	node_t *element = elements->head;
	while (element != NULL) {
		fs_node_t *next_node = fs_finddir(node, element->value);

		fs_node_release(&node);
		if (next_node == NULL) {
			printf("%s: fs_finddir: NULL!\n", __func__);
			return NULL;
		}
		node = next_node;

		if (node->flags & FS_NODE_SYMLINK) {
			char buf[256];
			int len = fs_readlink(node, &buf[0], 256);
			if (len < 0) {
				printf("%s: fs_readlink returned error\n", __func__);
				assert(0); // TODO: free
				return NULL;
			}

			assert(buf[len] == 0);

			// copy all leading list elements
			list_t *l = copy_elements(elements->head, element);

			canonicalize_path_push_path(buf, l);

			fs_mount_t *mount = walk_mount_graph(l, fs_root_mount);
			next_node = findfile_recursive(l, mount->node, level + 1);

			for list_each(node, l) {
				kfree(node->value);
				node->value = NULL;
			}
			list_free(l);

			if (next_node != NULL) {
				fs_node_release(&node);
				node = next_node;
			} else {
				printf("%s: dangling symlink!\n", __func__);
				fs_node_release(&node);
				return NULL;
			}
		}
		element = element->next;
	}

	return node;
}

static fs_node_t *findfile(const char *root, const char *relative_path) {
	list_t *l = canonicalize_path_list(root, relative_path);
	fs_mount_t *mount = walk_mount_graph(l, fs_root_mount);
	fs_node_t *f = findfile_recursive(l, mount->node, 0);

	for list_each(node, l) {
		kfree(node->value);
		node->value = NULL;
	}
	list_free(l);
	return f;
}

/* kernel filesystem api wrappers */
fs_node_t *kopen(const char *path, unsigned int flags) {
	fs_node_t *node;

	if (!memcmp(path, "/", 2)) {
		if (fs_root_mount != NULL) {
			node = fs_node_reference(fs_root_mount->node);
		} else {
			node = NULL;
		}
	} else {
		node = findfile("/", path);
	}

	if (node == NULL) {
		return NULL;
	}

	fs_open(node, flags);
	// XXX: findfile already has a reference on this node, and we need to release it after opening the node
	fs_node_release(&node);
	return node;
}

bool kmount(const char *path, fs_node_t *node) {
	assert(path != NULL);
	assert(node != NULL);
	printf("%s(path: '%s', node: %p)\n", __func__, path, node);

	list_t *elements = tokenize_path_list(path);
	if (elements == NULL) {
		return false;
	}

	for list_each(node, elements) {
		const char *s = (const char *)node->value;
		printf("%s: element '%s'\n", __func__, s);
	}

	if (elements->length < 1) {
		return false;
	}

	char *mount_element = elements->tail->value;
	assert(mount_element != NULL);

	fs_mount_t *parent1 = walk_mount_graph(elements, fs_root_mount);
	printf("%s: parent1: %p\n", __func__, parent1);

	if (elements->length == 0) {
		printf("%s: TODO: there is already a mount at that location\n", __func__);
		// TODO: properly handle and free memory
		return false;
	}

	if (elements->length == 1) {
		fs_mount_t *submount = fs_mount_submount(parent1, node, mount_element);
		if (submount == NULL) {
			return false;
		} else {
			printf("%s: submount->__refcount == %i\n", __func__, submount->__refcount);
			assert(submount->__refcount == 2);
			fs_mount_release(&submount);
		}
	} else {
		// TODO: insert "empty" submount entries
		assert(0);
	}

	for list_each(node, elements) {
		kfree(node->value);
		node->value = NULL;
	}
	list_free(elements);

	return true;
}
