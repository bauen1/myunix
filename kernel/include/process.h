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
} fd_entry_t;

typedef struct {
	fd_entry_t **entries;
	size_t length;
	size_t capacity;
} fd_table_t;

typedef struct process {
	uintptr_t kstack;
	size_t kstack_size;
	uintptr_t kstack_top;

	registers_t *regs;
	uint32_t esp, ebp, eip;
	page_directory_t *pdir;
	char *name;
	pid_t pid;
	fd_table_t *fd_table;
	bool is_kernel_task;
} process_t;

extern process_t *current_process;

typedef int (ktask_func)(void *extra, char *name);
void create_ktask(ktask_func func, char *name, void *extra);

process_t *process_exec(fs_node_t *f, int argc, char **argv);
void process_destroy(process_t *process);

void process_add(process_t *p);
void process_remove(process_t *p);
void __attribute__((noreturn)) process_exit(unsigned int status);
void __attribute__((noreturn)) process_enable(void);

/* private helpers, don't use them */
void process_init_kernel_kstack(process_t *process);
void process_init_kstack(process_t *process);

#endif
