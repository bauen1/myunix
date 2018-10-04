#include <console.h>
#include <gdt.h>
#include <list.h>
#include <pmm.h>
#include <process.h>
#include <string.h>
#include <task.h>
#include <vmm.h>

task_t *current_task;
node_t *current_task_node;
list_t *task_list;
process_t *current_process;
static uintptr_t delayed_kstack = 0;

void task_kstack_alloc(task_t *task) {
	assert(task != NULL);

	uintptr_t kstack = find_vspace(kernel_directory, KSTACK_SIZE + 2);
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

static void free_kstack(uintptr_t kstack) {
	assert(get_page(get_table_alloc(kstack, kernel_directory), kstack) == PAGE_VALUE_GUARD);
	map_page(get_table_alloc(kstack, kernel_directory), kstack, 0, 0);
	invalidate_page(kstack);
	kstack -= BLOCK_SIZE;

	for (size_t i = 0; i < (KSTACK_SIZE - 2); i++) {
		uintptr_t vkaddr = kstack - i * BLOCK_SIZE;
		uintptr_t kpage = get_page(get_table_alloc(vkaddr, kernel_directory), vkaddr);
		assert(kpage & PAGE_PRESENT);

		memset((void *)vkaddr, 0, BLOCK_SIZE);

		map_page(get_table_alloc(vkaddr, kernel_directory), vkaddr, 0, 0);
		invalidate_page(vkaddr);
		pmm_free_blocks(kpage & ~0xFFF, 1);
	}

	kstack -= (KSTACK_SIZE - 2) * BLOCK_SIZE;

	assert(get_page(get_table_alloc(kstack, kernel_directory), kstack) == PAGE_VALUE_GUARD);
	map_page(get_table_alloc(kstack, kernel_directory), kstack, 0, 0);
	invalidate_page(kstack);
}

// XXX: just don't call it when we're using the kstack
void task_kstack_free(task_t *task) {
	assert(task != NULL);
	uintptr_t kstack = task->kstack;
	assert(kstack != 0);
	free_kstack(kstack);

	task->kstack = 0;
}


/*
XXX: this works, by setting a special variable, which gets checked after the return to
another process, which will then call task_internal_deinit_kstack
*/
void task_kstack_delayed_free(task_t *task) {
	assert(delayed_kstack == 0);
	assert(task->kstack != 0);
	delayed_kstack = task->kstack;
	task->kstack = 0;
}

node_t *next_task_node() {
	assert(task_list != NULL);
	node_t *node = NULL;
	if (current_task_node == NULL) {
		node = task_list->head;
	} else if (current_task_node->next != NULL) {
		node = current_task_node->next;
	} else {
		node = task_list->head;
	}
	assert(node != NULL);
	return node;
}

// switch to some new task and return when we get scheduled again
// XXX: only call with interrupts off
// TODO: rewrite partially in assembly
// FIXME: a better name would be yield
// FIXME: assert that interrupts are off
void __switch_task() {
	if (current_task == NULL) {
		// FIXME: pit timer calls this before tasking is initialized
		return;
	}

	/* XXX: ensure interrupts are off, it's the caller's responsibility to re-enable them if needed */
	__asm__ __volatile__("cli");

	uint32_t esp, ebp, eip;
	__asm__ __volatile__("mov %%esp, %0" : "=r" (esp));
	__asm__ __volatile__("mov %%ebp, %0" : "=r" (ebp));
	eip = read_eip();
	if (eip == 0) {
		// XXX: check if we need to free another kstack
		if (delayed_kstack != 0) {
			free_kstack(delayed_kstack);
			delayed_kstack = 0;
		}
		return;
	}
	current_task->esp = esp;
	current_task->ebp = ebp;
	current_task->eip = eip;

	__switch_direct();
}

static void __attribute__((noreturn)) __restore_task();

// XXX: doesn't save any state and never returns
void __attribute__((noreturn)) __switch_direct(void) {
	current_task_node = next_task_node();
	assert(current_task_node != NULL);
	current_task = current_task_node->value;
	assert(current_task != NULL);
	if (current_task->type == TASK_TYPE_KTASK) {
		current_process = NULL;
	} else if (current_task->type == TASK_TYPE_USER_PROCESS) {
		current_process = (process_t *)current_task->obj;
	} else {
		assert(0);
	}
	__restore_task();
}

static void __attribute__((noreturn)) __restore_task() {
	tss_set_kstack(current_task->kstack);

	// TODO: rewrite partially in assembly
	uint32_t esp = current_task->esp;
	uint32_t ebp = current_task->ebp;
	uint32_t eip = current_task->eip;

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

/* XXX: only call with interrupts off */
void task_remove(task_t *task) {
	assert(task_list != NULL);
	assert(task != NULL);
	printf("%s: removing task %p from queue\n", __func__, task);
	if (current_task == task) {
		printf("%s: removing active task!\n", __func__);
		// FIXME: v
		assert(current_task_node != task_list->head);
		assert(task_list->head != NULL);
		assert(current_task != task_list->head->value);
		// TODO: call reschedule or something instead of this mess
		current_task_node = task_list->head;
		current_task = current_task_node->value;
	}
	list_remove(task_list, task);
}

void task_add(task_t *task) {
	// FIXME: check if interrupts are already disabled
	__asm__ __volatile__("cli");
	assert(task != NULL);
	assert(task->obj != NULL);

	if (task_list == NULL) {
		task_list = list_init();
	}
	list_insert(task_list, task);
	__asm__ __volatile__("sti");
	return;
}

void __attribute__((noreturn)) tasking_enable(void) {
	printf("%s()\n", __func__);
	__asm__ __volatile__ ("cli");
	current_task_node = task_list->head;
	assert(current_task_node != NULL);
	current_task = current_task_node->value;
	assert(current_task != NULL);
	__restore_task();
}
