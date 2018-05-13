#include <assert.h>

#include <console.h>
#include <fs.h>
#include <heap.h>
#include <list.h>
#include <string.h>

static void tmpfs_mkdir(fs_node_t *node, char *name, uint16_t permissions);
static void tmpfs_create (fs_node_t *node, char *name, uint16_t permissions);

void tmpfs_register(fs_node_t *tmpfs_root, fs_node_t *node) {
	assert(tmpfs_root != NULL);
	assert(tmpfs_root->object != NULL);
	assert(node != NULL);
	list_insert(((list_t *)tmpfs_root->object), node);
}

static uint32_t tmpfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf) {
	(void)node;
	(void)offset;
	(void)size;
	(void)buf;
	return -1;
}

static uint32_t tmpfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf) {
	(void)node;
	(void)offset;
	(void)size;
	(void)buf;
	return -1;
}

static void tmpfs_open(fs_node_t *node, unsigned int flags) {
	(void)node;
	(void)flags;
	return;
}

static void tmpfs_close(fs_node_t *node) {
	(void)node;
	// TODO: free node->object
	return;
}

static struct dirent *tmpfs_readdir(fs_node_t *node, uint32_t i) {
	(void)node;
	if (i < 16) {
		if (i == 0) {
			struct dirent *v = (struct dirent *)kcalloc(1, sizeof(struct dirent));
			v->ino = i;
			strcpy(v->name, ".");
			return v;
		}
		if (i == 1) {
			struct dirent *v = (struct dirent *)kcalloc(1, sizeof(struct dirent));
			v->ino = i;
			strcpy(v->name, "..");
			return v;
		}
		int j = i - 2;
		if (node->object == NULL) {
			return NULL;
		}
		list_t *l = (list_t *)node->object;
		for (node_t *v = l->head; v != NULL; v = v->next) {
			if (j == 0) {
				fs_node_t *f = (fs_node_t *)v->value;
				struct dirent *v = (struct dirent *)kcalloc(1, sizeof(struct dirent));
				v->ino = i;
				strcpy(v->name, f->name);
				return v;
			} else {
				j--;
			}
		}
	}
	return NULL;
}

static fs_node_t *tmpfs_finddir(fs_node_t *node, char *name) {
//	printf("tmpfs_finddir(0x%x, '%s')\n", (uintptr_t)node, name);
	size_t strlen_name = strlen(name);
	if (node->object == NULL) {
		return NULL;
	}
	list_t *l = (list_t *)node->object;
	for (node_t *v = l->head; v != NULL; v = v->next) {
		if (!memcmp((void *)name, (void *)((fs_node_t *)v->value)->name, strlen_name + 1)) {
			return (fs_node_t *)v->value;
		}
	}
	return NULL;
}

static void tmpfs_unlink (fs_node_t *node, char *name) {
	if ((name == NULL) || (node->object == NULL)) {
		return;
	}

	size_t name_strlen = strlen(name);
	for (node_t *v = ((list_t *)node->object)->head; v != NULL; v = v->next) {
		fs_node_t *f = (fs_node_t *)v->value;
		if (!memcmp(name, f->name, name_strlen)) {
			kfree(f); // FIXME: garbage collect this shit
			list_delete((list_t *)node->object, v);
			return;
		}
	}
	return;
}

static void tmpfs_mkdir(fs_node_t *node, char *name, uint16_t permissions) {
	(void)permissions;
	if ((name == NULL) || (node->object == NULL)) {
		return;
	}

	size_t name_strlen = strlen(name);
	for (node_t *v = ((list_t *)node->object)->head; v != NULL; v = v->next) {
		fs_node_t *f = (fs_node_t *)v->value;
		if (!memcmp(name, f->name, name_strlen)) {
			printf("already exists\n");
			return;
		}
	}

	fs_node_t *f = kcalloc(1, sizeof(fs_node_t));
	strcpy(f->name, name);
	f->read = tmpfs_read;
	f->write = tmpfs_write;
	f->open = tmpfs_open;
	f->close = tmpfs_close;
	f->readdir = tmpfs_readdir;
	f->create = tmpfs_create;
	f->unlink = tmpfs_unlink;
	f->mkdir = tmpfs_mkdir;
	f->object = list_init();

	list_insert((list_t *)node->object, f);
	return;
}

static void tmpfs_create (fs_node_t *node, char *name, uint16_t permissions) {
	(void)permissions;
	if ((name == NULL) || (node->object == NULL)) {
		return;
	}

	size_t name_strlen = strlen(name);
	for (node_t *v = ((list_t *)node->object)->head; v != NULL; v = v->next) {
		fs_node_t *f = (fs_node_t *)v->value;
		if (!memcmp(name, f->name, name_strlen)) {
			printf("already exists\n");
			return;
		}
	}

	fs_node_t *f = kcalloc(1, sizeof(fs_node_t));
	strcpy(f->name, name);
	f->read = tmpfs_read;
	f->write = tmpfs_write;
	f->open = tmpfs_open;
	f->close = tmpfs_close;
	f->readdir = tmpfs_readdir;
	f->create = tmpfs_create;
	f->unlink = tmpfs_unlink;
	f->mkdir = tmpfs_mkdir;
	f->object = NULL;

	printf("list_insert(0x%x, 0x%x);\n", (uintptr_t)node->object, (uintptr_t)f);
	list_insert((list_t *)node->object, f);
	return;
}

fs_node_t *mount_tmpfs() {
	fs_node_t *root = (fs_node_t *)kcalloc(1, sizeof(fs_node_t));
	assert(root != NULL);
	strcpy(root->name, "tmpfs");
	root->read = tmpfs_read;
	root->write = tmpfs_write;
	root->open = tmpfs_open;
	root->close = tmpfs_close;
	root->readdir = tmpfs_readdir;
	root->finddir = tmpfs_finddir;
	root->create = tmpfs_create;
	root->unlink = tmpfs_unlink;
	root->mkdir = tmpfs_mkdir;
	root->object = list_init();
	return root;
}
