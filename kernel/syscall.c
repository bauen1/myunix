#include <assert.h>
#include <stddef.h>

#include <console.h>
#include <cpu.h>
#include <isr.h>
#include <pit.h>
#include <pmm.h>
#include <process.h>
#include <string.h>
#include <vmm.h>
#include <heap.h>
#include <task.h>
#include <gdt.h>

/*
returns 0 on success
returns -1 on failure
*/
static intptr_t map_userspace_to_kernel(page_directory_t *pdir, uintptr_t ptr, uintptr_t kptr, size_t n) {
	if ((pdir == NULL) || ((ptr & 0xFFF) != 0) || ((kptr & 0xFFF) != 0) || (n == 0)) {
		printf("%s(pdir: %p, ptr: %p, kptr: %p, n: 0x%x)\n", __func__, pdir, ptr, kptr, (uintptr_t)n);
		assert(0);
	}

	for (uintptr_t i = 0; i < n; i++) {
		uintptr_t u_virtaddr = ptr + (i * BLOCK_SIZE);
		uintptr_t k_virtaddr = kptr + (i * BLOCK_SIZE);

		map_page(get_table(k_virtaddr, kernel_directory), k_virtaddr, 0,0);
		invalidate_page(k_virtaddr);

		page_table_t *table = get_table(u_virtaddr, pdir);

		if (table == NULL) {
			printf("%s: return early! table = NULL i=%u\n", __func__, i);
			goto failure;
		}

		page_t page = get_page(table, u_virtaddr);
		if (! (page & PAGE_PRESENT)) {
			printf("%s: return early! page not present i=%u\n", __func__, i);
			goto failure;
		}
		if (! (page & PAGE_READWRITE)) {
			printf("%s: return early! page not read-write i=%u\n", __func__, i);
			goto failure;
		}
		if (! (page & PAGE_USER)) {
			printf("%s: return early! page not user i=%u\n", __func__, i);
			goto failure;
		}
		map_page(get_table(k_virtaddr, kernel_directory), k_virtaddr, page, PAGE_PRESENT | PAGE_READWRITE);
		invalidate_page(k_virtaddr);
	}

	return 0;

failure:
	dump_directory(pdir);
	for (uintptr_t i = 0; i < n; i++) {
		uintptr_t virtaddr = i * BLOCK_SIZE + kptr;
		page_table_t *table = get_table(virtaddr, kernel_directory);
		if (get_page(table, virtaddr) == 0) {
			break;
		} else {
			map_page(table, virtaddr, 0, 0);
		}
	}
	return -1;
}

static void unmap_from_kernel(uintptr_t kptr, size_t n) {
	assert(n != 0); // probably a bug
	for (uintptr_t i = 0; i < n; i++) {
		uintptr_t virtaddr = i * BLOCK_SIZE + kptr;
		map_page(get_table(virtaddr, kernel_directory), virtaddr, 0, 0);
	}
}

// only copies if all data was successfully mapped
intptr_t copy_from_userspace(page_directory_t *pdir, uintptr_t ptr, size_t n, void *buffer) {
	size_t size_in_blocks = (BLOCK_SIZE - 1 + n + (ptr & 0xfff)) / BLOCK_SIZE;
	assert(size_in_blocks != 0); // something's wrong

	uintptr_t kptr = find_vspace(kernel_directory, size_in_blocks);
	if (kptr == 0) {
		return -1;
	}
	intptr_t v = map_userspace_to_kernel(pdir, ptr & ~0xFFF, kptr, size_in_blocks);
	if (v != 0) {
		unmap_from_kernel(kptr, size_in_blocks);
		return -1;
	}

	uintptr_t kptr2 = kptr + (ptr & 0xFFF);

	memcpy(buffer, (void *)kptr2, n);

	unmap_from_kernel(kptr, size_in_blocks);
	return n;
}

