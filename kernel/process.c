// TODO: LOCKS EVERYWHERE!
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <boot.h>
#include <bitmap.h>
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
#include <tree.h>
#include <vmm.h>
#include <syscall.h>

bitmap_t *pid_bitmap;

static pid_t last_pid = 1;
pid_t get_pid(void) {
	if (last_pid > PROCESS_MAX_PID) {
		assert(0);
	}
	last_pid++;
	pid_t pid = last_pid;
	assert(bitmap_test(pid_bitmap, pid) == false);
	bitmap_set(pid_bitmap, pid);
	return pid;
}

tree_t *ptree;
tree_node_t *ptree_get_init_node(void) {
	return ptree->root;
}

tree_node_t *ptree_new_node(tree_node_t *parent, process_t *process) {
	tree_node_t *v = tree_node_new();
	assert(v != NULL);
	v->value = process;
	tree_node_insert_child(ptree, ((process_t *)parent->value)->ptree_node, v);
	return v;
}

/* fd_entry_t helpers */
fd_entry_t *fd_reference(fd_entry_t *fd) {
	assert(fd != NULL);
	if (fd->__refcount != -1) {
		fd->__refcount++;
	}
	return fd;
}

fd_entry_t *fd_new(void) {
	fd_entry_t *fd = kcalloc(1, sizeof(fd_entry_t));
	if (fd == NULL) {
		return NULL;
	}
	fd->seek = 0;
	return fd_reference(fd);
}

void fd_free(fd_entry_t *fd) {
	assert(fd != NULL);
	if (fd->__refcount == -1) {
		return;
	}

	fd->__refcount--;
	if (fd->__refcount == 0) {
		if (fd->node != NULL) {
			fs_close(&(fd->node));
		}
		kfree(fd);
	}
}

/* fd_table_t helpers */
static void fd_table_realloc(fd_table_t *table) {
	assert(table != NULL);
	if (table->entries == NULL) {
		table->entries = kcalloc(table->capacity, sizeof(fd_entry_t *));
	} else {
		fd_entry_t **new_entries = kcalloc(table->capacity, sizeof(fd_entry_t *));
		memcpy(new_entries, table->entries, table->length * sizeof(fd_entry_t *));
		kfree(table->entries);
		table->entries = new_entries;
	}
	assert(table->entries != NULL);
}

fd_table_t *fd_table_reference(fd_table_t *fd_table) {
	assert(fd_table != NULL);
	if (fd_table->__refcount != -1) {
		fd_table->__refcount++;
	}
	return fd_table;
}

fd_table_t *fd_table_new() {
	fd_table_t *fd_table = kcalloc(1, sizeof(fd_table_t));
	assert(fd_table != NULL);
	fd_table->capacity = 16;
	fd_table->length = 0;
	fd_table_realloc(fd_table);
	return fd_table_reference(fd_table);
}

fd_table_t *fd_table_clone(fd_table_t *oldfdt) {
	assert(oldfdt != NULL);
	fd_table_t *newfdt = fd_table_new();
	assert(newfdt != NULL);
	for (unsigned int i = 0; i < oldfdt->length; i++) {
		fd_entry_t *oldfd = fd_table_get(oldfdt, i);
		if (oldfd != NULL) {
			// FIXME: not 100% posix compliant
			fd_table_set(newfdt, i, fd_reference(oldfd));
		} else {
			fd_table_set(newfdt, i, NULL);
		}
	}
	return newfdt;
}

void fd_table_free(fd_table_t *fd_table) {
	assert(fd_table != NULL);
	assert(fd_table->__refcount != -1);

	if (fd_table->__refcount == -1) {
		return;
	}

	fd_table->__refcount--;
	if (fd_table->__refcount == 0) {
		for (unsigned int i = 0; i < fd_table->length; i++) {
			if (fd_table->entries[i] != NULL) {
				fd_free(fd_table->entries[i]);
			}
		}
		kfree(fd_table);
	}
}

