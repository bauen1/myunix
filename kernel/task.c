#include <assert.h>
#include <stdbool.h>

#include <pit.h>
#include <console.h>
#include <gdt.h>
#include <pmm.h>
#include <process.h>
#include <string.h>
#include <task.h>
#include <vmm.h>

#define assert_panic(exp) if (!(exp)) { interrupts_disable(); __asm__ __volatile__("hlt");}
#define assert_interrupts_disabled() do {uint32_t f;__asm__ __volatile__("pushf\npop %0":"=r"(f)); assert_panic(!(f & 1<<9)); }while(0)

/* kstack helpers */
void task_kstack_alloc(task_t *task) {
	assert(task != NULL);

	uintptr_t kstack = find_vspace(kernel_directory, KSTACK_SIZE);
	assert(kstack != 0);

	// guard page
	map_page(get_table(kstack, kernel_directory), kstack, PAGE_VALUE_GUARD, 0);
	invalidate_page(kstack);
	kstack += BLOCK_SIZE;

	for (size_t i = 0; i < (KSTACK_SIZE - 2); i++) {
		uintptr_t vkaddr = kstack + i * BLOCK_SIZE;
		uintptr_t phys = pmm_alloc_blocks_safe(1);
		map_page(get_table(vkaddr, kernel_directory), vkaddr, phys,
			PAGE_PRESENT | PAGE_READWRITE);
		invalidate_page(vkaddr);
		memset((void *)vkaddr, 0, BLOCK_SIZE);
	}

	kstack += (KSTACK_SIZE - 2) * BLOCK_SIZE;
	task->kstack = kstack;

	map_page(get_table(kstack, kernel_directory), kstack, PAGE_VALUE_GUARD, 0);
	invalidate_page(kstack);
}

void task_kstack_free(task_t *task) {
	assert(task != NULL);
	uintptr_t kstack = task->kstack;
	assert(kstack != 0);

	assert(get_page(get_table_alloc(kstack, kernel_directory), kstack) == PAGE_VALUE_GUARD);
	map_page(get_table_alloc(kstack, kernel_directory), kstack, 0, 0);
	invalidate_page(kstack);
	kstack -= BLOCK_SIZE;

	for (size_t i = 0; i < (KSTACK_SIZE - 2); i++) {
		uintptr_t vkaddr = kstack - i * BLOCK_SIZE;
		uintptr_t kpage = get_page(get_table_alloc(vkaddr, kernel_directory), vkaddr);
		assert(kpage & PAGE_PRESENT);
		map_page(get_table_alloc(vkaddr, kernel_directory), vkaddr, 0, 0);
		invalidate_page(vkaddr);
		pmm_free_blocks(kpage & ~0xFFF, 1);
	}

	kstack -= (KSTACK_SIZE - 2) * BLOCK_SIZE;

	assert(get_page(get_table_alloc(kstack, kernel_directory), kstack) == PAGE_VALUE_GUARD);
	map_page(get_table_alloc(kstack, kernel_directory), kstack, 0, 0);
	invalidate_page(kstack);

	task->kstack = 0;
}

/* task_queue_t helpers */
static void task_queue(task_queue_t *queue, task_t *task) {
	assert_interrupts_disabled();
	assert(queue != NULL);
	assert(task != NULL);
	assert(task->next_task == NULL);

	if (queue->first == NULL) {
		assert(queue->last == NULL);
		queue->first = task;
		queue->last = task;
	} else {
		assert(queue->last != NULL);
		queue->last->next_task = task;
		queue->last = task;
	}
}

static task_t *task_dequeue(task_queue_t *queue) {
	assert_interrupts_disabled();
	assert(queue != NULL);
	if (queue->first == NULL) {
		return NULL;
	}

	task_t *task = queue->first;
	queue->first = task->next_task;
	if (queue->first == NULL) {
		assert(task->next_task == NULL);
		assert(queue->last == task);
		queue->last = NULL;
	}
	task->next_task = NULL;
	return task;
}

/* actual scheduler logic */
task_t *current_task;
task_queue_t ready_queue;
task_queue_t wait_queue;
task_queue_t terminated_queue;

bool scheduler_ready = false;
volatile unsigned int scheduler_lock_count = 0;
bool postponed_schedule = false;
bool enable_ints = false;

// TODO: rewrite partially in assembly
__attribute__((noreturn)) static void __restore_task(task_t *task) {
	assert_interrupts_disabled();
	assert(scheduler_lock_count == 0);

	tss_set_kstack(task->kstack);

	uint32_t esp = task->esp;
	uint32_t ebp = task->ebp;
	uint32_t eip = task->eip;

	__asm__ __volatile__ (
		"mov %0, %%ebx\n"
		"mov %1, %%esp\n"
		"mov %2, %%ebp\n"
		"mov $0, %%eax\n"
		"jmp *%%ebx"
		:
		: "r" (eip), "r" (esp), "r" (ebp)
		: "ebx", "esp", "eax");
#ifndef __TINYC__
	__builtin_unreachable();
#endif
}

