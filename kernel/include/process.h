#ifndef PROCESS_H
#define PROCESS_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cpu.h>
#include <task.h>
#include <fs.h>
#include <vmm.h>

typedef unsigned int pid_t;

typedef struct {
	fs_node_t *node;
	uint32_t seek;
	int32_t __refcount;
} fd_entry_t;

typedef struct {
	fd_entry_t **entries;
	size_t length;
	size_t capacity;
	int32_t __refcount;
} fd_table_t;

typedef struct process {
	uintptr_t kstack;
	size_t kstack_size;
	uintptr_t kstack_top;

	task_t task;
	char *name;
	pid_t pid;
	fd_table_t *fd_table;
} process_t;

extern process_t *current_process;

/* fd_entry_t helpers */
fd_entry_t *fd_reference(fd_entry_t *fd);
fd_entry_t *fd_new(void);
void fd_free(fd_entry_t *fd);

/* fd_table_t helpers */
fd_table_t *fd_table_reference(fd_table_t *fd);
fd_table_t *fd_table_new();
void fd_table_free(fd_table_t *fd_table);
int fd_table_set(fd_table_t *fd_table, unsigned int i, fd_entry_t *entry);
int fd_table_append(fd_table_t *fd_table, fd_entry_t *entry);
fd_entry_t *fd_table_get(fd_table_t *fd_table, unsigned int i);

/* process_t helpers */
void process_exec2(process_t *process, fs_node_t *f, int agrc, const char **argv);
process_t *process_exec(fs_node_t *f, int argc, const char **argv);
void process_destroy(process_t *process);

/* XXX: internal helpers, don't use them unless you know what you're doing  */
void process_init_kstack(process_t *process);
void __attribute__((noreturn)) process_exit(unsigned int status);

void process_add(process_t *process);

#endif