// TODO: improve this, a lot
intptr_t copy_from_userspace_string(page_directory_t *pdir, uintptr_t ptr, size_t n, void *buffer) {
	assert(pdir != NULL);
	assert(ptr != 0);
	assert(buffer != NULL);

	uintptr_t kptr = find_vspace(kernel_directory, 1);
	if (kptr == 0) {
		return -1;
	}

	size_t i = 0;
	char *buf = buffer;
	uintptr_t p = ptr & ~0xFFF;
	size_t offset = ptr & 0xFFF;

	while(1) {
		intptr_t v = map_userspace_to_kernel(pdir, p, kptr, 1);
		if (v != 0) {
			unmap_from_kernel(kptr, 1);
			return -1;
		}

		for (char *v = (char *)(kptr + offset); (uintptr_t)v < kptr + BLOCK_SIZE; v++) {
			if ((uintptr_t)buf >= (uintptr_t)buffer + n) {
				unmap_from_kernel(kptr, 1);
				return -1;
			}
			*buf = *v;
			if (*v == 0) {
				unmap_from_kernel(kptr, 1);
				return i;
			} else {
				i++;
				buf++;
			}
		}
		*buf = 0;
		offset = 0;
		p += BLOCK_SIZE;
	}

	unmap_from_kernel(kptr, 1);
	return -1;
}

// TODO: improve this, a lot
intptr_t copy_from_userspace_ptr_array(page_directory_t *pdir, uintptr_t ptr, size_t size, uintptr_t *buffer) {
	assert(pdir != NULL);
	assert(buffer != NULL);

	printf("%s(pdir: %p, ptr: %p, size: %u, buffer: %p)\n", __func__, pdir, ptr, (unsigned int)size, buffer);

	if (ptr == 0) {
		return 0;
	}

	if ((ptr % 4) != 0) {
		printf("%s: misaligned pointer %p\n", __func__, ptr);
		return -1;
	}

	uintptr_t kptr = find_vspace(kernel_directory, 1);
	if (kptr == 0) {
		printf("%s: could not find kernel vspace\n", __func__);
		return -1;
	}

	size_t i = 0;
	uintptr_t *buf = buffer;
	uintptr_t p = ptr & ~0xFFF;
	size_t offset = ptr & 0xFFF;

	while(1) {
		intptr_t v = map_userspace_to_kernel(pdir, p, kptr, 1);
		if (v != 0) {
			printf("%s: failure mapping %p from userspace\n", __func__, p);
			unmap_from_kernel(kptr, 1);
			return -1;
		}

		for (uintptr_t *v = (uintptr_t *)(kptr + offset); (uintptr_t)v < kptr + BLOCK_SIZE; v++) {
			if ((uintptr_t)buf >= (uintptr_t)buffer + size * sizeof(uintptr_t)) {
				printf("%s: end of buffer\n", __func__);
				unmap_from_kernel(kptr, 1);
				return i - 1;
			}
			printf("%s: v: %p\n", __func__, *v);
			*buf = *v;
			if (*v == 0) {
				printf("%s: end of array\n", __func__);
				unmap_from_kernel(kptr, 1);
				return i;
			} else {
				i++;
				buf++;
			}
		}
		offset = 0;
		p += BLOCK_SIZE;
	}

	unmap_from_kernel(kptr, 1);
	return -1;
}

// TODO: limit maximum number of array elements / element size
char **copy_from_userspace_array(page_directory_t *pdir, uintptr_t ptr, size_t size) {
	char **array = kcalloc(size, sizeof(char *));
	if (array == NULL) {
		printf("%s: array allocation out of memory\n", __func__);
		return NULL;
	}

	uintptr_t *user_array = kcalloc(size, sizeof(uintptr_t));
	if (user_array == NULL) {
		printf("%s: out of memory\n", __func__);
		kfree(array);
		return NULL;
	}
	{
		intptr_t r = copy_from_userspace_ptr_array(pdir, ptr, size, user_array);
		if (r < 0) {
			kfree(array);
			kfree(user_array);
			printf("%s: copy_from_userspace_ptr_array returned %d\n", __func__, r);
			return NULL;
		}
	}

	for (size_t i = 0; i < (size - 1); i++) {
		const uintptr_t user_ptr = user_array[i];
		if (user_ptr == 0) {
			printf("%s: user_ptr = 0 assuming end\n", __func__);
			array[i] = NULL;
			return array;
		} else {
			const size_t element_size = 256;
			char *element = kmalloc(element_size);
			printf("%s: copy %p from user\n", __func__, user_array[i]);
			intptr_t r = copy_from_userspace_string(pdir, user_array[i], element_size, element);
			if (r < 0) {
				printf("%s: copy failure! (r=%d)\n", __func__, r);
				kfree(array);
				kfree(user_array);
				return NULL;
			}
			array[i] = element;
		}
	}

	array[size - 1] = NULL;
	kfree(user_array);
	return array;
}