int fd_table_set(fd_table_t *fd_table, unsigned int i, fd_entry_t *entry) {
	assert(fd_table != NULL);

	if (entry == NULL) {
		if (i < fd_table->capacity) {
			if (fd_table->entries[i] != NULL) {
				fd_free(fd_table->entries[i]);
				fd_table->entries[i] = NULL;
			}
		}
	} else {
		if (i < fd_table->capacity) {
			if (fd_table->entries[i] != NULL) {
				fd_free(fd_table->entries[i]);
			}
		} else {
			fd_table->capacity = i;
			fd_table_realloc(fd_table);
		}
		fd_table->entries[i] = entry;
		if (fd_table->length < i) {
			fd_table->length = i;
		}
	}
	return i;
}

int fd_table_append(fd_table_t *fd_table, fd_entry_t *entry) {
	assert(fd_table != NULL);
	assert(entry != NULL);

	for (unsigned int i = 0; i < fd_table->length; i++) {
		if (fd_table->entries[i] == NULL) {
			fd_table->entries[i] = entry;
			return i;
		}
	}

	// no gaps found, is the table full ?
	if (fd_table->length == fd_table->capacity) {
		fd_table->capacity = fd_table->capacity + 8;
		fd_table_realloc(fd_table);
	}
	// if not then entries[fd_table->length] is free

	fd_table->entries[fd_table->length] = entry;
	fd_table->length++;
	return fd_table->length - 1;
}

fd_entry_t *fd_table_get(fd_table_t *fd_table, unsigned int i) {
	assert(fd_table != NULL);

	if (i > fd_table->length) {
		return NULL;
	}
	return fd_table->entries[i];
}

/* process_t helpers */
static void process_map_shared_region(page_directory_t *pdir, uintptr_t start, uintptr_t end, unsigned int permissions) {
	assert(pdir != NULL);
	assert((start & 0xFFF) == 0);

	for (uintptr_t i = 0; i < (end - start); i += BLOCK_SIZE) {
		uintptr_t addr = start + i;
		map_page(get_table_alloc(addr, pdir), addr, addr, permissions);
	}
}

static void process_unmap_shared_region(page_directory_t *pdir, uintptr_t start, uintptr_t end) {
	assert(pdir != NULL);
	assert((start & 0xFFF) == 0);

	for (uintptr_t i = 0; i < (end - start); i += BLOCK_SIZE) {
		uintptr_t addr = start + i;
		uintptr_t p = get_page(get_table_alloc(addr, pdir), addr);
		assert(addr == (p & ~0xFFF));
		map_page(get_table_alloc(addr, pdir), addr, 0, 0);
	}
}

// allocate and map a kernel stack into kernel space and userspace
static void process_map_kstack(process_t *process) {
	assert(process != NULL);
	assert(process->task.kstack != 0);

	// FIXME: we might not need to map more than one page, since we switch to the kernel directory as soon as possible
	uintptr_t kstack = process->task.kstack;
	assert(kstack != 0);

	for (uintptr_t i = 0; i < KSTACK_SIZE; i++) {
		uintptr_t vkaddr = kstack - i * BLOCK_SIZE;
		page_t page = get_page(get_table(vkaddr, kernel_directory), vkaddr);
		assert(! (page & PAGE_USER));
		assert(get_page(get_table_alloc(vkaddr, process->task.pdir), vkaddr) == 0);
		set_page(get_table_alloc(vkaddr, process->task.pdir), vkaddr, page);
	}
}

static void process_unmap_kstack(process_t *process) {
	assert(process != NULL);

	uintptr_t kstack = process->task.kstack;
	assert(kstack != 0);

	for (uintptr_t i = 0; i < KSTACK_SIZE; i++) {
		uintptr_t vkaddr = kstack - i * BLOCK_SIZE;
		page_t kpage = get_page(get_table(vkaddr, kernel_directory), vkaddr);
		page_t upage = get_page(get_table_alloc(vkaddr, process->task.pdir), vkaddr);
		// FIXME: doesn't cover all cases
		assert((kpage & ~0xFFF) == (upage & ~0xFFF));
		map_page(get_table_alloc(vkaddr, process->task.pdir), vkaddr, 0, 0);
	}
}

