#include <kernel_task.h>
#include <task.h>

#include <string.h>
#include <pmm.h>
#include <vmm.h>
#include <heap.h>
#include <console.h>

__attribute__((noreturn)) static void ktask_enter(ktask_t *ktask) {
	__ktask_exit(ktask->func(ktask->name, ktask->extra));
}

void ktask_spawn(ktask_func *func, const char *name, void *extra) {
	printf("%s(func: 0x%x, name: '%s');\n", __func__, (uintptr_t)func, name);

	ktask_t *ktask = kcalloc(1, sizeof(ktask_t));
	assert(ktask != NULL);
	ktask->task.type = TASK_TYPE_KTASK;
	ktask->task.pdir = kernel_directory;
	ktask->task.obj = ktask;
	task_kstack_alloc(&ktask->task);

	ktask->name = strndup(name, strlen(name));
	assert(ktask->name != NULL);

	ktask->func = func;
	ktask->extra = extra;

	ktask->task.registers = NULL;
	uintptr_t esp = ktask->task.kstack;
	esp -= sizeof(uintptr_t);
	*((uintptr_t *)esp) = (uintptr_t)ktask;
	esp -= sizeof(uintptr_t);
	*((uintptr_t *)esp) = (uintptr_t)0;

	ktask->task.esp = (uint32_t)esp;
	ktask->task.eip = (uint32_t)&ktask_enter;
	ktask->task.ebp = 0;

	printf("%s: adding ktask %p (name: %p '%s')\n", __func__, ktask, ktask->name, ktask->name);
	task_add(&ktask->task);
}

// TODO: implement
void ktask_destroy(ktask_t *ktask) {
	(void)ktask;
	printf("%s: TODO: implement\n", __func__);
	assert(0);
}

// FIXME: name gets overwritten for some reason
__attribute__((noreturn)) void __ktask_exit(unsigned int status) {
	scheduler_lock();
	ktask_t *ktask = current_task->obj;
	assert(ktask != NULL);
	printf("%s(status: %u, name: %p '%s')\n", __func__, status, ktask->name, ktask->name);

	task_exit();
	assert(0);
	ktask_destroy(ktask);
}
