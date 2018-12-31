#ifndef FS_H
#define FS_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <list.h>

/*
 * TODO: promote usage of these macros every where
 */
#define PATH_SEPERATOR '/'
#define PATH_SEPERATOR_STRING "/"

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
	unsigned char type; /* FS_NODE_FLAGS */
	char name[256];
};

struct fs_node;
typedef uint32_t (*read_type_t) (struct fs_node *node, uint32_t offset, uint32_t size, void *buffer);
typedef uint32_t (*write_type_t) (struct fs_node *node, uint32_t offset, uint32_t size, void *buffer);
// XXX: open is called every time something calls fs_open(node)
typedef void (*open_type_t) (struct fs_node *node, unsigned int flags);
// XXX: close is only called on the last call to fs_close(node), just before the node is freed
typedef void (*close_type_t) (struct fs_node *node);
typedef struct dirent *(*readdir_type_t) (struct fs_node *node, uint32_t i);
// XXX: finddir should return a fs_node received by fs_node_new()
typedef struct fs_node *(*finddir_type_t) (struct fs_node *node, char *name);
typedef void (*create_type_t) (struct fs_node *node, char *name, uint16_t permissions);
typedef int (*unlink_type_t) (struct fs_node *node, char *name);
typedef int (*readlink_type_t) (struct fs_node *node, char *buf, size_t size);
typedef void (*mkdir_type_t) (struct fs_node *node, char *name, uint16_t permissions);

typedef struct fs_node {
	char name[256];
	enum fs_node_flags flags;
	void *object;	/* optional driver-specific object */

	size_t length;

	read_type_t read;
	write_type_t write;
	open_type_t open;
	close_type_t close;
	readdir_type_t readdir;
	finddir_type_t finddir;
	create_type_t create;
	unlink_type_t unlink;
	readlink_type_t readlink;
	mkdir_type_t mkdir;

	int32_t __refcount;
} fs_node_t;

/* fs_node_t helpers */
fs_node_t *fs_node_new(void);
void fs_node_release(fs_node_t **node);
fs_node_t *fs_node_reference(fs_node_t *node);

/* mount graph nodes */
struct fs_mount;
typedef struct fs_mount {
	/*
	 * element: path element of this mount
	 */
	char *element;
	/*
	 * node: filesystem node associated, NULL if no mount, but submounts exist
	 */
	fs_node_t *node;
	/*
	 * mounts: TODO: use hashed list
	 * list of sub mounts
	 */
	list_t *mounts;
	/*
	 * __refcount: reference counting, used to implement bind mounts
	 */
	int32_t __refcount;
} fs_mount_t;

extern fs_mount_t *fs_root_mount;

/**
TODO: document quirks
*/
uint32_t fs_read(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer);
/**
*/
uint32_t fs_write(fs_node_t *node, uint32_t offset, uint32_t size, void *buffer);
void fs_open(fs_node_t *node, unsigned int flags);
/*
fs_close:
XXX: will only call node->close before fs_node is freed
*/
void fs_close(fs_node_t **node);
/*
dirent fs_readdir(node, i);
XXX: the caller has to call kfree on the returned value if not null
*/
struct dirent *fs_readdir(struct fs_node *node, uint32_t i);
/*
fs_finddir:
XXX: the caller has to call fs_node_release() on the returned value if not null
*/
struct fs_node *fs_finddir(struct fs_node *node, char *name);
void fs_create (fs_node_t *node, char *name, uint16_t permissions);
/**
fs_unlink, if name=NULL, delete the node, else, try to delete the file 'name' inside the directory
fs_unlink: remove the node named 'name' inside the directory node
*/
int fs_unlink (fs_node_t *node, char *name);

int fs_readlink(fs_node_t *node, char *buf, size_t size);

void fs_mkdir(fs_node_t *node, char *name, uint16_t permission);

void fs_mount_root(fs_node_t *node);

fs_node_t *kopen(const char *path, unsigned int flags);
bool kmount(const char *path, fs_node_t *node);

#endif
