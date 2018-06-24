// FIXME: memory leaks
#include <assert.h>

#include <console.h>
#include <tar.h>
#include <fs.h>
#include <heap.h>
#include <string.h>
#include <oct2bin.h>

struct tar_header {
	char name[100];
	uint32_t mode;
	uint32_t gid;
	uint32_t uid;
	char fsize[12];
	char last_mod[12];
	uint32_t checksum;
	uint8_t type;
	char linked_name[100] __attribute__((packed));
} __attribute__((packed));

struct tar_obj {
	fs_node_t *device;
	char *name;
	uintptr_t offset;
	enum fs_node_flags flags;
	uint32_t length;
	
};

static uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static void tar_open(fs_node_t *node, unsigned int flags);
static void tar_close(fs_node_t *node);
static struct dirent *tar_readdir(struct fs_node *node, uint32_t i);
static fs_node_t *tar_finddir(struct fs_node *node, char *name);

static struct tar_obj *tar_create_obj(fs_node_t *device, uintptr_t offset) {
	struct tar_obj *obj = kcalloc(1, sizeof(struct tar_obj));
	printf("kcalloc tar_obj: 0x%x\n", (uintptr_t)obj);
	assert(obj != NULL);
	obj->device = device;
	obj->offset = offset;
	obj->flags = FS_NODE_DIRECTORY;

	uint8_t buf[512];
	fs_read(device, offset, 512, buf);
	obj->length = oct2bin(buf + 124, 11);
	obj->name = kmalloc(strlen(buf) + 1);
	assert(obj->name);
	strncpy(obj->name, buf, 100);

	return obj;
}

static fs_node_t *fs_node_from_tar(struct tar_obj *tar_obj) {
	fs_node_t *f = kcalloc(1, sizeof(fs_node_t));
	printf("kcalloc fnode 0x%x\n", (uintptr_t)f);
	assert(f != NULL);
	strncpy(f->name, tar_obj->name, 255);
	f->flags = tar_obj->flags;
	f->object = tar_obj;
	f->open = tar_open;
	f->close = tar_close;
	f->readdir = tar_readdir;
	f->finddir = tar_finddir;
	f->read = tar_read;
	f->length = tar_obj->length;
	return f;
}

static uintptr_t tar_lookup(fs_node_t *device, char *fname) {
	printf("tar_lookup('%s')\n", fname);
	uint8_t buf[512];
	uintptr_t ptr = 0;
	size_t s = 0;
	size_t slen_fname = strlen(fname);
	do {
		s = fs_read(device, ptr, 512, buf);
		if (!memcmp((void *)buf, fname, slen_fname + 1)) {
			return ptr;
		}
		ptr += (((oct2bin(buf + 124, 11) + 511) / 512) + 1) * 512;
	} while (s == 512);
	return -1;
}

static uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	if (offset > node->length) {
		return 0;
	}

	if (offset + size > node->length) {
		size = node->length - offset;
	}

	struct tar_obj *tar_obj = (struct tar_obj *)node->object;

	return fs_read(tar_obj->device, (tar_obj->offset + 512 + offset), size, buffer);
}

static void tar_open(fs_node_t *node, unsigned int flags) {
	(void)node;
	(void)flags;
	printf("tar open\n");
	return;
}

static void tar_close(fs_node_t *node) {
	(void)node;
	printf("tar close\n");
	struct tar_obj *tar_obj = (struct tar_obj *)node->object;
	kfree(tar_obj->name);
	kfree(tar_obj);
	return;
}

// FIXME: implement correctly
static struct dirent *tar_readdir(struct fs_node *node, uint32_t i) {
	printf("tar_readdir(node: 0x%x, i: %u)\n", (uintptr_t)node, i);
	uint8_t buf[512];
	if (i == 0) {
		struct dirent *v = kcalloc(1, sizeof(struct dirent));
		assert(v != NULL);
		v->ino = 0;
		strncpy(v->name, ".", 255);
		return v;
	}
	if (i == 1) {
		struct dirent *v = kcalloc(1, sizeof(struct dirent));
		assert(v != NULL);
		v->ino = 0;
		strncpy(v->name, "..", 255);
		return v;
	}

	uintptr_t ptr = 0;
	size_t s = 0;
	size_t slen_fname = strlen(node->name);
	uint32_t j = 0;
	do {
		s = fs_read(((struct tar_obj *)node->object)->device, ptr, 512, buf);
		if (buf[0] == 0) {
			break;
		}
		if (!memcmp((void *)buf, node->name, slen_fname)) {
			j++;
			if ((i-1) == j) {
				struct dirent *v = kcalloc(1, sizeof(struct dirent));
				assert(v != NULL);
				v->ino = i;
				memcpy(v->name, buf, 100);
				return v;
			}
		}
		ptr += (((oct2bin(buf + 124, 11) + 511) / 512) + 1) * 512; //FIXME: overflows
	} while (s == 512);

	return NULL;
}

static fs_node_t *tar_finddir(struct fs_node *node, char *name) {
	printf("tar_finddir(0x%x, '%s');\n", (uintptr_t)node, name);
	struct tar_obj *obj = (struct tar_obj *)node->object;
	uintptr_t off = tar_lookup(((struct tar_obj *)node->object)->device, name);
	if (off == (unsigned)-1) {
		return NULL;
	} else {
		return fs_node_from_tar(tar_create_obj(obj->device, off));
	}
}

fs_node_t *mount_tar(fs_node_t *device) {
	fs_open(device, 0);

	struct tar_obj *tar_root_obj = kcalloc(1, sizeof(struct tar_obj));
	assert(tar_root_obj != NULL);

	tar_root_obj->device = device;
	tar_root_obj->offset = 0;
	tar_root_obj->name = kmalloc(1);
	tar_root_obj->name[0] = 0;
	tar_root_obj->flags = FS_NODE_DIRECTORY;
//	strncpy(tar_root->name, "tar_root", 255);

	fs_node_t *tar_root = fs_node_from_tar(tar_root_obj);
	fs_open(tar_root, 0);
	return tar_root;
}
