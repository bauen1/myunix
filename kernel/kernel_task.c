#include <kernel_task.h>
#include <task.h>

#include <string.h>
#include <pmm.h>
#include <vmm.h>
#include <heap.h>
#include <console.h>

void __attribute__((noreturn)) __ktask_exit(uint32_t status);

// XXX: allocate and map a kernel stack into kernel space
// XXX: we could just call process_init_kstack it shouldn't make a difference (it would be a bit slower)
static void ktask_init_kstack(ktask_t *ktask) {
	assert(ktask != NULL);

	// kstack
	ktask->kstack_size = KSTACK_SIZE;
	uintptr_t v_kstack = find_vspace(kernel_directory, ktask->kstack_size + 4);
	assert(v_kstack != 0);

	// kstack guard
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	v_kstack += BLOCK_SIZE;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	v_kstack += BLOCK_SIZE;

	ktask->kstack = v_kstack;
	for (size_t i = 0; i < ktask->kstack_size; i++) { // skip guard
		uintptr_t v_kaddr = v_kstack + i * BLOCK_SIZE;
		uintptr_t block = pmm_alloc_blocks_safe(1);
		assert((get_page(get_table(v_kaddr, kernel_directory), v_kaddr) & PAGE_PRESENT) == 0);
		map_page(get_table(v_kaddr, kernel_directory), v_kaddr, block,
			PAGE_PRESENT | PAGE_READWRITE);
		invalidate_page(v_kaddr);
	}

	uintptr_t kstack_length = ktask->kstack_size * BLOCK_SIZE;
	ktask->kstack_top = ktask->kstack + kstack_length;

	v_kstack += kstack_length;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	v_kstack += BLOCK_SIZE;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);

	memset((void *)ktask->kstack, 0, kstack_length);
}

// TODO: de-initialize (free) kernel stack of a kernel task
void ktask_deinit_kstack(ktask_t *ktask) {
	assert(ktask != NULL);
	printf("%s: TODO: implement!\n", __func__);
}

void ktask_spawn(ktask_func func, const char *name, void *extra) {
	printf("%s(func: 0x%x, name: '%s');\n", __func__, (uintptr_t)func, name);

	ktask_t *ktask = kcalloc(1, sizeof(ktask_t));
	assert(ktask != NULL);
	ktask->task.type = TASK_TYPE_KTASK;
	ktask->task.pdir = kernel_directory;
	ktask->task.obj = &ktask;

	ktask_init_kstack(ktask);

	// FIXME: we should probably allocate this
	ktask->name = name;

	// TODO: enable interrupts in kernel task's for preemption (we need to fix a lot of things before that)
	ktask->task.registers = NULL;
	uintptr_t esp = ktask->kstack_top;
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
