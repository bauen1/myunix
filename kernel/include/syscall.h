#ifndef SYSCALL_H
#define SYSCALL_H 1

#include <stdint.h>
#include <process.h>

void syscall_init();

intptr_t copy_from_userspace(page_directory_t *pdir, uintptr_t ptr, size_t n, void *buffer);
intptr_t copy_to_userspace(page_directory_t *pdir, uintptr_t ptr, size_t n, const void *buffer);

#endif
