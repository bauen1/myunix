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
	char fsize[12] __attribute__((packed));
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
static int tar_readlink(fs_node_t *node, char *buf, size_t size);

static size_t tar_entry_length(struct tar_header *header) {
	return (((oct2bin(header->fsize, 12) + 511) / 512) + 1) * 512;
}

struct tar_obj *tar_create_obj_from_header(fs_node_t *device, struct tar_header *header, uintptr_t offset) {
	struct tar_obj *obj = kcalloc(1, sizeof(struct tar_obj));
	if (obj == NULL) {
		return NULL;
	}
	obj->device = device;
	obj->offset = offset;

	obj->length = oct2bin(header->fsize, 12);
	obj->name = strndup(header->name, 100);
	if (obj->name == NULL) {
		kfree(obj);
		return NULL;
	}

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
			printf("tar: %s: unsupported file type ('%c' (0x%x))!\n", __func__, header->type, header->type);
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
	fs_node_t *f = fs_node_new();
	if (f == NULL) {
		return NULL;
	}

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
	f->readlink = tar_readlink;
	f->length = tar_obj->length;
	return f;
}

// iterate over every tar entry using the backing device 'device' and writing the header to 'dest', declares 'ptr' and 's' in the loop scope
// it's a bit complicated but simplifies a lot of the code beyond here
#define tar_foreach_entry(device, header) for ( \
	uintptr_t ptr = 0, s = fs_read((device), ptr, sizeof(struct tar_header), (uint8_t *)(header)); \
	s == sizeof(struct tar_header); \
	ptr += tar_entry_length((header)), s = fs_read((device), ptr, sizeof(struct tar_header), (uint8_t *)(header)) \
)


static uintptr_t tar_lookup(fs_node_t *device, char *fname, struct tar_header *header) {
	const size_t fname_len = strlen(fname);
	if (fname_len > (100 - 2)) {
		return -1;
	}

	tar_foreach_entry(device, header) {
		if (!memcmp(header->name, fname, fname_len)) {
			if (header->name[fname_len + 1] == 0) {
				return ptr;
			} else if ((header->name[fname_len + 1] == '/') && (header->name[fname_len + 2] == 0)) {
				return ptr;
			}
		}
	}
	return -1;
}

static uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer) {
	struct tar_obj *tar_obj = (struct tar_obj *)node->object;
	assert(tar_obj != NULL);

	if (offset > node->length) {
		return 0;
	}

	if (offset + size > node->length) {
		size = node->length - offset;
	}

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

// FIXME: overflow checks
static struct dirent *tar_readdir(struct fs_node *node, uint32_t i) {
	struct tar_obj *tar_obj = node->object;
	assert(tar_obj != NULL);
	fs_node_t *device = tar_obj->device;
	assert(device != NULL);

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

	char *fname = tar_obj->name;
	const size_t fname_len = strlen(fname);

	// inode counter
	uint32_t j = 1;
	struct tar_header header;
	tar_foreach_entry(device, &header) {
		if (!memcmp(header.name, "./", 3)) {
			// ignore './' root directory entry
		} else if (!memcmp(header.name, fname, fname_len)) {
			// this inode matches the leading path

			// XXX: this complicated mess is needed to handle subdirectories correctly
			// FIXME: strncpy maximum length of 100 isn't correct, we need to caclulate it
			char subname[100]; // everything after the fname
			memset(subname, 0, 100);

			// strip leading '/' of subname if needed
			if (header.name[fname_len] == '/') {
				strncpy(subname, (const char *)&header.name[fname_len + 1], 100);
			} else {
				strncpy(subname, (const char *)&header.name[fname_len], 100);
			}

			// XXX: if the headers name ends with '/\0' it is the entrie for the director we are searching in, ignore it and don't count it
			if (subname[0] == 0) {
				continue;
			}

			const size_t subname_plen = path_element_size(subname);
			if (subname[subname_plen] == '/') {
				// inode of a directory found
				if (subname[subname_plen + 1] != 0) {
					// entry of the directory, ignore
					continue;
				}
				// remove the trailing /
				subname[subname_plen] = 0;
			}
			j++;

			// check if we have found the entry we need
			if (j == i) {
				struct dirent *v = kcalloc(1, sizeof(struct dirent));
				assert(v != NULL);
				v->ino = i;
				strncpy(v->name, subname, 100);
				return v;
			}
		}
	}

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

	if (off == (uintptr_t)-1) {
		return NULL;
	} else {
		struct tar_obj *obj2 = tar_create_obj_from_header(obj->device, &header, off);
		assert(obj2 != NULL);
		fs_node_t *f = fs_node_from_tar(obj2);
		assert(f != NULL);
		return f;
	}
}

static int tar_readlink(fs_node_t *node, char *buf, size_t size) {
	struct tar_obj *obj = (struct tar_obj *)node->object;
	assert(obj->flags & FS_NODE_SYMLINK);

	struct tar_header header;
	size_t s = fs_read(obj->device, obj->offset, sizeof(struct tar_header), (uint8_t *)&header);
	if (s != sizeof(struct tar_header)) {
		printf("tar: %s: short read\n", __func__);
		return -1;
	}

	const size_t linked_name_len = strlen(header.linked_name) + 1;

	if (size < linked_name_len) {
		return -1;
	} else {
		memcpy(buf, header.linked_name, linked_name_len);
		return linked_name_len - 1;
	}
}

fs_node_t *mount_tar(fs_node_t *device) {
	struct tar_obj *tar_root_obj = kcalloc(1, sizeof(struct tar_obj));
	assert(tar_root_obj != NULL);

	fs_open(device, 0);
	tar_root_obj->device = device;
	tar_root_obj->offset = 0;
	tar_root_obj->name = kmalloc(1);
	assert(tar_root_obj->name != NULL);
	tar_root_obj->name[0] = 0;
	tar_root_obj->flags = FS_NODE_DIRECTORY;
	fs_node_t *f = fs_node_from_tar(tar_root_obj);
	assert(f != NULL);
	return f;
}
