#ifndef PROCESS_H
#define PROCESS_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cpu.h>
#include <task.h>
#include <fs.h>
#include <vmm.h>
#include <tree.h>

#define PROCESS_MAX_PID 32768

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
	task_t task;
	char *name;
	pid_t pid;
	fd_table_t *fd_table;
	tree_node_t *ptree_node;
	task_queue_t wait_queue;
} process_t;

#define current_process ((process_t *)(current_task->type == TASK_TYPE_USER_PROCESS ? current_task->obj : NULL))

/* fd_entry_t helpers */
fd_entry_t *fd_reference(fd_entry_t *fd);
fd_entry_t *fd_new(void);
void fd_free(fd_entry_t *fd);

/* fd_table_t helpers */
fd_table_t *fd_table_reference(fd_table_t *fd);
fd_table_t *fd_table_clone(fd_table_t *oldfdt);
fd_table_t *fd_table_new(void);
void fd_table_free(fd_table_t *fd_table);
int fd_table_set(fd_table_t *fd_table, unsigned int i, fd_entry_t *entry);
int fd_table_append(fd_table_t *fd_table, fd_entry_t *entry);
fd_entry_t *fd_table_get(fd_table_t *fd_table, unsigned int i);

/* process_t helpers */
void process_execve(process_t *process, fs_node_t *f, size_t argc, char * const * const argv, size_t envc, char * const * const envp);
process_t *process_spawn_init(fs_node_t *f, size_t argc, char * const * const argv);
void process_destroy(process_t *process);
pid_t get_pid(void);

void __attribute__((noreturn)) process_exit(unsigned int status);
enum syscall_clone_flags {
	SYSCALL_CLONE_FLAGS_VM      = 0x00000100,
	SYSCALL_CLONE_FLAGS_FS      = 0x00000200,
	SYSCALL_CLONE_FLAGS_FILES   = 0x00000400,
	SYSCALL_CLONE_FLAGS_SIGHAND = 0x00000800,
	SYSCALL_CLONE_FLAGS_PTRACE  = 0x00002000,
	SYSCALL_CLONE_FLAGS_VFORK   = 0x00004000,
	SYSCALL_CLONE_FLAGS_PARENT  = 0x00008000,
	SYSCALL_CLONE_FLAGS_THREAD  = 0x00010000,
	SYSCALL_CLONE_FLAGS_NEWNS   = 0x00020000,
	SYSCALL_CLONE_FLAGS_SYSVSEM = 0x00040000,
	SYSCALL_CLONE_FLAGS_SETTLS  = 0x00080000,
	SYSCALL_CLONE_FLAGS_SETTID  = 0x00100000,
	SYSCALL_CLONE_FLAGS_CLEARTID= 0x00200000,
	SYSCALL_CLONE_FLAGS_DETACHED= 0x00400000,
	SYSCALL_CLONE_FLAGS_UNTRACED= 0x00800000,
	SYSCALL_CLONE_CHILD_SETTID  = 0x01000000,
	SYSCALL_CLONE_NEWCGROUP     = 0x02000000,
	SYSCALL_CLONE_NEWUTS        = 0x04000000,
	SYSCALL_CLONE_NEWIPC        = 0x08000000,
	SYSCALL_CLONE_NEWUSER       = 0x10000000,
	SYSCALL_CLONE_NEWPID        = 0x20000000,
	SYSCALL_CLONE_NEWNET        = 0x40000000,
	SYSCALL_CLONE_IO            = 0x80000000,
};
process_t *process_clone(process_t *oldproc, enum syscall_clone_flags flags, uintptr_t child_stack);

void process_add(process_t *process);
void process_init(void);

uint32_t process_waitpid(pid_t pid, uint32_t status, uint32_t options);

#endif
