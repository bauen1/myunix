#include <assert.h>

#include <console.h>
#include <fs.h>
#include <heap.h>
#include <list.h>
#include <string.h>

struct tmpfs_object {
	char *name;
	enum fs_node_flags flags;
	list_t *childs;
};

static uint32_t tmpfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf);
static uint32_t tmpfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf);
static void tmpfs_open(fs_node_t *node, unsigned int flags);
static void tmpfs_close(fs_node_t *node);
static struct dirent *tmpfs_readdir(fs_node_t *node, uint32_t i);
static fs_node_t *tmpfs_finddir(fs_node_t *node, char *name);
static void tmpfs_unlink (fs_node_t *node, char *name);
static void tmpfs_mkdir(fs_node_t *node, char *name, uint16_t permissions);
static void tmpfs_create (fs_node_t *node, char *name, uint16_t permissions);

static struct tmpfs_object *tmpfs_create_obj(char *name, enum fs_node_flags flags) {
	struct tmpfs_object *obj = kcalloc(1, sizeof(struct tmpfs_object));
	assert(obj != NULL);
	obj->flags = flags;
	obj->name = kmalloc(strlen(name) + 1);
	assert(obj->name != NULL);
	strncpy(obj->name, name, 255);
	obj->childs = list_init();
	assert(obj->childs != NULL);
	return obj;
}

static fs_node_t *fs_node_from_tmpfs(struct tmpfs_object *tmpfs_obj) {
	fs_node_t *f = kcalloc(1, sizeof(fs_node_t));
	assert(f != NULL);
	strncpy(f->name, tmpfs_obj->name, 255);
	f->flags = tmpfs_obj->flags;
	f->object = tmpfs_obj;
	switch(f->flags) {
		case FS_NODE_DIRECTORY:
			f->open = tmpfs_open;
			f->close = tmpfs_close;
			f->readdir = tmpfs_readdir;
			f->create = tmpfs_create;
			f->unlink = tmpfs_unlink;
			f->mkdir = tmpfs_mkdir;
			f->finddir = tmpfs_finddir;
			break;
		case FS_NODE_FILE:
			f->read = tmpfs_read;
			f->write = tmpfs_write;
			break;
		default:
			assert(0);
			break;
	}

	return f;
}

static uint32_t tmpfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf) {
	(void)node; (void)offset; (void)size; (void)buf;
	return -1;
}

static uint32_t tmpfs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buf) {
	(void)node; (void)offset; (void)size; (void)buf;
	return -1;
}

static void tmpfs_open(fs_node_t *node, unsigned int flags) {
	(void)node; (void)flags;
	return;
}

static void tmpfs_close(fs_node_t *node) {
	(void)node;
/*
	struct tmpfs_object *obj = (struct tmpfs_object *)node->object;
	kfree(obj->childs);
	kfree(obj->name);
	kfree(obj);
*/
	return;
}

static struct dirent *tmpfs_readdir(fs_node_t *node, uint32_t i) {
	assert(node != NULL);
	struct tmpfs_object *obj = (struct tmpfs_object *)node->object;
	if (i == 0) {
		struct dirent *v = kcalloc(1, sizeof(struct dirent));
		assert(v != NULL);
		v->ino = i;
		strncpy(v->name, ".", 255);
		return v;
	}
	if (i == 1) {
		struct dirent *v = kcalloc(1, sizeof(struct dirent));
		assert(v != NULL);
		v->ino = i;
		strncpy(v->name, "..", 255);
		return v;
	}
	int j = i - 2;
	if (node->object == NULL) {
		return NULL;
	}
	list_t *l = obj->childs;
	for (node_t *v = l->head; v != NULL; v = v->next) {
		if (j == 0) {
			fs_node_t *f = (fs_node_t *)v->value;
			struct dirent *v = kcalloc(1, sizeof(struct dirent));
			assert(v != NULL);
			v->ino = i;
			strncpy(v->name, f->name, 255);
			return v;
		} else {
			j--;
		}
	}
	return NULL;
}

static fs_node_t *tmpfs_finddir(fs_node_t *node, char *name) {
//	printf("tmpfs_finddir(0x%x, '%s')\n", (uintptr_t)node, name);
	if (node->object == NULL) {
		return NULL;
	}

	struct tmpfs_object *obj = (struct tmpfs_object *)node->object;
	list_t *l = obj->childs;
	size_t strlen_name = strlen(name);
	for (node_t *v = l->head; v != NULL; v = v->next) {
		if (!memcmp(name, ((struct tmpfs_object *)v->next)->name, strlen_name + 1)) {
			return fs_node_from_tmpfs((struct tmpfs_object *)v->next);
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
//			kfree(f); // FIXME: garbage collect this shit
			list_delete((list_t *)node->object, v);
			return;
		}
	}
	return;
}

static void tmpfs_mkdir(fs_node_t *node, char *name, uint16_t permissions) {
	(void)permissions;
	assert(name != NULL);
	assert(node != NULL);
	assert(node->object != NULL);

	struct tmpfs_object *obj = (struct tmpfs_object *)node->object;
	assert(obj->childs != NULL);

	list_t *childs = obj->childs;

	size_t name_strlen = strlen(name);
	for (node_t *v = childs->head; v != NULL; v = v->next) {
		struct tmpfs_object *obj2 = (struct tmpfs_object *)v->value;
		if (!memcmp(name, obj2->name, name_strlen + 1)) {
			printf("already exists\n");
			return;
		}
	}

	struct tmpfs_object *nobj = tmpfs_create_obj(name, FS_NODE_DIRECTORY);
	list_insert(childs, nobj);

	return;
}

static void tmpfs_create (fs_node_t *node, char *name, uint16_t permissions) {
	(void)permissions;
	assert(name != NULL);
	assert(node != NULL);
	assert(node->object != NULL);

	struct tmpfs_object *obj = (struct tmpfs_object *)node->object;
	assert(obj->childs != NULL);

	list_t *childs = obj->childs;

	size_t name_strlen = strlen(name);
	for (node_t *v = childs->head; v != NULL; v = v->next) {
		struct tmpfs_object *obj2 = (struct tmpfs_object *)v->value;
		if (!memcmp(name, obj2->name, name_strlen + 1)) {
			printf("already exists\n");
			return;
		}
	}

	struct tmpfs_object *nobj = tmpfs_create_obj(name, FS_NODE_FILE);
	list_insert(childs, nobj);

	return;
}

fs_node_t *mount_tmpfs() {
/*	fs_node_t *root = kcalloc(1, sizeof(fs_node_t));
	assert(root != NULL);
	strncpy(root->name, "tmpfs", 255);
	root->flags = FS_NODE_DIRECTORY;
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
*/
	struct tmpfs_object *tmpfs_root = tmpfs_create_obj("tmpfs", FS_NODE_DIRECTORY);
	fs_node_t *root_node = fs_node_from_tmpfs(tmpfs_root);
	fs_open(root_node, 0);
	return root_node;
}
