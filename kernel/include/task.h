#ifndef TASK_H
#define TASK_H 1

#include_next <task.h>
#include <stdbool.h>
#include <stdint.h>
#include <cpu.h>
#include <vmm.h>
#include <atomic.h>

// kernel stack size in pages, including guard pages
#define KSTACK_SIZE 8

enum task_type {
	TASK_TYPE_INVALID      = 0,
	TASK_TYPE_KTASK        = 1,
	TASK_TYPE_USER_PROCESS = 2,
};

enum task_state {
	TASK_STATE_READY = 0,
	TASK_STATE_BLOCKED = 1,
	TASK_STATE_RUNNING = 2,
	TASK_STATE_TERMINATED = 3,
	TASK_STATE_WAITING = 4,
	TASK_STATE_BLOCKED_LOCK = 5,
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
	struct task *next_task;
	enum task_state state;
	uint64_t wait_target;
} task_t;

extern task_t *current_task;

/* task_t kernel stack helpers */
void task_kstack_alloc(task_t *task);
void task_kstack_free(task_t *task);

/* task_t blocking */
typedef struct {
	task_t *first;
	task_t *last;
} task_queue_t;

void task_block(task_queue_t *queue, unsigned int reason);
void task_unblock_next(task_queue_t *queue);

void task_sleep_until(uint64_t target);
void task_sleep_miliseconds(uint64_t time);

/* semaphore_t */
typedef struct {
	unsigned int count;
	task_queue_t blocked_tasks;
	spin_t lock;
} semaphore_t;

void semaphore_acquire(semaphore_t *semaphore);
void semaphore_release(semaphore_t *semaphore);

/* mutex_t */
typedef struct {
	volatile int lock[1];
	// XXX: locked_task might be NULL even when lock is 1
	task_t *locked_task;
	// XXX: only modify after scheduler_lock();
	task_queue_t blocked_tasks;
} mutex_t;

void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

/* yield, don't block but give up the cpu until scheduled again */
void yield(void);

/* everything else */
// XXX: don't use
void _sleep(uint64_t delta);

void task_add(task_t *task);

void scheduler_lock(void);
void scheduler_unlock(void);
// XXX: only call after scheduler_lock
void schedule(void);
void scheduler_wakeup(void);

// XXX: only call after scheduler_lock
__attribute__((noreturn)) void task_exit(void);

/* kmain exit call */
__attribute__((noreturn)) void tasking_enable(void);

#endif