/* switch task and return when scheduled again
 * XXX: does not add the task to the ready queue
 * XXX: only call with interrupts off
 * XXX: don't call directly, use call_switch_task(void *);
 * TODO: rewrite partially in assembly
 */
__attribute__((used)) void switch_task(task_t *new_task) {
	assert_interrupts_disabled();
	assert(current_task != NULL);
	assert(new_task != NULL);
	assert(scheduler_lock_count == 0);

	uint32_t esp, ebp;
	__asm__ __volatile__("mov %%esp, %0\n"
	                     "mov %%ebp, %1\n" : "=r"(esp), "=r"(ebp));
	uint32_t eip = read_eip();
	if (eip == 0) {
		return;
	}
	current_task->esp = esp;
	current_task->ebp = ebp;
	current_task->eip = eip;

	assert_panic(new_task->state == TASK_STATE_RUNNING);
	current_task = new_task;
	__restore_task(new_task);
}

static void __schedule(void) {
	assert_interrupts_disabled();
	assert(scheduler_lock_count == 0);

	task_t *next_task = current_task;
	if (current_task != NULL) {
		assert_panic(current_task->state != TASK_STATE_READY);
	}

	if (ready_queue.first != NULL) {
		// XXX: switch to next ready task
		next_task = task_dequeue(&ready_queue);
		assert(next_task != current_task);
		assert(next_task != NULL);
	} else {
		if (current_task != NULL) {
			if (current_task->state == TASK_STATE_RUNNING) {
				// XXX: only process that wants cpu so don't bother doing anything
			} else {
				// XXX: idle
				next_task = current_task;
				current_task = NULL;
				scheduler_lock_count++;
				do {
					interrupts_enable();
					__asm__ __volatile__("hlt");
					interrupts_disable(); //XXX: need to be disabled for while check below
				} while (ready_queue.first == NULL);
				scheduler_lock_count--;
				assert(scheduler_lock_count == 0);
				current_task = next_task;
				next_task = task_dequeue(&ready_queue);
				assert_panic(next_task != NULL);
			}
		} else {
			// XXX: already idleing, an interrupt handler called __schedule, in short: we're fucked
			assert(0);
		}
	}

	if (current_task != next_task) {
		if (current_task != NULL) {
			if (current_task->state == TASK_STATE_RUNNING) {
				// XXX: task got preempted
				current_task->state = TASK_STATE_READY;
				task_queue(&ready_queue, current_task);
			} else {
				// XXX: task blocked or something else

			}
			assert(next_task->state == TASK_STATE_READY);
			next_task->state = TASK_STATE_RUNNING;
			call_switch_task(next_task);
		} else {
			printf("%s: WARN: terminating current task\n", __func__);
			// XXX: previous task terminated
			// XXX: ensure we don't get here if current_task == NULL because of idle
			current_task = next_task;
			assert(next_task->state == TASK_STATE_READY);
			current_task->state = TASK_STATE_RUNNING;
			__restore_task(current_task);
		}
	} else {
		assert(current_task != NULL);
		current_task->state = TASK_STATE_RUNNING;
	}
}

void scheduler_lock(void) {
	__attribute__((used)) uint32_t eflags;
	__asm__ __volatile__ ("pushf\n"
	                      "pop %0\n"
	                      : "=r"(eflags));
	interrupts_disable();
	if (scheduler_lock_count == 0) {
		if (eflags & (1<<9)) {
			// XXX: interrupts where enabled when called, enable them on exit too
			enable_ints = true;
		}
	}
	scheduler_lock_count++;
}

void scheduler_unlock(void) {
	assert_interrupts_disabled();
	scheduler_lock_count--;
	if (scheduler_lock_count == 0) {
		if ((scheduler_ready) && postponed_schedule) {
			postponed_schedule = false;
			__schedule();
		}
		if (enable_ints) {
			enable_ints = false;
			interrupts_enable();
		}
	}
}

/*
 * XXX: only call with schedule_lock()
 * TODO: remove, it's kinda useless
 */
void schedule(void) {
	assert_interrupts_disabled();
	assert(scheduler_lock_count != 0);
	assert(scheduler_ready);
	postponed_schedule = true;
}

void yield(void) {
	scheduler_lock();
	schedule();
	scheduler_unlock();
}

/* task blocking helpers */
static void __task_block(task_queue_t *queue, task_t *task, unsigned int reason) {
	assert_panic(scheduler_lock_count != 0);
	assert_interrupts_disabled();
	assert_panic(task != NULL);
	assert_panic(queue != NULL);

	task->state = reason;
	task_queue(queue, task);
}

void task_block(task_queue_t *queue, unsigned int reason) {
	assert(queue != NULL);
	scheduler_lock();
	assert_panic(scheduler_lock_count == 1);
	__task_block(queue, current_task, reason);
	schedule();
	scheduler_unlock();
}

static void __task_unblock(task_t *task) {
	assert_panic(scheduler_lock_count != 0);
	assert_interrupts_disabled();
	assert_panic(task != NULL);

	task->state = TASK_STATE_READY;
	task_queue(&ready_queue, task);
}