intptr_t copy_to_userspace(page_directory_t *pdir, uintptr_t ptr, size_t n, const void *buffer) {
	size_t size_in_blocks = (BLOCK_SIZE - 1 + n + (ptr & 0xfff)) / BLOCK_SIZE;
	assert(size_in_blocks != 0);

	uintptr_t kptr = find_vspace(kernel_directory, size_in_blocks);
	if (kptr == 0) {
		printf("%s: kptr = NULL !\n", __func__);
		return -1;
	}
	intptr_t v = map_userspace_to_kernel(pdir, ptr & ~0xFFF, kptr, size_in_blocks);
	if (v != 0) {
		unmap_from_kernel(kptr, size_in_blocks);
		return -1;
	}

	uintptr_t kptr2 = kptr + (ptr & 0xFFF);

	memcpy((void *)kptr2, buffer, n);

	unmap_from_kernel(kptr, size_in_blocks);
	return n;
}

static uint32_t syscall_execve(registers_t *regs) {
	uintptr_t user_path = regs->ebx;
	uintptr_t user_argv = regs->ecx;
	uintptr_t user_envp = regs->edx;

	printf("%s(user_path: %p, user_argv: %p, user_envp: %p)\n", __func__, user_path, user_argv, user_envp);

	char path[256];
	intptr_t r = copy_from_userspace_string(current_process->task.pdir, user_path, 256, path);
	if (r < 0) {
		return -1;
	}
	path[255] = 0;
	printf(" path='%s'\n", path);

	char **argv = NULL;
	size_t argc = 0;
	if (user_argv == 0) {
		argv = kmalloc(sizeof(char *) * 2);
		argv[0] = &path[0];
		argv[1] = NULL;
		argc = 1;
	} else {
		argv = copy_from_userspace_array(current_process->task.pdir, user_argv, 20);
		if (argv != NULL) {
			for (argc = 0; argv[argc] != NULL; argc++) {;}
			for (size_t i = 0; i < argc; i++) {
				printf("argv[%u]: %p '%s'\n", (unsigned int)i, argv[i], argv[i]);
			}
		}
	}
	char **envp = NULL;
	size_t envc = 0;
	if (user_envp != 0) {
		envp = copy_from_userspace_array(current_process->task.pdir, user_envp, 20);
		if (envp != NULL) {
			for (envc = 0; envp[envc] != NULL; envc++) {;}
			for (size_t i = 0; i < envc; i++) {
				printf("envp[%u]: %p '%s'\n", (unsigned int)i, envp[i], envp[i]);
			}
		}
	}

	fs_node_t *f = kopen(path, 0);
	if (f == NULL) {
		printf(" '%s' not found\n", path);
		if (argv != NULL) {
			for (char **v = argv; *v != NULL; v++) {
				kfree(*v);
			}
			kfree(argv);
		}
		if (envp != NULL) {
			for (char **v = envp; *v != NULL; v++) {
				kfree(*v);
			}
			kfree(envp);
		}
		return 2;
	} else {
		process_execve(current_process, f, argc, argv, envc, envp);
		fs_close(&f);
		if (argv != NULL) {
			for (char **v = argv; *v != NULL; v++) {
				kfree(*v);
			}
			kfree(argv);
		}
		if (envp != NULL) {
			for (char **v = envp; *v != NULL; v++) {
				kfree(*v);
			}
			kfree(envp);
		}
		printf("%s: kstack: %p\n", __func__, current_task->kstack);
		current_task->state = TASK_STATE_RUNNING;
		tss_set_kstack(current_process->task.kstack);
		return 0;
	}
}