static void process_page_directory_map_shared(page_directory_t *page_directory) {
	// ISR stubs
	process_map_shared_region(page_directory, (uintptr_t)&isrs_start,
		(uintptr_t)&isrs_end, PAGE_PRESENT);
	// other stuff (gdt, idt, tss)
	// TODO: make a user_shared_readonly and user_shared_readwrite section
	process_map_shared_region(page_directory, (uintptr_t)&__start_user_shared,
		(uintptr_t)&__stop_user_shared, PAGE_PRESENT | PAGE_READWRITE);
}

// TODO: ensure no additional information gets leaked (align and FILL a block) (maybe by stuffing everything in a special segment)
// FIXME: shouldn't the isrs also be in the user_shared section ?
page_directory_t *process_page_directory_new() {
	page_directory_t *page_directory = page_directory_new();
	assert(page_directory != NULL);

	process_page_directory_map_shared(page_directory);

	return page_directory;
}

static void process_page_directory_free(page_directory_t *pdir) {
	if (pdir->__refcount != 1) {
		// XXX: page_directory_free only decrements __refcount
		return page_directory_release(pdir);
	}

	// XXX: this is the last reference, clean up
	process_unmap_shared_region(pdir, (uintptr_t)&isrs_start, (uintptr_t)&isrs_end);
	process_unmap_shared_region(pdir, (uintptr_t)&__start_user_shared, (uintptr_t)&__stop_user_shared);

	// XXX: and free all allocated blocks
	for (uintptr_t i = 0; i < 1024; i++) {
		uintptr_t phys_table = pdir->physical_tables[i];
		if (phys_table & PAGE_PRESENT) {
			page_table_t *table = pdir->tables[i];
			assert(table != NULL);

			for (uintptr_t j = 0; j < 1024; j++) {
				page_t page = table->pages[j];
				uintptr_t virtaddr = (i << 22) | (j << 12);
				if (page == 0) {
					continue;
				} else if (page & (PAGE_PRESENT | PAGE_USER)) {
					// XXX: free page, depends on this being allocated to only this page directory
					uintptr_t phys = page & ~0xFFF;
					pmm_free_blocks(phys, 1);
					table->pages[j] = 0;
				} else {
					/* XXX: this is bad, all kernel pages should have been unmapped already, we don't know how to handle it */
					printf("%8x => 0x%8x this should not be here!\n", virtaddr, page);
					dump_directory(pdir);
					assert(0);
				}
			}
		} else {
			assert(pdir->tables[i] == NULL);
		}
	}
}

// process_execve helper
static uintptr_t *process_exec_copy_array(process_t *process, uintptr_t *region_ptr, uintptr_t region_end, char * const * const array, size_t array_length) {
	uintptr_t *ptrs = kcalloc(array_length, sizeof(uintptr_t));
	assert(ptrs != NULL);

	if (array == NULL) {
		assert(array_length == 0);
	} else {
		assert(array[array_length] == NULL);
		for (size_t i = 0; i < array_length; i++) {
			printf("%s: trying to copy array[%u] %p '%s' to %p\n", __func__, (unsigned int)i, array[i], array[i], *region_ptr);
			const char *v = array[i];
			assert(v != NULL);
			const size_t v_len = strlen(v) + 1;
			if (*region_ptr + v_len >= region_end) {
				// TODO: grow the region
				assert(0);
			}
			int32_t r = copy_to_userspace(process->task.pdir, *region_ptr, v_len, v);
			assert(r >= 0);
			ptrs[i] = *region_ptr;
			*region_ptr += v_len;
		}
	}

	return ptrs;
}

