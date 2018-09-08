#ifndef FS_H
#define FS_H 1

#include <stddef.h>
#include <stdint.h>

#define PATH_SEPERATOR '/'

enum fs_node_flags {
	FS_NODE_FILE        = 0x01,
	FS_NODE_DIRECTORY   = 0x02,
	FS_NODE_CHARDEVICE  = 0x04,
	FS_NODE_BLOCKDEVICE = 0x08,
	FS_NODE_PIPE        = 0x10,
	FS_NODE_SYMLINK     = 0x20,
	FS_NODE_MOUNTPOINT  = 0x40,
};

struct dirent {
	uint32_t ino;
	char name[256];
};

struct fs_node;
typedef uint32_t (*read_type_t) (struct fs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef uint32_t (*write_type_t) (struct fs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef void (*open_type_t) (struct fs_node *node, unsigned int flags); // called everytime a program opens the file node
typedef void (*close_type_t) (struct fs_node *node); // called when the last program closes a file node, free all driver objects related
typedef struct dirent *(*readdir_type_t) (struct fs_node *node, uint32_t i);
typedef struct fs_node *(*finddir_type_t) (struct fs_node *node, char *name);
typedef void (*create_type_t) (struct fs_node *node, char *name, uint16_t permissions);
typedef void (*unlink_type_t) (struct fs_node *node, char *name);
typedef void (*mkdir_type_t) (struct fs_node *node, char *name, uint16_t permissions);

typedef struct fs_node {
	char name[256];
	enum fs_node_flags flags;
	void *object;	/* optional driver-specific object */

	// TODO: symlink support

	size_t length;

	read_type_t read;
	write_type_t write;
	open_type_t open;
	close_type_t close;
	readdir_type_t readdir;
	finddir_type_t finddir;
	create_type_t create;
	unlink_type_t unlink;
	mkdir_type_t mkdir;

	// TODO: garbage collect
	int32_t __refcount;
} fs_node_t;

extern fs_node_t *fs_root;
/* allocating / freeing a node */
fs_node_t *fs_alloc_node();
void fs_free_node(fs_node_t **node);

uint32_t fs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t fs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void fs_open(fs_node_t *node, unsigned int flags);
void fs_close(fs_node_t *node);
struct dirent *fs_readdir(struct fs_node *node, uint32_t i);
struct fs_node *fs_finddir(struct fs_node *node, char *name);
void fs_create (fs_node_t *node, char *name, uint16_t permissions);
void fs_unlink (fs_node_t *node, char *name);
void fs_mkdir(fs_node_t *node, char *name, uint16_t permission);

void fs_mount_root(fs_node_t *node);

#endif
