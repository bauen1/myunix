#ifndef TMPFS_H
#define TMPFS_H 1

#include <fs.h>

void tmpfs_register(fs_node_t *tmpfs_root, fs_node_t *node);
fs_node_t *mount_tmpfs();

#endif
