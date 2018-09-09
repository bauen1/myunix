// FIXME: possible memory leaks
#include <assert.h>

#include <console.h>
#include <tar.h>
#include <fs.h>
#include <heap.h>
#include <string.h>
#include <oct2bin.h>

struct tar_header {
	char name[100] __attribute__((packed));
	char mode[8] __attribute__((packed));
	char gid[8] __attribute__((packed));
	char uid[8] __attribute__((packed));
	char fsize[12] __attribute__((pcked));
	char last_mod[12] __attribute__((packed));
	char checksum[8] __attribute__((packed));
	char type __attribute__((packed));
	char linked_name[100] __attribute__((packed));
	char ustar_indicator[6] __attribute__((packed));
	char ustar_version[2] __attribute__((packed));
	char owner_user_name[32] __attribute__((packed));
	char owner_group_name[32] __attribute__((packed));
	char device_major_number[8] __attribute__((packed));
	char device_minor_number[8] __attribute__((packed));
	char filename_prefix[155] __attribute__((packed));
	char __free[12] __attribute__((packed));
} __attribute__((packed));
static_assert(sizeof(struct tar_header) == 512);

struct tar_obj {
	fs_node_t *device;
	char *name;
	uintptr_t offset;
	enum fs_node_flags flags;
	uint32_t length;
};

static uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer);
static void tar_open(fs_node_t *node, unsigned int flags);
static void tar_close(fs_node_t *node);
static struct dirent *tar_readdir(struct fs_node *node, uint32_t i);
static fs_node_t *tar_finddir(struct fs_node *node, char *name);

struct tar_obj *tar_create_obj_from_header(fs_node_t *device, struct tar_header *header, uintptr_t offset) {
	struct tar_obj *obj = kcalloc(1, sizeof(struct tar_obj));
	assert(obj != NULL);
	obj->device = device;
	obj->offset = offset;

	obj->length = oct2bin(header->fsize, 12);
	obj->name = kmalloc(101);
	assert(obj->name != NULL);
	strncpy(obj->name, header->name, 100);
	obj->name[strlen(header->name)] = 0;

	switch((char)header->type) {
		case 0:
		case '0':
			obj->flags = FS_NODE_FILE;
			break;
		case '2':
			obj->flags = FS_NODE_SYMLINK;
			break;
		case '5':
			obj->flags = FS_NODE_DIRECTORY;
			break;
		default:
			printf("Warning: unsupported type ('%c') !\n", header->type);
			obj->flags = 0;
			break;
	}

	if (obj->flags & FS_NODE_DIRECTORY) {
		if (obj->name[strlen(obj->name) - 1] == '/') {
			obj->name[strlen(obj->name) - 1] = 0;
		}
	}

	return obj;
}

static fs_node_t *fs_node_from_tar(struct tar_obj *tar_obj) {
	fs_node_t *f = fs_alloc_node();
	assert(f != NULL);

	// removes the leading path from the name
	// probably really inefficient but hey, it works /shrug
	char *s = tar_obj->name;
	char *d;
	do {
		d = f->name;
		while ((*s != '/') && (*s != 0)) {
			*d = *s;
			s++;
			d++;
		}
		s++;
	} while (*d != 0);

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

static uintptr_t tar_lookup(fs_node_t *device, char *fname, struct tar_header *header) {
	uintptr_t ptr = 0;
	size_t s = 0;
	size_t slen_fname = strlen(fname);
	do {
		s = fs_read(device, ptr, sizeof(struct tar_header), (uint8_t *)header);
		if (s != sizeof(struct tar_header)) {
			printf("tar: %s short read\n", __func__);
			return -1;
		}
		if (!memcmp(header->name, fname, slen_fname)) {
			if (*(header->name + slen_fname + 1) == 0) {
				return ptr;
			} else if ((*(header->name + slen_fname + 1) == '/') && (*(header->name + slen_fname + 2) == 0)) {
				return ptr;
			}
		}
		uintptr_t jump = oct2bin(header->fsize, 12);
		if ((jump % 512) != 0) {
			jump += 512 - (jump % 512);
		}
		ptr += 512 + jump;
	} while (s == 512);
	return -1;
}

static uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer) {
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
	(void)node; (void)flags;
	return;
}

static void tar_close(fs_node_t *node) {
	struct tar_obj *tar_obj = (struct tar_obj *)node->object;
	kfree(tar_obj->name);
	tar_obj->name = NULL;
	kfree(tar_obj);
}

static struct dirent *tar_readdir(struct fs_node *node, uint32_t i) {
	struct tar_header header;
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
		v->ino = 1;
		strncpy(v->name, "..", 255);
		return v;
	}

	uintptr_t ptr = 0;
	size_t s = 0;
	size_t slen_fname = strlen(node->name);
	uint32_t j = 0;
	do {
		fs_node_t *device = ((struct tar_obj *)node->object)->device;
		s = fs_read(device, ptr, sizeof(struct tar_header), (uint8_t *)&header);
		if (s != sizeof(struct tar_header)) {
			printf("tar: %s short read\n", __func__);
			break;
		}

		if (header.name[0] == 0) {
			break;
		}

		if ((header.name[0] == '.') && (header.name[1] == '/') && (header.name[2] == 0)) {
			// ignore './' root directory entry
		} else if (!memcmp(header.name, node->name, slen_fname)) {
			// this complicated mess is needed to handle subdirectories correctly
			char subname[100];
			memset(subname, 0, 100);
			if (header.name[slen_fname] == '/') {
				strncpy(subname, (const char *)&header.name[slen_fname + 1], 100);
			} else {
				strncpy(subname, (const char *)&header.name[slen_fname], 100);
			}
			// ignore the directory entry for the node
			if (subname[0] != 0) {
				char *s = &subname[0];
				while (1) {
					if (*s == '/') {
						if (*(s+1) == 0) {
							*s = 0;
							j++;
							break;
						} else { // subdir
							break;
						}
					}
					if (*s == 0) {
						j++;
						break;
					}
					s++;
				}
				if ((i-1) == j) {
					struct dirent *v = kcalloc(1, sizeof(struct dirent));
					assert(v != NULL);
					v->ino = i;
					strncpy(v->name, subname, 100);
					return v;
				}
			}
		}
		uintptr_t jump = oct2bin(header.fsize, 12);
		if ((jump % 512) != 0) {
			jump += 512 - (jump % 512);
		}
		ptr += sizeof(struct tar_header) + jump;
	} while (s == 512);

	return NULL;
}

static fs_node_t *tar_finddir(struct fs_node *node, char *name) {
	struct tar_obj *obj = (struct tar_obj *)node->object;
	struct tar_header header;

	// rebuild the full path
	size_t len = strlen(obj->name) + strlen(name) + 1 + 1;
	char *path = kmalloc(len);
	strncpy(path, obj->name, len);
	char *path1 = path + strlen(obj->name);
	if (strlen(obj->name) != 0) {
		*path1++ = '/';
	}
	strncpy(path1, name, strlen(name) + 1);
	path1 += strlen(name);
	*path1 = 0;

	uintptr_t off = tar_lookup(((struct tar_obj *)node->object)->device, path, &header);

	if (off == (unsigned)-1) {
		return NULL;
	} else {
		struct tar_obj *obj2 = tar_create_obj_from_header(obj->device, &header, off);
		return fs_node_from_tar(obj2);
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

	fs_node_t *tar_root = fs_node_from_tar(tar_root_obj);
	fs_open(tar_root, 0);
	return tar_root;
}
