// FIXME: memory leaks

#include <console.h>
#include <tar.h>
#include <fs.h>
#include <heap.h>
#include <string.h>
#include <oct2bin.h>

uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t tar_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void tar_open(fs_node_t *node, unsigned int flags);
void tar_close(fs_node_t *node);
struct dirent *tar_readdir(struct fs_node *node, uint32_t i);
fs_node_t *tar_finddir(struct fs_node *node, char *name);

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
	uintptr_t offset;
};

fs_node_t *fs_node_from_tar(struct tar_obj *tar_obj, uintptr_t offset) {
	uint8_t buf[512];
	fs_node_t *v = (fs_node_t *)kcalloc(1, sizeof(fs_node_t));
	struct tar_obj *new_obj = (struct tar_obj *)kcalloc(1, sizeof(struct tar_obj));

	new_obj->device = tar_obj->device;
	new_obj->offset = offset;

	fs_read(new_obj->device, new_obj->offset, 512, buf);

	v->object = new_obj;
	v->length = oct2bin(buf + 124, 11);
	memcpy(v->name, buf, 100);
	v->read = tar_read;
	v->write = tar_write;
	v->open = tar_open;
	v->close = tar_close;
	v->readdir = tar_readdir;
	v->finddir = tar_finddir;
	return v;
}

static uintptr_t tar_lookup(fs_node_t *device, char *fname) {
	uint8_t buf[512];
	uintptr_t ptr = 0;
	size_t s = 0;
	size_t slen_fname = strlen(fname);
	do {
		s = fs_read(device, ptr, 512, buf);
		if (!memcmp((void *)buf, fname, slen_fname + 1)) {
			return ptr;
		}
		ptr += (((oct2bin(buf + 124, 11) + 511) / 512) + 1) * 512; //FIXME: overflows
	} while (s == 512);
	return -1;
}

uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	if (offset > node->length) {
		return 0;
	}

	if (offset + size > node->length) {
		size = node->length - offset;
	}

	struct tar_obj *tar_obj = (struct tar_obj *)node->object;

	return fs_read(tar_obj->device, (tar_obj->offset + 512 + offset), size, buffer);
}

// read only, always return -1
uint32_t tar_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
	(void)node;
	(void)offset;
	(void)size;
	(void)buffer;
	return -1;
}

// FIXME: implement
void tar_open(fs_node_t *node, unsigned int flags) {
	(void)node;
	(void)flags;
	return;
}

// FIXME: implement
void tar_close(fs_node_t *node) {
	(void)node;
	return;
}

// FIXME: implement correctly
struct dirent *tar_readdir(struct fs_node *node, uint32_t i) {
	uint8_t buf[512];
	if (i == 0) {
		struct dirent *v = (struct dirent *)kcalloc(1, sizeof(struct dirent));
		v->ino = 0;
		strncpy(v->name, ".", 255);
		return v;
	}
	if (i == 1) {
		struct dirent *v = (struct dirent *)kcalloc(1, sizeof(struct dirent));
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
				struct dirent *v = (struct dirent *)kcalloc(1, sizeof(struct dirent));
				v->ino = i;
				memcpy(v->name, buf, 100);
				return v;
			}
		}
		ptr += (((oct2bin(buf + 124, 11) + 511) / 512) + 1) * 512; //FIXME: overflows
	} while (s == 512);

	return NULL;
}

fs_node_t *tar_finddir(struct fs_node *node, char *name) {
	uintptr_t off = tar_lookup(((struct tar_obj *)node->object)->device, name);
	if (off == (unsigned)-1) {
		return NULL;
	} else {
		return fs_node_from_tar(node->object, off);
	}
}

fs_node_t *mount_tar(fs_node_t *device) {
	fs_node_t *tar_root = (fs_node_t *)kcalloc(1, sizeof(fs_node_t));
	struct tar_obj *tar_root_obj = (struct tar_obj *)kcalloc(1, sizeof(struct tar_obj));

	tar_root_obj->device = device;
	tar_root_obj->offset = 0;

	strncpy(tar_root->name, "tar_root", 255);
	tar_root->read = tar_read;
	tar_root->write = tar_write;
	tar_root->open = tar_open;
	tar_root->close = tar_close;
	tar_root->readdir = tar_readdir;
	tar_root->finddir = tar_finddir;
	tar_root->object = tar_root_obj;
	fs_open(device, 0);

	return tar_root;
}
