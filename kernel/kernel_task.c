#include <kernel_task.h>
#include <task.h>

#include <string.h>
#include <pmm.h>
#include <vmm.h>
#include <heap.h>
#include <console.h>

void __attribute__((noreturn)) __ktask_exit(uint32_t status);

void ktask_spawn(ktask_func func, const char *name, void *extra) {
	printf("%s(func: 0x%x, name: '%s');\n", __func__, (uintptr_t)func, name);

	ktask_t *ktask = kcalloc(1, sizeof(ktask_t));
	assert(ktask != NULL);
	ktask->task.type = TASK_TYPE_KTASK;
	ktask->task.pdir = kernel_directory;
	ktask->task.obj = &ktask;
	task_kstack_alloc(&ktask->task);

	// FIXME: we should probably allocate this
	ktask->name = name;

	// TODO: enable interrupts in kernel task's for preemption (we need to fix a lot of things before that)
	ktask->task.registers = NULL;
	uintptr_t esp = ktask->task.kstack;
	esp -= sizeof(uintptr_t);
	*((uintptr_t *)esp) = (uintptr_t)extra;
	esp -= sizeof(uintptr_t);
	*((uintptr_t *)esp) = (uintptr_t)name;
	esp -= sizeof(uintptr_t);
	*((uintptr_t *)esp) = (uintptr_t)func;

	ktask->task.esp = (uint32_t)esp;
	ktask->task.eip = (uint32_t)&__call_ktask;
	ktask->task.ebp = 0;

	task_add(&ktask->task);
}

void ktask_destroy(ktask_t *ktask) {
	(void)ktask;
	printf("%s: TODO: implement\n", __func__);
	assert(0);
}

// XXX: should only be called with interrupts off
void __attribute__((noreturn)) __ktask_exit(uint32_t status) {
	__asm__ __volatile__ ("cli");
	ktask_t *ktask = (ktask_t *)current_task->obj;
	assert(ktask != NULL);
	printf("%s(name: '%s' status: %u)\n", __func__, ktask->name, status);
	task_remove(&ktask->task);
	ktask_destroy(ktask);
	__switch_direct();
}
