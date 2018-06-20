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
	fs_node_t *entries[16];
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

typedef void (ktask_func)(void);
void create_ktask(ktask_func func, char *name);
void __attribute__((noreturn)) ktask_exit(unsigned int status);

process_t *kidle_init(void);
process_t *process_exec(fs_node_t *f);
process_t *process_init(uintptr_t start, uintptr_t end);

void process_add(process_t *p);
void process_remove(process_t *p);
void __attribute__((noreturn)) process_enable(void);

#endif