void task_unblock_next(task_queue_t *queue) {
	assert(queue != NULL);
	scheduler_lock();
	task_t *task = task_dequeue(queue);
	if (task != NULL) {
		__task_unblock(task);
		schedule();
	}
	scheduler_unlock();
}

/* task sleep helpers */
// TODO: implement sleep delta list https://wiki.osdev.org/Blocking_Process

void task_sleep_until(uint64_t target) {
	printf("%s: task %p sleeping until %u\n", __func__, current_task, (unsigned int)target);

	scheduler_lock();
	if (target <= timer_ticks) {
		scheduler_unlock();
		return;
	}

	current_task->state = TASK_STATE_WAITING;
	current_task->wait_target = target;
	task_queue(&wait_queue, current_task);
	schedule();
	scheduler_unlock();
	assert(scheduler_lock_count == 0);
}

/*
 * scheduler_wakeup is called by a timer to wake up sleeping tasks
 * and to preempt the current running task
 */
void scheduler_wakeup(void) {
	if (scheduler_ready) {
		scheduler_lock();
		task_t *task = wait_queue.first;
		wait_queue.first = NULL;
		wait_queue.last = NULL;
		while (task != NULL) {
			task_t *next_task = task->next_task;
			if (task->wait_target <= timer_ticks) {
				// XXX: hey, wake up
				task->state = TASK_STATE_READY;
				task->next_task = NULL;
				task_queue(&ready_queue, task);
			} else {
				// XXX: put it back
				// FIXME: inefficient
				task_queue(&wait_queue, task);
			}
			task = next_task;
		}
		schedule();
/*
	if (scheduler_lock_count == 1) {
		// XXX: first level IRQ
		schedule();
	} else if (scheduler_lock_count == 2) {
		// XXX: second level IRQ do not schedule
	} else {
		assert_panic(0);
	}
*/
		scheduler_unlock();
	}
}

/* semaphore_t helpers */
void semaphore_acquire(semaphore_t *semaphore) {
	assert(semaphore != NULL);

	spin_lock(semaphore->lock);
	if (semaphore->count > 0) {
		semaphore->count--;
	} else {
		scheduler_lock();
		__task_block(&semaphore->blocked_tasks, current_task, TASK_STATE_BLOCKED_LOCK);
		schedule();
		scheduler_unlock();
	}
	// XXX: if we don't schedule after spin_unlock we might return without acquireing
	assert_panic(scheduler_lock_count == 1);
	spin_unlock(semaphore->lock);
	return;
}

void semaphore_release(semaphore_t *semaphore) {
	assert(semaphore != NULL);

	spin_lock(semaphore->lock);
	if (semaphore->blocked_tasks.first != NULL) {
		scheduler_lock();
		task_unblock_next(&semaphore->blocked_tasks);
		scheduler_unlock();
	} else {
		semaphore->count++;
	}
	spin_unlock(semaphore->lock);
	return;
}

/* other helpers */
void _sleep(uint64_t delta) {
	// TODO: FIXME: this shouldn't be used anywhere
	uint64_t target = delta + timer_ticks;
	while (target < timer_ticks) {
		__asm__ __volatile__("hlt");
	}
}

void task_add(task_t *task) {
	printf("%s(task: %p)\n", __func__, task);
	assert(task != NULL),
	assert(task->obj != NULL);
	task->state = TASK_STATE_READY;

	scheduler_lock();
	task_queue(&ready_queue, task);
	scheduler_unlock();
}

/*
TODO:
this has the following tasks:
 add to terminated tasks list
 awake parent / reaper (if blocked on waitpid)

parent / reaper / cleaner:
 free kstack, etc...
TODO: ktask reaper / cleaner / parent
*/
__attribute__((noreturn)) void task_exit(void) {
	printf("%s: task: %p\n", __func__, current_task);
	assert(scheduler_lock_count == 1); // XXX: this might even work ...
	task_t *task = current_task;
	assert(task != NULL);
	assert(task->state == TASK_STATE_RUNNING);
	current_task = NULL;

	printf("%s: queueing task %p on terminated_queue\n", __func__, task);
	task->state = TASK_STATE_TERMINATED;
	task_queue(&terminated_queue, task);

	scheduler_lock_count--;
	assert_panic(scheduler_lock_count == 0);
	postponed_schedule = false;
	__schedule();
	printf("%s: WTF AFTER __schedule()\n", __func__);
	assert_panic(0);

#ifndef __TINYC__
	__builtin_unreachable();
#endif
}

__attribute__((noreturn)) void tasking_enable(void) {
	printf("%s()\n", __func__);
	interrupts_disable();
	assert(scheduler_ready == false);
	scheduler_ready = true;
	assert(current_task == NULL);
	current_task = task_dequeue(&ready_queue);
	assert(current_task != NULL);
	assert(current_task->state == TASK_STATE_READY);
	current_task->state = TASK_STATE_RUNNING;
	__restore_task(current_task);
	assert(0);
}