void process_execve(process_t *process, fs_node_t *f, size_t argc, char * const * const argv, size_t envc, char * const * const envp) {
	const uintptr_t virt_text_start = 0x1000000;
	const uintptr_t virt_heap_start = 0x2000000; // max 16mb text
	const uintptr_t virt_stack_start = 0x8000000;
	const uintptr_t virt_misc_start = 0x10000000;

	assert(process != NULL);
	assert(f != NULL);
	assert(f->length != 0);

	if (process->task.pdir != NULL) {
		process_unmap_kstack(process);
		process_page_directory_free(process->task.pdir);
	}

	process->task.type = TASK_TYPE_USER_PROCESS;
	process->task.pdir = process_page_directory_new();
	assert(process->task.pdir != NULL);
	if (process->task.kstack == 0) {
		task_kstack_alloc(&process->task);
	}
	process_map_kstack(process);

	// allocate some userspace heap (16kb)
	// FIXME: size hardcoded (and to be honest it's mostly useless)
	uintptr_t k_tmp = find_vspace(kernel_directory, 1);
	assert(k_tmp != 0);

	for (unsigned int i = 0; i < 256; i++) {
		// allocate non-continous space
		uintptr_t virtaddr = virt_heap_start + i*BLOCK_SIZE;
		uintptr_t block = (uintptr_t)pmm_alloc_blocks_safe(1);
		map_page(get_table(k_tmp, kernel_directory), k_tmp, block,
			PAGE_PRESENT | PAGE_READWRITE);
		invalidate_page(k_tmp);

		memset((void *)k_tmp, 0, BLOCK_SIZE);
		map_page(get_table_alloc(virtaddr, process->task.pdir), virtaddr,
			block,
			PAGE_PRESENT | PAGE_READWRITE | PAGE_USER);
	}

	// map the .text section
	// TODO: check for f->length overflow
	for (uintptr_t i = 0; i < f->length; i+=BLOCK_SIZE) {
		uintptr_t block = pmm_alloc_blocks_safe(1);
		uintptr_t virtaddr = virt_text_start + i;
		map_page(get_table(k_tmp, kernel_directory),
			k_tmp,
			block, PAGE_PRESENT | PAGE_READWRITE);
		invalidate_page(k_tmp);

		uint32_t j = fs_read(f, i, BLOCK_SIZE, (void *)k_tmp);
		assert(j != (uint32_t)-1);
		map_page(get_table_alloc(virtaddr, process->task.pdir),
			virtaddr,
			block,
			PAGE_PRESENT | PAGE_USER | PAGE_READWRITE);
	}

	// allocate stack
	for (unsigned int i = 0; i < 256; i++) {
		// allocate non-continous space
		uintptr_t virtaddr = virt_stack_start + i*BLOCK_SIZE;
		uintptr_t block = (uintptr_t)pmm_alloc_blocks_safe(1);
		map_page(get_table(k_tmp, kernel_directory), k_tmp, block,
			PAGE_PRESENT | PAGE_READWRITE);
		invalidate_page(k_tmp);

		memset((void *)k_tmp, 0, BLOCK_SIZE);
		map_page(get_table_alloc(virtaddr, process->task.pdir), virtaddr,
			block,
			PAGE_PRESENT | PAGE_READWRITE | PAGE_USER);
	}

	// allocate misc region (cmdline, *argv, environment)
	for (unsigned int i = 0; i < 1; i++) {
		// allocate non-continous space
		uintptr_t virtaddr = virt_misc_start + i*BLOCK_SIZE;
		uintptr_t block = (uintptr_t)pmm_alloc_blocks_safe(1);
		map_page(get_table(k_tmp, kernel_directory), k_tmp, block,
			PAGE_PRESENT | PAGE_READWRITE);

		invalidate_page(k_tmp);

		memset((void *)k_tmp, 0, BLOCK_SIZE);
		map_page(get_table_alloc(virtaddr, process->task.pdir), virtaddr,
			block,
			PAGE_PRESENT | PAGE_READWRITE | PAGE_USER);
	}

	map_page(get_table(k_tmp, kernel_directory), k_tmp, 0, 0);

	// TODO: good fucking god fix this please ...

	/*
	 * TODO: implement properly
	 * XXX: ensure the user stack is 16byte aligned
	 */

	uintptr_t misc_ptr = virt_misc_start;
	uintptr_t misc_region_end = virt_misc_start + 1 * PAGE_SIZE;

	uintptr_t *argv_ptrs = process_exec_copy_array(process, &misc_ptr, misc_region_end, argv, argc);
	uintptr_t *envp_ptrs = process_exec_copy_array(process, &misc_ptr, misc_region_end, envp, envc);

	size_t user_stack_size = argc + 2 + envc + 1;
	uint32_t * const user_stack = kmalloc(sizeof(uint32_t) * user_stack_size);
	assert(user_stack != NULL);
	uint32_t *user_stack_ptr = user_stack;

	// XXX: push in reverse order
	*user_stack_ptr++ = argc;
	for (size_t i = 0; i < argc; i++) {
		*user_stack_ptr++ = argv_ptrs[i];
	}
	*user_stack_ptr++ = 0;
	for (size_t i = 0; i < envc; i++) {
		*user_stack_ptr++ = envp_ptrs[i];
	}
	*user_stack_ptr++ = 0;

	const uintptr_t user_stack_top = (uintptr_t)(user_stack + user_stack_size);
	assert((uintptr_t)user_stack_ptr == user_stack_top);
	uintptr_t virt_stack_top = virt_heap_start + 256 * BLOCK_SIZE - user_stack_size * sizeof(uint32_t);
	intptr_t r = copy_to_userspace(process->task.pdir, virt_stack_top, user_stack_size * sizeof(uint32_t), user_stack);
	assert(r > 0);
	kfree(user_stack);
	kfree(argv_ptrs);
	kfree(envp_ptrs);

	registers_t *regs = (registers_t *)(process->task.kstack - sizeof(registers_t));
	regs->old_directory = (uintptr_t)process->task.pdir->physical_address;
	regs->gs = 0;
	regs->fs = 0;
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
	regs->esp = virt_stack_top;
	regs->ss = regs->ds;

	process->task.registers = regs;
	process->task.esp = (uint32_t)regs;
	process->task.ebp = process->task.esp;
	process->task.eip = (uintptr_t)return_to_regs;
}