static uint32_t syscall_clone(registers_t *regs) {
	uintptr_t flags = regs->ebx;
	uintptr_t child_stack = regs->ecx;

	process_t *child = process_clone(current_process, flags, child_stack);
	if (child == NULL) {
		return -1;
	} else {
		process_add(child);
		return child->pid;
	}
}

// TODO: implement syscall_lseek completely
static uint32_t syscall_lseek(registers_t *regs) {
	uintptr_t fd_num = regs->ebx;
	uintptr_t offset = regs->ecx;
	uintptr_t flags = regs->edx;

	fd_entry_t *fd = fd_table_get(current_process->fd_table, fd_num);
	if (fd == NULL) {
		return -1;
	}

	if (flags == 0) {
		(void)flags;
		fd->seek = offset;
		return 0;
	} else {
		return -1;
	}
}

// TODO: implement syscall_times
static uint32_t syscall_times(registers_t *regs) {
	(void)regs;
//	printf("%s()\n", __func__);
	return -1;
}

// TODO: implement syscall_mkdir properly
static uint32_t syscall_mkdir(registers_t *regs) {
	uintptr_t user_path = regs->ebx;
	// regs->ecx mode
	char buffer[256];
	intptr_t r = copy_from_userspace_string(current_process->task.pdir, user_path, 256, buffer);
	if (r < 0) {
		return -1;
	}

	fs_node_t *node = kopen(buffer, 0);
	if (node != NULL) {
		printf("already exists\n");
		fs_close(&node);
		return -1;
	}

	// FIXME: creating directories not in the root would be cool
	fs_mkdir(fs_root_mount->node, buffer, regs->ecx);
	return 0;
}

// TODO: implement syscall_rmdir properly
static uint32_t syscall_rmdir(registers_t *regs) {
	(void)regs;
	printf("%s()\n", __func__);
	// regs->ebx path
	/*
	maybe just call syscall_unlink (possibly adding a flag)
	*/
	return -1;
}

// TODO: implement syscall_create properly
static uint32_t syscall_create(registers_t *regs) {
	uintptr_t user_path = regs->ebx;
	// regs->ecx mode
	char buffer[256];
	intptr_t r = copy_from_userspace_string(current_process->task.pdir, user_path, 256, buffer);
	if (r < 0) {
		return -1;
	}

	// FIXME: creating files not in the root would be cool
	fs_create(fs_root_mount->node, buffer, regs->ecx);
	return 0;
}

// TODO: implement syscall_unlink properly
static uint32_t syscall_unlink(registers_t *regs) {
	uintptr_t user_path = regs->ebx;
	char buffer[256];
	intptr_t r = copy_from_userspace_string(current_process->task.pdir, user_path, 256, buffer);
	if (r < 0) {
		return -1;
	}

	// FIXME: unlinking not in the root would be cool
	fs_unlink(fs_root_mount->node, buffer);
	return 0;
}

static uint32_t syscall_waitpid(registers_t *regs) {
	int32_t pid = regs->ebx;
	uint32_t status = regs->ecx;
	uint32_t options = regs->edx;

	printf("%s(pid: %i, status: %u, options: %u)\n", __func__, pid, status, options);

	uint32_t r = process_waitpid(pid, status, options);

	process_waitpid(pid, status, options);

	printf("%s: done: %u\n", __func__, r);
	return r;
}

// TODO: implement syscall_gettimeofday properly
static uint32_t syscall_gettimeofday(registers_t *regs) {
	(void)regs;
//	printf("%s()\n", __func__);
	return -1;
}

static uint32_t syscall_getpid(registers_t *regs) {
	(void)regs;
	return (uint32_t)current_process->pid;
}

// FIXME: improve implementation
static void __attribute__((noreturn)) syscall_exit(registers_t *regs) {
	printf("pid %u exit(%u)\n", current_process->pid, regs->ebx);
	process_exit(regs->ebx);
}

