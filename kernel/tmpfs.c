#include <assert.h>

#include <console.h>
#include <fs.h>
#include <heap.h>
#include <list.h>
#include <string.h>

/**
TODO: implement file support
*/
struct tmpfs_object {
	char *name;
	enum fs_node_flags flags;
	list_t *childs;
	int __refcount;
};

static uint32_t tmpfs_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buf);
static uint32_t tmpfs_write(fs_node_t *node, uint32_t offset, uint32_t size, void *buf);
static void tmpfs_open(fs_node_t *node, unsigned int flags);
static void tmpfs_close(fs_node_t *node);
static struct dirent *tmpfs_readdir(fs_node_t *node, uint32_t i);
static fs_node_t *tmpfs_finddir(fs_node_t *node, char *name);
static int tmpfs_unlink (fs_node_t *node, char *name);
static void tmpfs_mkdir(fs_node_t *node, char *name, uint16_t permissions);
static void tmpfs_create (fs_node_t *node, char *name, uint16_t permissions);

static struct tmpfs_object *tmpfs_create_obj(char *name, enum fs_node_flags flags) {
	struct tmpfs_object *obj = kcalloc(1, sizeof(struct tmpfs_object));
	assert(obj != NULL);
	obj->flags = flags;
	obj->name = strndup(name, strlen(name));
	assert(obj->name != NULL);
	if (obj->flags & FS_NODE_DIRECTORY) {
		// directory, init files list
		obj->childs = list_init();
		assert(obj->childs != NULL);
	} else {
		// file, init block list
		obj->childs = NULL;
	}
	return obj;
}

static void tmpfs_destroy_object(struct tmpfs_object *obj) {
	assert(obj->__refcount == 0);
	kfree(obj->name);
	assert(obj->childs->length == 0);
	kfree(obj->childs);
	kfree(obj);
}

static fs_node_t *fs_node_from_tmpfs(struct tmpfs_object *tmpfs_obj) {
	assert(tmpfs_obj != NULL);
	fs_node_t *f = fs_node_new();
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
	tmpfs_obj->__refcount++;

	return f;
}

static uint32_t tmpfs_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buf) {
	(void)node; (void)offset; (void)size; (void)buf;
	return -1;
}

static uint32_t tmpfs_write(fs_node_t *node, uint32_t offset, uint32_t size, void *buf) {
	(void)node; (void)offset; (void)size; (void)buf;
	return -1;
}

static void tmpfs_open(fs_node_t *node, unsigned int flags) {
	(void)node; (void)flags;
	return;
}

static void tmpfs_close(fs_node_t *node) {
	struct tmpfs_object *obj = node->object;
	assert(obj != NULL);

	obj->__refcount--;
	if (obj->__refcount == 0) {
		tmpfs_destroy_object(obj);
	}

	node->object = NULL;
	return;
}

static struct dirent *tmpfs_readdir(fs_node_t *node, uint32_t i) {
	assert(node != NULL);
	assert(node->object != NULL);
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
	list_t *l = obj->childs;
	assert(l != NULL);
	for (node_t *node = l->head; node != NULL; node = node->next) {
		if (j == 0) {
			struct tmpfs_object *nobj = (struct tmpfs_object *)node->value;
			struct dirent *v = kcalloc(1, sizeof(struct dirent));
			assert(v != NULL);
			v->ino = i;
			strncpy(v->name, nobj->name, sizeof(v->name));
			return v;
		} else {
			j--;
		}
	}
	return NULL;
}

static fs_node_t *tmpfs_finddir(fs_node_t *node, char *name) {
	assert(node->object != NULL);

	struct tmpfs_object *obj = (struct tmpfs_object *)node->object;
	assert(obj != NULL);
	list_t *l = obj->childs;
	assert(l != NULL);
	size_t name_strlen = strlen(name);
	for (node_t *v = l->head; v != NULL; v = v->next) {
		struct tmpfs_object *v_obj = v->value;
		assert(v_obj != NULL);
		if (!memcmp(name, v_obj->name, name_strlen)) {
			return fs_node_from_tmpfs(v_obj);
		}
	}
	return NULL;
}

static int tmpfs_unlink (fs_node_t *node, char *name) {
	struct tmpfs_object *obj = node->object;
	assert(obj != NULL);

	if (name == NULL) {
		return -1;
	}

	list_t *l = obj->childs;
	assert(l != NULL);
	size_t name_strlen = strlen(name);
	for (node_t *v = l->head; v != NULL; v = v->next) {
		struct tmpfs_object *v_obj = v->value;
		assert(v_obj != NULL);
		if (!memcmp(name, v_obj->name, name_strlen)) {
			v_obj->__refcount--;
			if (v_obj->__refcount == 0) {
				tmpfs_destroy_object(v_obj);
			}
			// FIXME: use another list_* call
			list_delete(l, v);
			kfree(v);
			return 0;
		}
	}

	// no node found
	return -1;
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
			// already exists
			return;
		}
	}

	struct tmpfs_object *nobj = tmpfs_create_obj(name, FS_NODE_DIRECTORY);
	nobj->__refcount++;
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
			// already exists
			return;
		}
	}

	struct tmpfs_object *nobj = tmpfs_create_obj(name, FS_NODE_FILE);
	nobj->__refcount++;
	list_insert(childs, nobj);

	return;
}

fs_node_t *mount_tmpfs() {
	struct tmpfs_object *tmpfs_root = tmpfs_create_obj("tmpfs", FS_NODE_DIRECTORY);
	tmpfs_root->__refcount = -1;
	fs_node_t *root_node = fs_node_from_tmpfs(tmpfs_root);
	fs_open(root_node, 0);
	return root_node;
}