process_t *process_spawn_init(fs_node_t *f, size_t argc, char * const * const argv) {
	assert(f != NULL);
	assert(f->length != 0);

	process_t *process = kcalloc(1, sizeof(process_t));
	assert(process != NULL);
	process->task.type = TASK_TYPE_USER_PROCESS;
	process->task.obj = process;

	process->pid = 1;
	process->ptree_node = ptree_get_init_node();
	process->ptree_node->value = process;

	process->fd_table = fd_table_new();
	assert(process->fd_table != NULL);
	fd_entry_t *tty_fd = fd_new();
	assert(tty_fd != NULL);
	tty_fd->node = &tty_node;
	tty_fd->seek = 0;
	fd_table_set(process->fd_table, 0, tty_fd);
	fd_table_set(process->fd_table, 1, fd_reference(tty_fd));
	fd_table_set(process->fd_table, 2, fd_reference(tty_fd));

	process_execve(process, f, argc, argv, 0, NULL);
	return process;
}

// FIXME: part of this should be in vmm.c
static void page_directory_clone_table(page_table_t *newtable, page_table_t *oldtable) {
	assert(oldtable != NULL);
	assert(newtable != NULL);

	uintptr_t oldkvtmp = find_vspace(kernel_directory, 1);
	assert(oldkvtmp != 0);
	uintptr_t newkvtmp = find_vspace(kernel_directory, 1);
	assert(newkvtmp != 0);

	for (uintptr_t i = 0; i < 1024; i++) {
		page_t page = oldtable->pages[i];
		if (page & PAGE_PRESENT) {
			if (page & PAGE_USER) {
				if (newtable->pages[i] != 0) {
					assert(0);
				}
				uintptr_t oldphys = page & ~0x3FF;
				uintptr_t newphys = pmm_alloc_blocks_safe(1);
				map_page(get_table(oldkvtmp, kernel_directory), oldkvtmp,
					oldphys, PAGE_PRESENT | PAGE_READWRITE);
				invalidate_page(oldkvtmp);
				map_page(get_table(newkvtmp, kernel_directory), newkvtmp,
					newphys, PAGE_PRESENT | PAGE_READWRITE);
				invalidate_page(newkvtmp);
				memcpy((void *)newkvtmp, (void *)oldkvtmp, BLOCK_SIZE);
				// XXX: we might be copying more than we want
				newtable->pages[i] = newphys | (page & 0x3FF);
			} else {
				// XXX: ignore kernel pages
//				assert(0);
				continue;
			}
		} else if (page == 0) {
			continue;
		} else {
			if (page == PAGE_VALUE_GUARD) {
				continue;
			} else if (page == PAGE_VALUE_RESERVED) {
				/* XXX: cloning this just calls for trouble*/
				assert(0);
			} else {
				assert(0);
			}
		}
	}

	map_page(get_table(oldkvtmp, kernel_directory), oldkvtmp, 0, 0);
	invalidate_page(oldkvtmp);
	map_page(get_table(newkvtmp, kernel_directory), newkvtmp, 0, 0);
	invalidate_page(newkvtmp);
}