// FIXME: syscall_open implement properly
static uint32_t syscall_open(registers_t *regs) {
	uintptr_t user_path = regs->ebx;
	uint32_t flags = regs->ecx;
	uint32_t mode = regs->edx;

	char path[256];
	intptr_t r = copy_from_userspace_string(current_process->task.pdir, user_path, sizeof(path), path);
	if (r < 0) {
		return -1;
	}

	(void)mode; (void)flags;

	fs_node_t *node = kopen(path, 0);
	if (node) {
		fd_entry_t *fd = fd_new();
		if (fd == NULL) {
			return -1;
		}
		fd->node = node;
		fd->seek = 0;
		return fd_table_append(current_process->fd_table, fd);
	} else {
		return -2;
	}
}

static uint32_t syscall_dup(registers_t *regs) {
	uint32_t oldfd = regs->ebx;
	assert(current_process->fd_table != NULL);
	fd_entry_t *fd = fd_table_get(current_process->fd_table, oldfd);
	if (fd == NULL) {
		return -1;
	}

	int r = fd_table_append(current_process->fd_table, fd_reference(fd));
	if (r == -1) {
		fd_free(fd);
		return -1;
	}
	return r;
}

static uint32_t syscall_dup2(registers_t *regs) {
	uint32_t oldfd = regs->ebx;
	uint32_t newfd = regs->ecx;
	fd_entry_t *fd = fd_table_get(current_process->fd_table, oldfd);
	if (fd == NULL) {
		return -1;
	}
	if (newfd == oldfd) {
		return newfd;
	}
	// FIXME: sanitize newfd
	return fd_table_set(current_process->fd_table, newfd, fd_reference(fd));
}

// TODO: implement syscall_dup3 correctly
static uint32_t syscall_dup3(registers_t *regs) {
	uint32_t oldfd = regs->ebx;
	uint32_t newfd = regs->ecx;
	uint32_t flags = regs->edx;
	printf("%s(oldfd: %u, newfd: %u, flags: %u)\n", __func__, oldfd, newfd, flags);
	return syscall_dup2(regs);
}

static uint32_t syscall_close(registers_t *regs) {
	uintptr_t fd_num = regs->ebx;

	fd_entry_t *fd = fd_table_get(current_process->fd_table, fd_num);
	if (fd == NULL) {
		return -1;
	}

	fd_table_set(current_process->fd_table, fd_num, NULL);
	fd_free(fd);
	return 0;
}

static uint32_t syscall_read(registers_t *regs) {
	uintptr_t fd_num = regs->ebx;
	uintptr_t ptr = regs->ecx;
	uintptr_t length = regs->edx;
//	printf("%s(fd: %u, buf: 0x%x, length: 0x%x)\n", __func__, fd_num, ptr, length);

	if (current_process->fd_table == NULL) {
//		printf(" return early fd_tabel == NULL\n");
		return -1;
	}

	fd_entry_t *fd = fd_table_get(current_process->fd_table, fd_num);
	if (fd == NULL) {
		return -1;
	}

	// FIXME: handle length = 0 properly
	if (length == 0) {
//		return fs_read(fd->node, fd->seek, 0, NULL);
		return 0;
	}

	size_t n_blocks = (BLOCK_SIZE - 1 + length + (ptr & 0xfff)) / BLOCK_SIZE;
	page_directory_t *pdir = current_process->task.pdir;
	uintptr_t kptr = find_vspace(kernel_directory, n_blocks);
	uintptr_t kptr2 = kptr + (ptr & 0xFFF);
	size_t v = map_userspace_to_kernel(pdir, ptr & ~0xFFF, kptr, n_blocks);
	if (v != 0) {
		printf("syscall_read early abort: %u\n", (uintptr_t)v);
		return -1;
	}
	uint32_t r = fs_read(fd->node, fd->seek, length, (void *)kptr2);
	if (r != (uint32_t)-1) {
		fd->seek += r;
	}
	unmap_from_kernel(kptr, n_blocks);
	return r;
}

