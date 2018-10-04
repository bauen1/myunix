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
			fs_close(fd->node);
		}
		kfree(fd);
	}
}

/* helper */
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
	printf("%s()\n", __func__);
	fd_table_t *newfdt = fd_table_new();
	assert(newfdt != NULL);
	for (unsigned int i = 0; i < oldfdt->length; i++) {
		fd_entry_t *oldfd = fd_table_get(oldfdt, i);
		printf("%s: try copy fd %u (%p)\n", __func__, i, oldfd);
		// FIXME: not 100% posix compliant
		fd_table_set(newfdt, i, fd_reference(oldfd));
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

/* helpers */
int fd_table_set(fd_table_t *fd_table, unsigned int i, fd_entry_t *entry) {
	assert(fd_table != NULL);
	assert(entry != NULL);

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

/* helpers */
static void process_map_shared_region(page_directory_t *pdir, uintptr_t start, uintptr_t end, unsigned int permissions) {
	assert(pdir != NULL);
	assert((start & 0xFFF) == 0);

	if ((end & 0xFFF) != 0) {
		printf(" WARN: end 0x%8x not aligned!\n", end);
	}

	for (uintptr_t i = 0; i < (end - start); i += BLOCK_SIZE) {
		uintptr_t addr = start + i;
		map_page(get_table_alloc(addr, pdir), addr, addr, permissions);
	}
}

static void process_unmap_shared_region(page_directory_t *pdir, uintptr_t start, uintptr_t end) {
	assert(pdir != NULL);
	assert((start & 0xFFF) == 0);

	if ((end & 0xFFF) != 0) {
		printf(" WARN: end 0x%8x not aligned!\n", end);
	}

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
		return page_directory_free(pdir);
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
					assert(0);
				}
			}
		} else {
			assert(pdir->tables[i] == NULL);
		}
	}
}

void process_exec2(process_t *process, fs_node_t *f, int argc, const char **argv) {
	const uintptr_t virt_text_start = 0x1000000;
	const uintptr_t virt_heap_start = 0x2000000; // max 16mb text
	const uintptr_t virt_stack_start = 0x8000000;
	const uintptr_t virt_misc_start = 0x10000000;

	assert(process != NULL);
	assert(f != NULL);
	assert(f->length != 0);

	if (process->task.pdir != NULL) {
		printf("%s: freeing old pdir\n", __func__);
		process_page_directory_free(process->task.pdir);
	}

	process->task.type = TASK_TYPE_USER_PROCESS;
	process->task.pdir = process_page_directory_new();
	assert(process->task.pdir != NULL);
	task_kstack_alloc(&process->task);
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

	/*
	 * TODO: implement properly
	 * XXX: ensure the user stack is 16byte aligned
	 */
	uintptr_t buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	size_t init_frame_size = sizeof(buf);
	uintptr_t user_stack = virt_heap_start + (256-1)*BLOCK_SIZE;
	user_stack -= init_frame_size;
	buf[0] = argc; // argc
	buf[1] = virt_misc_start; // argv[0];
	buf[2] = 0; // argv[1]
	buf[3] = 0; // env[0]
	buf[4] = 0; // env[1]

	copy_to_userspace(process->task.pdir, user_stack, init_frame_size, buf);
	if (argv != NULL) {
		copy_to_userspace(process->task.pdir, buf[1], strlen(argv[0]) + 1, argv[0]);
	} else {
		printf("%s: WARN: argv = NULL!\n", __func__);
	}

	registers_t *regs = (registers_t *)(process->task.kstack - sizeof(registers_t));
	regs->old_directory = (uintptr_t)process->task.pdir->physical_address;
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
	regs->esp = user_stack;
	regs->ss = regs->ds;

	process->task.registers = regs;

	process->task.esp = (uint32_t)regs;
	process->task.ebp = process->task.esp;
	process->task.eip = (uintptr_t)return_to_regs;
}

process_t *process_exec(fs_node_t *f, int argc, const char **argv) {
	assert(f != NULL);
	assert(f->length != 0);
	printf("%s(f: %p (f->name: '%s'), argc: %i, argv: %p)\n", __func__, f, f->name, argc, argv);

	process_t *process = kcalloc(1, sizeof(process_t));
	assert(process != NULL);
	process->task.type = TASK_TYPE_USER_PROCESS;
	process->task.obj = process;

	process->fd_table = fd_table_new();
	assert(process->fd_table != NULL);
	fd_entry_t *tty_fd = fd_new();
	assert(tty_fd != NULL);
	tty_fd->node = &tty_node;
	tty_fd->seek = 0;
	fd_table_set(process->fd_table, 0, tty_fd);
	fd_table_set(process->fd_table, 1, fd_reference(tty_fd));
	fd_table_set(process->fd_table, 2, fd_reference(tty_fd));

	process_exec2(process, f, argc, argv);
	return process;
}

// FIXME: part of this should be in vmm.c
static void page_directory_clone_table(page_table_t *newtable, page_table_t *oldtable) {
	assert(oldtable != NULL);
	assert(newtable != NULL);
//	printf("%s(newtable: %p, oldtable: %p)\n", __func__, newtable, oldtable);

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
//				printf("%s: cloning page %i from %p to: %p\n", __func__, i, oldphys, newphys);
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
//				assert(0);
				// XXX: ignore kernel pages
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
		printf("%s: attempting to fd_table_clone\n", __func__);
		child->fd_table = fd_table_clone(current_process->fd_table);
		printf("%s: successfully cloned fd_table\n", __func__);
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
	printf("%s: child name: '%s'\n", __func__, child->name);

	child->pid = get_pid();
	printf("%s: child pid: %u\n", __func__, child->pid);

	child->task.type = current_process->task.type;
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

	bitmap_unset(pid_bitmap, process->pid);
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
	printf("%s: removing process %p (name: '%s') from queue\n", __func__, p, p->name);
	task_remove(&p->task);

	/* free process */
	fd_table_free(p->fd_table);
	bitmap_unset(pid_bitmap, p->pid);
	process_unmap_kstack(p);
	task_kstack_delayed_free(&p->task);
	process_page_directory_free(p->task.pdir);
	kfree(p->name);
	kfree(p);

	printf("%s: __switch_direct\n", __func__);

	__switch_direct();
}

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
}