/* XXX: don't try to clone the kernel directory */
void page_directory_clone(page_directory_t *newpdir, page_directory_t *old) {
	assert(old != NULL);
	assert(newpdir != NULL);

	for (uintptr_t i = 0; i < 1024; i++) {
		page_table_t *table = old->tables[i];
		uintptr_t table_phys = old->physical_tables[i];
		if (!(table_phys & PAGE_TABLE_PRESENT)) {
			if (table_phys == 0) {
				continue;
			} else {
				assert(0);
			}
		}

		// FIXME: write a new helper for this
		page_table_t *newtable = get_table_alloc(i << 22, newpdir);
		page_directory_clone_table(newtable, table);
	}
}

process_t *process_clone(process_t *oldproc, enum syscall_clone_flags flags, uintptr_t child_stack) {
	process_t *child = kcalloc(1, sizeof(process_t));
	if (child == NULL) {
		return NULL;
	}

	if (flags & SYSCALL_CLONE_FLAGS_FILES) {
		child->fd_table = fd_table_reference(current_process->fd_table);
	} else {
		// FIXME: not 100% correct
		child->fd_table = fd_table_clone(current_process->fd_table);
		if (child->fd_table == NULL) {
			kfree(child);
			return NULL;
		}
	}

	if (flags & SYSCALL_CLONE_FLAGS_VM) {
		child->task.pdir = page_directory_reference(oldproc->task.pdir);
	} else {
		child->task.pdir = page_directory_new();
		page_directory_clone(child->task.pdir, oldproc->task.pdir);
		process_page_directory_map_shared(child->task.pdir);
	}

	task_kstack_alloc(&child->task);
	process_map_kstack(child);

	// FIXME: strdup to the rescue
	const size_t name_strlen = strlen(current_process->name);
	child->name = kmalloc(name_strlen + 1);
	strncpy(child->name, current_process->name, name_strlen);

	child->pid = get_pid();
	child->ptree_node = ptree_new_node(oldproc->ptree_node, child);

	child->task.type = current_process->task.type;
	child->task.state = TASK_STATE_READY;
	child->task.registers = (registers_t *)(child->task.kstack - sizeof(registers_t));
	memcpy(child->task.registers, current_process->task.registers, sizeof(registers_t));
	registers_t *child_registers = child->task.registers;
	child_registers->old_directory = (uint32_t)child->task.pdir->physical_address;
	// XXX: eax = return code
	child_registers->eax = 0;
	if (child_stack != 0) {
		child_registers->esp = (uint32_t)child_stack;
	}

	child->task.esp = (uint32_t)child_registers;
	child->task.ebp = child->task.esp;
	child->task.eip = (uintptr_t)return_to_regs;
	child->task.obj = child;
	return child;
}