static uint32_t syscall_write(registers_t *regs) {
	uint32_t fd_num = regs->ebx;
	uintptr_t ptr = regs->ecx;
	uintptr_t length = regs->edx;
//	printf("write(fd: %u, buf: 0x%x, length: 0x%x)\n", fd_num, ptr, length);

	fd_entry_t *fd = fd_table_get(current_process->fd_table, fd_num);
	if (fd == NULL) {
		return -1;
	}
	if (length == 0) {
//		return fs_read(fd->node, fd->seek, 0, NULL);
		return 0;
	}

	size_t n_blocks = (BLOCK_SIZE - 1 + length + (ptr & 0xfff)) / BLOCK_SIZE;
	assert(n_blocks != 0);
	page_directory_t *pdir = current_process->task.pdir;
	uintptr_t kptr = find_vspace(kernel_directory, n_blocks);
	uintptr_t kptr2 = kptr + (ptr & 0xFFF);
	size_t v = map_userspace_to_kernel(pdir, ptr & ~0xFFF, kptr, n_blocks);
	if (v != 0) {
		printf("syscall_write early abort v: %u\n", (uintptr_t)v);
		return -1;
	}
	uint32_t r = fs_write(fd->node, fd->seek, length, (uint8_t *)kptr2);
	if (r != (uint32_t)-1) {
		fd->seek += r;
	}
	unmap_from_kernel(kptr, n_blocks);
	return r;
}

// FIXME: improve
static uint32_t syscall_mmap_anon(registers_t *regs) {
	// FIXME: validate len etc...
	uintptr_t addr = regs->ebx;
	size_t len = regs->ecx;
	// regs->edx prot
	if (len > BLOCK_SIZE*300) { // 300*4kb max alloc
		printf("length too big!\n");
		return -1;
	}

	len = len / BLOCK_SIZE;

	if (addr == 0) {
		addr = find_vspace(current_process->task.pdir, len);
	}

	uint32_t prot;
	switch (regs->edx) {
		case (1):
			prot = PAGE_PRESENT | PAGE_USER;
			break;;
		case (3):
			prot = PAGE_PRESENT | PAGE_USER | PAGE_READWRITE;
			break;;
		default:
			// TODO: free vspace and return error
		case (0):
			prot = PAGE_PRESENT | PAGE_USER;
			break;;
	}

	uintptr_t kptr = find_vspace(kernel_directory, 1);
	assert(kptr != 0);
	for (size_t i = 0; i < len; i++) {
		uintptr_t virtaddr = addr + i * BLOCK_SIZE;
		uintptr_t block = pmm_alloc_blocks_safe(1);
		// TODO: free already mapped pages
		assert(get_page(get_table(virtaddr, current_process->task.pdir), virtaddr) == PAGE_VALUE_RESERVED);

		map_page(get_table(kptr, kernel_directory), kptr, block, PAGE_PRESENT | PAGE_READWRITE);
		memset((void *)kptr, 0, BLOCK_SIZE);
		map_page(get_table(kptr, kernel_directory), kptr, 0, 0);
		invalidate_page(kptr);
		map_page(get_table(virtaddr, current_process->task.pdir), virtaddr,
			block,
			prot);
	}

	return addr;
}

static uint32_t syscall_munmap(registers_t *regs) {
	uintptr_t addr = regs->ebx;
	uintptr_t length = regs->ecx;

	if ((addr & 0xFFF) != 0) {
		// not aligned
		return -1;
	}

	if (length == 0) {
		// length too small
		return -1;
	}

	if ((length & 0xFFF) != 0) {
		// length not aligned
		length = (length & 0xFFF) + 0x1000;
	}

	for (uintptr_t i = 0; i < length; i += BLOCK_SIZE) {
		page_t p = get_page(get_table(i, current_process->task.pdir), i);
		if (p == 0) {
			continue;
		}

		if (p & PAGE_USER) {
			pmm_free_blocks(p & ~0xFFF, 1);
			map_page(get_table(i, current_process->task.pdir), i, 0, 0);
			continue;
		} else {
			printf("refusing to munmap address %p (not user address)\n", (uintptr_t)p);
		}

		// something worth debugging went wrong
		assert(0);
		return -1;
	}
	return 0;
}

static uint32_t syscall_nano_sleep(registers_t *regs) {
//	unsigned long target = ticks + regs->ebx;
	uint32_t delay = regs->ebx;
	uint64_t target = timer_ticks + delay;
	task_sleep_until(target);
	return 0;
}

struct utsname {
	char sysname[256];
	char nodename[256];
	char release[256];
	char version[256];
	char machine[256];
};

