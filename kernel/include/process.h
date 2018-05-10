#ifndef PROCESS_H
#define PROCESS_H 1

#include <stddef.h>
#include <stdint.h>
#include <cpu.h>
#include <task.h>
#include <fs.h>

typedef unsigned int pid_t;

typedef struct {
	fs_node_t *entries[16];
} fd_table_t;

typedef struct process {
	uintptr_t kstack;
	registers_t *regs;
	uint32_t esp, ebp, eip;
	uint32_t *pdir;
	const char *name;
	pid_t pid;
	fd_table_t *fd_table;
} process_t;

process_t *current_process;

process_t *kidle_init();
void process_add(process_t *p);
process_t *process_init(uintptr_t start, uintptr_t end);
void process_test(uintptr_t kidle_esp, uintptr_t mod0_start, uintptr_t mod0_end,
	uintptr_t mod1_start, uintptr_t mod1_end);
void __attribute__((noreturn)) process_enable(void);

#endif
