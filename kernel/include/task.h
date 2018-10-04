#ifndef TASK_H
#define TASK_H 1

#include_next <task.h>
#include <stdbool.h>
#include <stdint.h>
#include <cpu.h>
#include <vmm.h>

// kernel stack size in pages
// XXX: this includes the guard pages
#define KSTACK_SIZE 8

enum task_type {
	TASK_TYPE_INVALID      = 0,
	TASK_TYPE_KTASK        = 1,
	TASK_TYPE_USER_PROCESS = 2,
};

typedef struct task {
	registers_t *registers;
	uint32_t esp, ebp, eip;
	/* XXX: kstack points to the top of the kerel stack */
	uintptr_t kstack;
	/* XXX: it's the clients responsibility to map the kernel stack into pdir */
	page_directory_t *pdir;
	enum task_type type;
	void *obj;
} task_t;

extern task_t *current_task;

/* task_t helpers */
void task_kstack_alloc(task_t *task);
void task_kstack_free(task_t *task);
void task_kstack_delayed_free(task_t *task);

void task_add(task_t *task);
void task_remove(task_t *task);

void __attribute__((noreturn)) tasking_enable(void);

#endif
