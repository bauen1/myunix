#ifndef RAMDISK_H
#define RAMDISK_H 1

#include <stddef.h>
#include <stdint.h>
#include <fs.h>

fs_node_t *ramdisk_init(uintptr_t ptr, size_t size);

#endif