static struct utsname uname = {
	/* sysname  */ "myunix",
	/* nodename */ "",
	/* release  */ "0.0.1-git",
	/* version  */ "0 0 1",
	/* machine  */ "",
};

// TODO: broken
static uint32_t syscall_uname(registers_t *regs) {
	uintptr_t addr = regs->ebx;

	printf("sizeof(uname): 0x%x\n", sizeof(uname));
	copy_to_userspace(
		current_process->task.pdir,
		addr,
		sizeof(uname),
		&uname);

	return 0;
}

struct dirent_user {
	int64_t d_ino;
	unsigned char d_type;
	char d_name[256];
};

static uint32_t syscall_readdir(registers_t *regs) {
	uint32_t fd_num = regs->ebx;
	uint32_t d_ino = regs->ecx;
	uintptr_t dirent_user = regs->edx;

	fd_entry_t *fd = fd_table_get(current_process->fd_table, fd_num);
	if (fd == NULL) {
		printf("%s: fd not valid\n", __func__);
		return -1;
	}

	struct dirent *dirent_k = fs_readdir(fd->node, d_ino);
	if (dirent_k == NULL) {
		return 0;
	} else {
		struct dirent_user dirent_u;
		strncpy(dirent_u.d_name, dirent_k->name, sizeof(dirent_u.d_name));
		dirent_u.d_ino = dirent_k->ino;
		dirent_u.d_type = dirent_k->type;
		int32_t r = copy_to_userspace(current_process->task.pdir, dirent_user, sizeof(struct dirent_user), &dirent_u);
		if (r < 0) {
			printf("%s: copy_to_userspace\n", __func__);
			return -1;
		}
	}

	return 1;
}

static uint32_t syscall_dumpregs(registers_t *regs) {
	dump_regs(regs);
	return 0;
}

static void syscall_handler(registers_t *regs) {
//	printf("syscall regs->eax: 0x%x\n", regs->eax);
	switch (regs->eax) {
		case 0x01:
			syscall_exit(regs);
			break;
		case 0x03:
			regs->eax = syscall_read(regs);
			break;
		case 0x04:
			regs->eax = syscall_write(regs);
			break;
		case 0x05:
			regs->eax = syscall_open(regs);
			break;
		case 0x06:
			regs->eax = syscall_close(regs);
			break;
		case 0x07:
			regs->eax = syscall_waitpid(regs);
			break;
		case 0x08:
			regs->eax = syscall_create(regs);
			break;
		case 0x0A:
			regs->eax = syscall_unlink(regs);
			break;
		case 0x0B:
			regs->eax = syscall_execve(regs);
			break;
		case 0x0C:
			regs->eax = syscall_uname(regs);
			break;
		case 0x13:
			regs->eax = syscall_lseek(regs);
			break;
		case 0x14:
			regs->eax = syscall_getpid(regs);
			break;
		case 0x27:
			regs->eax = syscall_mkdir(regs);
			break;
		case 0x28:
			regs->eax = syscall_rmdir(regs);
			break;
		case 0x29:
			regs->eax = syscall_dup(regs);
			break;
		case 0x2b:
			regs->eax = syscall_times(regs);
			break;
		case 0x3f:
			regs->eax = syscall_dup2(regs);
			break;
		case 0x4e:
			regs->eax = syscall_gettimeofday(regs);
			break;
		case 0x59:
			regs->eax = syscall_readdir(regs);
			break;
		case 0xFF:
			regs->eax = syscall_dumpregs(regs);
			break;
		case 0x78:
			regs->eax = syscall_clone(regs);
			break;
		case 0xa2:
			regs->eax = syscall_nano_sleep(regs);
			break;
		case 0x14a:
			regs->eax = syscall_dup3(regs);
			break;
		case 0x200:
			regs->eax = syscall_mmap_anon(regs);
			break;
		case 0x201:
			regs->eax = syscall_munmap(regs);
			break;
		default:
			printf("undefined syscall %u\n", regs->eax);
			assert(0);
			regs->eax = (uint32_t)-1;
			break;
	}
}

void syscall_init() {
	isr_set_handler(0x80, syscall_handler);
}