/*
TODO: move syscall_fork, syscall_clone and syscall_exec here
*/
void process_destroy(process_t *process) {
	printf("%s(process: %p (name: '%s'))\n", __func__, process, process->name);

	assert(process->pid != 1);
	assert(process->pid != 0);

	bitmap_unset(pid_bitmap, process->pid);
	tree_node_delete_child(ptree, process->ptree_node->parent, process->ptree_node);

	process_unmap_kstack(process);
	task_kstack_free(&process->task);
	fd_table_free(process->fd_table);
	process_page_directory_free(process->task.pdir);
	kfree(process->name);
	kfree(process);
}

void __attribute__((noreturn)) process_exit(unsigned int status) {
	if (current_process->pid == 1) {
		printf("%s: PANIC: init attempting to exit!\n", __func__);
		halt();
	}
	printf("process_exit (pid: %u name: '%s') status: %u\n", current_process->pid, current_process->name, status);
	process_t *p = current_process;

	/* free anything not critical */
	printf("%s: freeing fd table\n", __func__);
	fd_table_free(p->fd_table);
	printf("%s: unmapping kstack from userspace\n", __func__);
	process_unmap_kstack(p);
	printf("%s: page directory free\n", __func__);
	process_page_directory_free(p->task.pdir);

	printf("%s: scheduler_lock()\n", __func__);
	scheduler_lock();
	printf("%s: task_unblock_next()\n", __func__);
	task_unblock_next(&p->wait_queue);
	task_exit();
	assert(0);
}

/*
void process_reap(process_t *process) {
	task_free_kstack(&process->kstack);
	bitmap_unset(pid_bitmap, p->pid);
	kfree(process->name);
	kfree(process);
}
*/

void process_add(process_t *process) {
	assert(process != NULL);
	assert(process == process->task.obj);

	task_add(&process->task);
}

void process_init(void) {
	pid_bitmap = bitmap_new(PROCESS_MAX_PID);
	assert(pid_bitmap != NULL);
	bitmap_set(pid_bitmap, 0); // "kidle"
	bitmap_set(pid_bitmap, 1); // init
	ptree = tree_new();
	ptree->root = tree_node_new();
	ptree->root->value = NULL;
}

process_t *process_waitpid_find(tree_node_t *parent_node, pid_t pid, uint32_t options) {
	(void)options;

	for (node_t *node = parent_node->children->head; node != NULL; node = node->next) {
		tree_node_t *tree_node = node->value;
		process_t *candidate = tree_node->value;
		if (candidate->pid == pid) {
			return candidate;
		}
	}

	return NULL;
}

uint32_t process_waitpid(pid_t pid, uint32_t status, uint32_t options) {
	printf("%s(pid: %u, status: %u, options: %u)\n", __func__, pid, status, options);

	do { // TODO: implement for other than pid > 0
		process_t *p = process_waitpid_find(current_process->ptree_node, pid, options);
		if (p == NULL) {
			printf("%s: no process found, returning\n", __func__);
			return -1;
		}

		if (p->task.state == TASK_STATE_TERMINATED) {
			printf("%s: done\n", __func__);
			return p->pid;
		} else {
			printf("%s: blocking\n", __func__);
			task_block(&p->wait_queue, TASK_STATE_BLOCKED);
			printf("%s: after block\n", __func__);
		}
	} while (1);
}
