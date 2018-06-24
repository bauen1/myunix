// TODO: better file descriptor implementation
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <console.h>
#include <fs.h>
#include <gdt.h>
#include <heap.h>
#include <idt.h>
#include <list.h>
#include <pmm.h>
#include <process.h>
#include <string.h>
#include <task.h>
#include <vmm.h>

// kernel stack size in pages (including guard page)
#define KSTACK_SIZE 4

static void __attribute__((noreturn)) __restore_process();

void __attribute__((noreturn)) _ktask_exit(uint32_t status);

process_t *current_process;
node_t *current_process_node;
list_t *process_list;

extern void *isrs_start;
extern void *isrs_end;

extern void *__start_user_shared;
extern void *__stop_user_shared;

// allocate and map a kernel stack into kernel space
// we could just call process_init_kstack it shouldn't make a difference (it would be a bit slower)
static void process_init_kernel_kstack(process_t *process) {
	assert(process != NULL);

	// kstack
	process->kstack_size = KSTACK_SIZE;
	uintptr_t v_kstack = find_vspace(kernel_directory, process->kstack_size + 4);
	assert(v_kstack != 0);

	// kstack guard
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	printf(" kstack 0x%8x => GUARD\n", v_kstack);
	v_kstack += BLOCK_SIZE;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	printf(" kstack 0x%8x => GUARD\n", v_kstack);
	v_kstack += BLOCK_SIZE;

	process->kstack = v_kstack;
	for (size_t i = 0; i < process->kstack_size; i++) { // skip guard
		uintptr_t v_kaddr = v_kstack + i * BLOCK_SIZE;
		uintptr_t block = pmm_alloc_blocks_safe(1);
		printf(" kstack 0x%8x => 0x%8x\n", v_kaddr, block);
		assert((get_page(get_table(v_kaddr, kernel_directory), v_kaddr) & PAGE_TABLE_PRESENT) == 0);
		map_page(get_table(v_kaddr, kernel_directory), v_kaddr, block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
		__asm__ __volatile__ ("invlpg (%0)" : : "b" (v_kaddr) : "memory");
	}

	uintptr_t kstack_length = process->kstack_size * BLOCK_SIZE;
	process->kstack_top = process->kstack + kstack_length;

	v_kstack += kstack_length;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	printf(" kstack 0x%8x => GUARD\n", v_kstack);
	v_kstack += BLOCK_SIZE;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	printf(" kstack 0x%8x => GUARD\n", v_kstack);

	printf("kstack_length: 0x%x\n", kstack_length);
	memset((void *)process->kstack, 0, kstack_length);
	printf("kstack: 0x%8x - 0x%8x\n", process->kstack, process->kstack_top);
}


void create_ktask(ktask_func func, char *name, void *extra) {
	printf("create_ktask(func: 0x%x, name: '%s');\n", (uintptr_t)func, name);

	process_t *process = kcalloc(1, sizeof(process_t));
	assert(process != NULL);
	process->is_kernel_task = true;
	process->pdir = kernel_directory;
	process->fd_table = NULL;

	process_init_kernel_kstack(process);

	process->name = kmalloc(strlen(name) + 1);
	assert(process->name != NULL);
	strncpy(process->name, name, 255);

	// TODO: enable interrupts in kernel task's for preemption (we need to fix a lot of things before that)
	process->regs = NULL;
	uintptr_t esp = process->kstack_top;
	esp -= sizeof(uintptr_t);
	*((uintptr_t *)esp) = (uintptr_t)name;
	esp -= sizeof(uintptr_t);
	*((uintptr_t *)esp) = (uintptr_t)extra;
	esp -= sizeof(uintptr_t);
	*((uintptr_t *)esp) = (uintptr_t)func;

	process->esp = (uint32_t)esp;
	process->eip = (uintptr_t)&__call_ktask;
	process->ebp = 0;

	process_add(process);
}

// should only be called with interrupts off
void __attribute__((noreturn)) _ktask_exit(uint32_t status) {
	__asm__ __volatile__ ("cli");
	printf("ktask_exit(name: '%s', status: %u);\n", current_process->name, status);
	process_t *p = current_process;
	process_remove(p);
	printf(" kstack: 0x%x kstack_size: %u\n", p->kstack, (uintptr_t)p->kstack_size);
	// FIXME: free kstack
	kfree(p);
	printf(" switch direct!\n");
	__switch_direct();
}

static void process_map_shared_region(process_t *process, uintptr_t start, uintptr_t end, unsigned int permissions) {
//	printf("process_map_shared_region(process: 0x%x, start: 0x%x, end: 0x%x, permissions: %u)\n", (uintptr_t)process, start, end, permissions);
	if ((start & 0xFFF) != 0) {
		printf(" WARN: start 0x%8x not aligned!\n", start);
	}
	if ((end & 0xFFF) != 0) {
		printf(" WARN: end 0x%8x not aligned!\n", end);
	}

	for (uintptr_t i = 0; i < (end - start); i += BLOCK_SIZE) {
		uintptr_t addr = start + i;
		map_page(get_table_alloc(addr, process->pdir), addr, addr, permissions);
	}
}

// TODO: ensure no additional information gets leaked (align and FILL a block)
// TODO: put everything that needs to be mapped in a special segment
static void process_map_shared(process_t *process) {
	// isr trampolines
	process_map_shared_region(process, (uintptr_t)&isrs_start,
		(uintptr_t)&isrs_end, PAGE_TABLE_PRESENT);
	// other stuff we really need ( gdt, idt, tss)
	process_map_shared_region(process, (uintptr_t)&__start_user_shared,
		(uintptr_t)&__stop_user_shared, PAGE_TABLE_PRESENT);
	// kernel_directory pointer
	// FIXME: don't map this, patch it in the isr routine or give the irs routine its own pointer
	// FIXME: find a better way to do this
	process_map_shared_region(process, (uintptr_t)&kernel_directory,
		(uintptr_t)&kernel_directory + BLOCK_SIZE, PAGE_TABLE_PRESENT);

	map_page(get_table_alloc((uintptr_t)&kernel_directory, process->pdir),
		(uintptr_t)&kernel_directory,
		(uintptr_t)&kernel_directory,
		PAGE_TABLE_PRESENT);
}

// allocate and map a kernel stack into kernel space and userspace
static void process_init_kstack(process_t *process) {
	assert(process != NULL);

	// kstack
	process->kstack_size = KSTACK_SIZE;
	uintptr_t v_kstack = find_vspace(kernel_directory, process->kstack_size + 4);
	assert(v_kstack != 0);

	// kstack guard
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	map_page(get_table_alloc(v_kstack, process->pdir), v_kstack, PAGE_VALUE_GUARD, 0);
	printf(" kstack 0x%8x => GUARD\n", v_kstack);
	v_kstack += BLOCK_SIZE;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	map_page(get_table_alloc(v_kstack, process->pdir), v_kstack, PAGE_VALUE_GUARD, 0);
	printf(" kstack 0x%8x => GUARD\n", v_kstack);
	v_kstack += BLOCK_SIZE;

	process->kstack = v_kstack;
	for (size_t i = 0; i < process->kstack_size; i++) { // skip guard
		uintptr_t v_kaddr = v_kstack + i * BLOCK_SIZE;
		uintptr_t block = pmm_alloc_blocks_safe(1);
		printf(" kstack 0x%8x => 0x%8x\n", v_kaddr, block);
		assert((get_page(get_table(v_kaddr, kernel_directory), v_kaddr) & PAGE_TABLE_PRESENT) == 0);
		map_page(get_table(v_kaddr, kernel_directory), v_kaddr, block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
		map_page(get_table_alloc(v_kaddr, process->pdir), v_kaddr, block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
		__asm__ __volatile__ ("invlpg (%0)" : : "b" (v_kaddr) : "memory");
	}

	uintptr_t kstack_length = process->kstack_size * BLOCK_SIZE;
	process->kstack_top = process->kstack + kstack_length;

	v_kstack += kstack_length;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	map_page(get_table_alloc(v_kstack, process->pdir), v_kstack, PAGE_VALUE_GUARD, 0);
	printf(" kstack 0x%8x => GUARD\n", v_kstack);
	v_kstack += BLOCK_SIZE;
	map_page(get_table(v_kstack, kernel_directory), v_kstack, PAGE_VALUE_GUARD, 0);
	map_page(get_table_alloc(v_kstack, process->pdir), v_kstack, PAGE_VALUE_GUARD, 0);
	printf(" kstack 0x%8x => GUARD\n", v_kstack);

	printf("kstack_length: 0x%x\n", kstack_length);
	memset((void *)process->kstack, 0, kstack_length);
	printf("kstack: 0x%8x - 0x%8x\n", process->kstack, process->kstack_top);
}

process_t *process_exec(fs_node_t *f) {
	uintptr_t virt_text_start = 0x1000000;
	uintptr_t virt_heap_start = 0x2000000; // max 16mb text
	printf("process_exec(0x%x (f->name: '%s'));\n", (uintptr_t)f, f->name);
	assert(f != NULL);
	assert(f->length != 0);
	process_t *process = kcalloc(1, sizeof(process_t));
	assert(process != NULL);
	printf(" process: 0x%x\n", (uintptr_t)process);
	process->is_kernel_task = false;
	process->fd_table = kcalloc(1, sizeof(fd_table_t));
	assert(process->fd_table != NULL);
	process->fd_table->entries[0] = &tty_node;
	process->fd_table->entries[1] = &tty_node;
	process->fd_table->entries[2] = &tty_node;

	// FIXME: use dma_malloc ?
	uintptr_t real_pdir = pmm_alloc_blocks_safe(1);
	process->pdir = (page_directory_t *)find_vspace(kernel_directory, 1);
	assert(process->pdir != 0);
	printf("process->pdir: 0x%x\n", (uintptr_t)process->pdir);
	map_page(get_table_alloc((uintptr_t)process->pdir, kernel_directory), (uintptr_t)process->pdir,
		real_pdir,
		PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
	memset((void *)process->pdir, 0, BLOCK_SIZE);

	process_map_shared(process);
	process_init_kstack(process);

	// allocate some userspace heap (16kb)
	// FIXME: size hardcoded
	uintptr_t k_tmp = find_vspace(kernel_directory, 1);
	assert(k_tmp != 0);
	for (unsigned int i = 0; i < 256; i++) {
		// allocate non-continous space
		uintptr_t virtaddr = virt_heap_start + i*BLOCK_SIZE;
		uintptr_t block = (uintptr_t)pmm_alloc_blocks_safe(1);
//		printf(" 0x%8x => 0x%8x PRESENT|WRITE|USER\n", virtaddr, block);
		map_page(get_table(k_tmp, kernel_directory), k_tmp, block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
		__asm__ __volatile__ ("invlpg (%0)" : : "b" (k_tmp) : "memory");
		memset((void *)k_tmp, 0, BLOCK_SIZE);
		map_page(get_table(k_tmp, kernel_directory), k_tmp, 0, 0);
		map_page(get_table_alloc(virtaddr, process->pdir), virtaddr,
			block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE | PAGE_TABLE_USER);
	}

	// map the .text section
	for (uintptr_t i = 0; i < f->length; i+=BLOCK_SIZE) {
		uintptr_t block = pmm_alloc_blocks_safe(1);
		uintptr_t u_vaddr = virt_text_start + i;
		map_page(get_table(k_tmp, kernel_directory),
			k_tmp,
			block, PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
		__asm__ __volatile__ ("invlpg (%0)" : : "b" (k_tmp) : "memory");
		uint32_t j = fs_read(f, i, BLOCK_SIZE, (uint8_t *)k_tmp);
		assert(j != (unsigned)-1);
//		printf("user: 0x%x => 0x%x (puw) 0x%x\n", u_vaddr, block, j);
		map_page(get_table_alloc(u_vaddr, process->pdir),
			u_vaddr,
			block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_USER | PAGE_TABLE_READWRITE);
	}
	map_page(get_table(k_tmp, kernel_directory), k_tmp, 0, 0);

	registers_t *regs = (registers_t *)(process->kstack_top - sizeof(registers_t));
	regs->old_directory = (uint32_t)real_pdir;
	regs->gs = 0x23;
	regs->fs = 0x23;
	regs->es = 0x23;
	regs->ds = 0x23;
	regs->edi = 0;
	regs->esi = 0;
	regs->ebp = 0;
	regs->esp = 0;
	regs->ebx = 0;
	regs->ecx = 0;
	regs->eax = 0;
	regs->isr_num = 0;
	regs->err_code = 0;
	regs->eip = virt_text_start;
	regs->cs = 0x1B;
	regs->eflags = 0x200; // enable interrupts
	regs->esp = virt_heap_start + BLOCK_SIZE;
	regs->ss = regs->ds;

	process->regs = regs;

	process->esp = (uint32_t)regs;
	process->ebp = process->esp;
	process->eip = (uintptr_t)return_to_regs;
	return process;
}

process_t *process_init(uintptr_t start, uintptr_t end) {
	printf("process_init(0x%x, 0x%x)\n", start, end);
	assert(start % 0x100 == 0);

	uintptr_t virt_text_start = 0x1000000;
	uintptr_t virt_heap_start = 0x2000000; // max 16mb text

	process_t *process = kcalloc(1, sizeof(process_t));
	assert(process != NULL);
	printf(" process: 0x%x\n", (uintptr_t)process);
	process->is_kernel_task = false;
	process->fd_table = kcalloc(1, sizeof(fd_table_t));
	assert(process->fd_table != NULL);
	process->fd_table->entries[0] = &tty_node;
	process->fd_table->entries[1] = &tty_node;
	process->fd_table->entries[2] = &tty_node;

	// page directory
	uintptr_t real_pdir = pmm_alloc_blocks_safe(1);
	process->pdir = (page_directory_t *)find_vspace(kernel_directory, 1);
	assert(process->pdir != 0);
	printf(" process->pdir: 0x%x\n", (uintptr_t)process->pdir);
	map_page(get_table_alloc((uintptr_t)process->pdir, kernel_directory), (uintptr_t)process->pdir,
		real_pdir,
		PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
	memset((void *)process->pdir, 0, BLOCK_SIZE);

	process_map_shared(process);
	process_init_kstack(process);

	// allocate some userspace heap (16kb)
	// FIXME: size hardcoded
	uintptr_t k_tmp = find_vspace(kernel_directory, 1);
	assert(k_tmp != 0);
	for (unsigned int i = 0; i < 256; i++) {
		// allocate non-continous space
		uintptr_t virtaddr = virt_heap_start + i*BLOCK_SIZE;
		uintptr_t block = (uintptr_t)pmm_alloc_blocks_safe(1);
		map_page(get_table(k_tmp, kernel_directory), k_tmp, block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
		memset((void *)k_tmp, 0, BLOCK_SIZE);
		map_page(get_table(k_tmp, kernel_directory), k_tmp, 0, 0);
		map_page(get_table_alloc(virtaddr, process->pdir), virtaddr,
			block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE | PAGE_TABLE_USER);
	}

	// map the .text section
	printf(" .text: 0x%x - 0x%x => 0x%x - 0x%x\n",
		virt_text_start, virt_text_start + (end - start),
		start, end);
	for (uintptr_t i = 0; i < (end - start); i += BLOCK_SIZE) {
		uintptr_t physaddr = start + i;
		uintptr_t virtaddr = virt_text_start + i;
		map_page(get_table_alloc(virtaddr, process->pdir),
			virtaddr,
			physaddr,
			PAGE_TABLE_PRESENT | PAGE_TABLE_USER | PAGE_TABLE_READWRITE);
	}

	registers_t *regs = (registers_t *)(process->kstack - sizeof(registers_t));
	regs->old_directory = (uint32_t)real_pdir;
	regs->gs = 0x23;
	regs->fs = 0x23;
	regs->es = 0x23;
	regs->ds = 0x23;
	regs->edi = 0;
	regs->esi = 0;
	regs->ebp = 0;
	regs->esp = 0;
	regs->ebx = 0;
	regs->ecx = 0;
	regs->eax = 0;
	regs->isr_num = 0;
	regs->err_code = 0;
	regs->eip = virt_text_start;
	regs->cs = 0x1B;
	regs->eflags = 0x200; // enable interrupts
	regs->esp = virt_heap_start + BLOCK_SIZE;
	regs->ss = regs->ds;

	process->regs = regs;

	process->esp = (uint32_t)regs;
	process->ebp = process->esp;
	process->eip = (uintptr_t)return_to_regs;
	return process;
}

process_t *next_process() {
	assert(process_list != NULL);
	node_t *n = current_process_node->next;
	if (n == NULL) {
		n = process_list->head;
	}
	current_process_node = n;
	current_process = current_process_node->value;
	return current_process;
}

extern uintptr_t read_eip();

// switch to some new task and return when we get scheduled again
// TODO: rewrite this completely in asm
void __attribute__((used)) _switch_task() {
	if (current_process == NULL) {
		return;
	}

	/* ensure interrupts are off, it's the caller's responsibility to re-enable them if needed */
	__asm__ __volatile__("cli");

	uint32_t esp, ebp, eip;
	__asm__ __volatile__("mov %%esp, %0" : "=r" (esp));
	__asm__ __volatile__("mov %%ebp, %0" : "=r" (ebp));
	eip = read_eip();
	if (eip == 0) {
		return;
	}
	current_process->esp = esp;
	current_process->ebp = ebp;
	current_process->eip = eip;

	__switch_direct();
}

// DESTRUCTIVE: don't save anything and NEVER return
void __attribute__((noreturn)) __switch_direct(void) {
	current_process = next_process();
	__restore_process();
}

static void __attribute__((noreturn)) __restore_process() {
//	tss_set_kstack(current_process->kstack);
	if (current_process->is_kernel_task) {
		tss_set_kstack(0);
	} else {
		tss_set_kstack(current_process->kstack_top);
	}

	uint32_t esp = current_process->esp;
	uint32_t ebp = current_process->ebp;
	uint32_t eip = current_process->eip;

	__asm__ __volatile__ (
		"mov %0, %%ebx\n"
		"mov %1, %%esp\n"
		"mov %2, %%ebp\n"
		// "mov %3, %%cr3\n"
		"mov $0, %%eax\n"
		"jmp *%%ebx"
		:
		: "r" (eip), "r" (esp), "r" (ebp) /*, "r"(current_dir)*/
		: "ebx", "esp", "eax");
#ifndef __TINYC__
	__builtin_unreachable();
#endif
}

/* only call with interrupts off */
void process_remove(process_t *p) {
	assert(process_list != NULL);
	assert(p != NULL);
	if (current_process == p) {
		current_process_node = process_list->head;
		assert(current_process_node != NULL);
		current_process = current_process_node->value;
		assert(current_process != NULL);
	}
	list_remove(process_list, p);
}

void process_add(process_t *p) {
	__asm__ __volatile__ ("cli");
	assert(p != NULL);
	if (process_list == NULL) {
		process_list = list_init();
//		printf("process_list: 0x%x\n", (uintptr_t)process_list);
	}
	list_insert(process_list, p);
	__asm__ __volatile__ ("sti");
	return;
}

void __attribute__((noreturn)) process_enable(void) {
	printf("process_enable\n");
	__asm__ __volatile__ ("cli");
	current_process_node = process_list->head;
	current_process = current_process_node->value;
	assert(current_process != NULL);
/*
	printf("current_process: 0x%x\n", (uintptr_t)current_process);
	printf("current_process->kstack: 0x%x\n", current_process->kstack);
	printf("current_process->kstack_size: 0x%x\n", current_process->kstack_size);
	printf("current_process->regs: 0x%x\n", (uintptr_t)current_process->regs);
	printf("current_process->esp: 0x%x\n", current_process->esp);
	printf("current_process->ebp: 0x%x\n", current_process->ebp);
	printf("current_process->eip: 0x%x\n", current_process->eip);
	printf("current_process->pdir: 0x%x\n", (uintptr_t)current_process->pdir);
	printf("current_process->name: '%s'\n", current_process->name);
	printf("current_process->pid: 0x%x\n", current_process->pid);
	printf("current_process->fd_table: 0x%x\n", (uintptr_t)current_process->fd_table);
*/
	__restore_process();
}
