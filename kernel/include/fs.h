#ifndef FS_H
#define FS_H 1

#include <stddef.h>
#include <stdint.h>

struct dirent {
	uint32_t ino;
	char name[256];
};

struct fs_node;
typedef uint32_t (*read_type_t) (struct fs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef uint32_t (*write_type_t) (struct fs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef void (*open_type_t) (struct fs_node *node, unsigned int flags);
typedef void (*close_type_t) (struct fs_node *node);
typedef struct dirent *(*readdir_type_t) (struct fs_node *node, uint32_t i);
typedef struct fs_node *(*finddir_type_t) (struct fs_node *node, char *name);

typedef struct fs_node {
	char name[256];
	void *object;	/* optional driver-specific object */

	size_t length;

	read_type_t read;
	write_type_t write;
	open_type_t open;
	close_type_t close;
	readdir_type_t readdir;
	finddir_type_t finddir;

	// TODO: garbage collect
} fs_node_t;

extern fs_node_t *fs_root;

uint32_t fs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t fs_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void fs_open(fs_node_t *node, unsigned int flags);
void fs_close(fs_node_t *node);
struct dirent *fs_readdir(struct fs_node *node, uint32_t i);
struct fs_node *fs_finddir(struct fs_node *node, char *name);

void fs_mount_root(fs_node_t *node);

#endif
