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

// 0 on success, size mapped on failure
// TODO: CRITICAL FIXME: may return 0 incase of failure on first page early which is success
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
			printf(" table == NULL; returning early: %u\n", i);
			goto failure;
		}

		page_t page = get_page(table, u_virtaddr);
		if (! (page & PAGE_PRESENT)) {
			// FIXME: this happens way too often (logic wrong ?)
			printf(" not present; returning early: %u\n", i);
			goto failure;
		}
		if (! (page & PAGE_READWRITE)) {
			printf(" not read-write; returning early: %u\n", i);
			goto failure;
		}
		if (! (page & PAGE_USER)) {
			printf(" not user; returning early: %u\n", i);
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

// 0 on success
static void unmap_from_kernel(uintptr_t kptr, size_t n) {
//	printf("unmap_from_kernel(0x%x, 0x%x)\n", kptr, n);
	for (uintptr_t i = 0; i < n; i++) {
		uintptr_t virtaddr = i * BLOCK_SIZE + kptr;
//		printf(" page[0x%x] = 0\n", virtaddr);
		map_page(get_table(virtaddr, kernel_directory), virtaddr, 0, 0);
	}
}

// only copies if all data was successfully mapped
intptr_t copy_from_userspace(page_directory_t *pdir, uintptr_t ptr, size_t n, void *buffer) {
	size_t size_in_blocks = (BLOCK_SIZE - 1 + n + (ptr & 0xfff)) / BLOCK_SIZE;
	assert(size_in_blocks != 0); // somethings wrong

	uintptr_t kptr = find_vspace(kernel_directory, size_in_blocks);
	if (kptr == 0) {
		return -1;
	}
	intptr_t v = map_userspace_to_kernel(pdir, ptr & ~0xFFF, kptr, size_in_blocks);
	if (v != 0) {
		unmap_from_kernel(kptr, v);
		return -1;
	}

	uintptr_t kptr2 = kptr + (ptr & 0xFFF);

	memcpy(buffer, (void *)kptr2, n);

	unmap_from_kernel(kptr, v);
	return n;
}

intptr_t copy_to_userspace(page_directory_t *pdir, uintptr_t ptr, size_t n, void *buffer) {
	printf("%s(pdir: 0x%x, ptr: 0x%x, n: 0x%x, buffer: 0x%x);\n", __func__, (uintptr_t)pdir, ptr, (uintptr_t)n, (uintptr_t)buffer);
	size_t size_in_blocks = (BLOCK_SIZE - 1 + n + (ptr & 0xfff)) / BLOCK_SIZE;
	assert(size_in_blocks != 0);

	uintptr_t kptr = find_vspace(kernel_directory, size_in_blocks);
	if (kptr == 0) {
		printf("kptr == 0\n");
		return -1;
	}
	intptr_t v = map_userspace_to_kernel(pdir, ptr & ~0xFFF, kptr, size_in_blocks);
	printf("v: %u\n", v);
	if (v != 0) {
		unmap_from_kernel(kptr, v);
		return -1;
	}

	uintptr_t kptr2 = kptr + (ptr & 0xFFF);

	printf("memcpy(dest: 0x%x, src: 0x%x, len: 0x%x);\n", kptr2, (uintptr_t)buffer, (uintptr_t)n);
	memcpy((void *)kptr2, buffer, n);

	unmap_from_kernel(kptr, v);
	return n;
}

/*
static uint32_t syscall_execve(registers_t *regs) {
//	printf("execve()\n");
	return -1;
}

static uint32_t syscall_fork(registers_t *regs) {
//	printf("fork()\n");
	return = -1;
}

static uint32_t syscall_fstat(registers_t *regs) {
//	printf("fstat()\n");
	return -1;
}

static uint32_t syscall_stat(registers_t *regs) {
//	printf("stat()\n");
	return -1;
}

static uint32_t syscall_link(registers_t *regs) {
//	printf("link()\n");
	return -1;
}
*/

static uint32_t syscall_lseek(registers_t *regs) {
//	printf("lseek()\n");
	uintptr_t fd_num = regs->ebx;
	uintptr_t offset = regs->ecx;
	uintptr_t flags = regs->edx;

	fd_table_t *fdt = current_process->fd_table;
	if (fd_num > fdt->length) {
		return -1;
	}

	fd_entry_t *fd = fdt->entries[fd_num];
	if (fd != NULL) {
		// TODO: implement correctly
		(void)flags;
		fd->seek = offset;
	}

	return -1;
}

static uint32_t syscall_times(registers_t *regs) {
	(void)regs;
//	printf("times()\n");
	return -1;
}

static uint32_t syscall_mkdir(registers_t *regs) {
	// regs->ebx 256 path
	// regs->ecx mode
	char buffer[256];
	intptr_t r = copy_from_userspace(current_process->pdir, regs->ebx, 255, buffer);
	if (r < 0) {
		return -1;
	}

	// FIXME: creating directories not in the root would be cool
	fs_mkdir(fs_root_mount->node, buffer, regs->ecx);
	return 0;
}

static uint32_t syscall_rmdir(registers_t *regs) {
	(void)regs;
	// regs->ebx path
	// TODO: just call syscall_unlink (maybe adding a flag)
	return -1;
}

static uint32_t syscall_create(registers_t *regs) {
	// regs->ebx path
	// regs->ecx mode
	char buffer[256];
	intptr_t r = copy_from_userspace(current_process->pdir, regs->ebx, 255, buffer);
	if (r < 0) {
		return -1;
	}

	// FIXME: creating files not in the root would be cool
	fs_create(fs_root_mount->node, buffer, regs->ecx);
	return 0;
}

static uint32_t syscall_unlink(registers_t *regs) {
	// regs->ebx path
	char buffer[256];
	intptr_t r = copy_from_userspace(current_process->pdir, regs->ebx, 255, buffer);
	if (r < 0) {
		return -1;
	}

	// FIXME: unlinking not in the root would be cool
	fs_unlink(fs_root_mount->node, buffer);
	return 0;
}

/*
static uint32_t syscall_wait(registers_t *regs) {
//	printf("wait()\n");
	return -1;
}
*/

static uint32_t syscall_gettimeofday(registers_t *regs) {
	(void)regs;
//	printf("gettimeofday()\n");
	return -1;
}

static uint32_t syscall_getpid(registers_t *regs) {
	(void)regs;
//	printf("getpid()\n");
	return (uint32_t)current_process->pid;
}

static void __attribute__((noreturn)) syscall_exit(registers_t *regs) {
	printf("pid %u exit(%u)\n", current_process->pid, regs->ebx);
	process_exit(regs->ebx);
}

static uint32_t process_append_fd(process_t *proc, fs_node_t *node) {
	fd_table_t *fdt = proc->fd_table;
	assert(fdt != NULL);

	fd_entry_t *entry = kcalloc(1, sizeof(fd_entry_t));
	if (entry == NULL) {
		return -1;
	}
	entry->node = node;
	entry->seek = 0;

	for (unsigned int i = 0; i < fdt->length; i++) { // gaps
		if (fdt->entries[i] == NULL) {
			fdt->entries[i] = entry;
			return i;
		}
	}

	// no gaps found, is the table full ?
	if (fdt->length == fdt->capacity) {
		fdt->capacity = fdt->capacity + 8;
		fdt->entries = krealloc(fdt->entries, fdt->capacity * sizeof(fd_entry_t *));
		// FIXME: check for OOM
	}

	fdt->entries[fdt->length] = entry;
	fdt->length++;
	return fdt->length - 1;
}

static uint32_t syscall_open(registers_t *regs) {
	// regs->ebx path
	char path[256];
	intptr_t r = copy_from_userspace(current_process->pdir, regs->ebx, 255, path);
	if (r < 0) {
		return -1;
	}
	// regs->ecx flags
	uint32_t flags = regs->ecx;
	// regs->edx mode
	uint32_t mode = regs->edx;

	(void)mode; (void)flags;

	fs_node_t *node = kopen(path, 0);
	if (node) {
		return process_append_fd(current_process, node);
	}

	return -1;
}

static uint32_t syscall_close(registers_t *regs) {
	uintptr_t fd_num = regs->ebx;

	if (current_process->fd_table == NULL) {
		return -1;
	}

	fd_table_t *fdt = current_process->fd_table;
	if (fd_num > fdt->length) {
		return -1;
	}
	if (fdt->entries[fd_num] != NULL) {
		fd_entry_t *entry = fdt->entries[fd_num];
		fs_close(entry->node);
		fdt->entries[fd_num] = NULL;
		kfree(entry);
		return 0;
	}
	return -1;
}

static uint32_t syscall_read(registers_t *regs) {
	uintptr_t fd_num = regs->ebx;
	uintptr_t ptr = regs->ecx;
	uintptr_t length = regs->edx;
//	printf("%s(fd: %u, buf: 0x%x, length: 0x%x)\n", __func__, fd_num, ptr, length);

	if (current_process->fd_table == NULL) {
		return -1;
	}

	fd_table_t *fdt = current_process->fd_table;
	if (fd_num > fdt->length) {
		return -1;
	}

	fd_entry_t *fd = fdt->entries[fd_num];
	if (fd != NULL) {
		if (length == 0) {
			// TODO: implement properly
//			return fs_read(fd->node, fd->seek, 0, NULL);
			return 0;
		}

		size_t n_blocks = (BLOCK_SIZE - 1 + length + (ptr & 0xfff)) / BLOCK_SIZE;
		page_directory_t *pdir = current_process->pdir;
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
	return -1;
}

static uint32_t syscall_write(registers_t *regs) {
	uint32_t fd_num = regs->ebx;
	uintptr_t ptr = regs->ecx;
	uintptr_t length = regs->edx;
//	printf("write(fd: %u, buf: 0x%x, length: 0x%x)\n", fd_num, ptr, length);

	if (current_process->fd_table == NULL) {
		return -1;
	}

	fd_table_t *fdt = current_process->fd_table;
	if (fd_num > fdt->length) {
		return -1;
	}

	fd_entry_t *fd = fdt->entries[fd_num];
	if (fd != NULL) {
		if (length == 0) {
//			return fs_read(fd->node, fd->seek, 0, NULL);
			return 0;
		}

		size_t n_blocks = (BLOCK_SIZE - 1 + length + (ptr & 0xfff)) / BLOCK_SIZE;
		assert(n_blocks != 0);
		page_directory_t *pdir = current_process->pdir;
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
		unmap_from_kernel(kptr, v);
		return r;
	}
	return -1;
}

static uint32_t syscall_mmap_anon(registers_t *regs) {
	// FIXME: validate len etc...
	uintptr_t addr = regs->ebx;
	size_t len = regs->ecx;
	// regs->edx prot
	printf("%s(addr: 0x%x, len: 0x%x, prot: %u)\n", __func__, addr, (uintptr_t)len, regs->edx);
	if (len > BLOCK_SIZE*200) {
		printf("length too big!\n");
		return -1;
	}

	len = len / BLOCK_SIZE;

	if (addr == 0) {
		addr = find_vspace(current_process->pdir, len);
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

	printf("addr: 0x%x prot: 0x%x\n", addr, prot);

	uintptr_t kptr = find_vspace(kernel_directory, 1);
	assert(kptr != 0);
	for (size_t i = 0; i < len; i++) {
		uintptr_t virtaddr = addr + i * BLOCK_SIZE;
		uintptr_t block = pmm_alloc_blocks_safe(1); // FIXME: this can panic (denial of service vulnerability)
		// TODO: free already mapped pages
		assert(get_page(get_table(virtaddr, current_process->pdir), virtaddr) == PAGE_VALUE_RESERVED);

		printf("u_virtaddr: 0x%x block: 0x%x\n", virtaddr, block);
		map_page(get_table(kptr, kernel_directory), kptr, block, PAGE_PRESENT | PAGE_READWRITE);
		memset((void *)kptr, 0, BLOCK_SIZE);
		map_page(get_table(kptr, kernel_directory), kptr, 0, 0);
		invalidate_page(kptr);
		map_page(get_table(virtaddr, current_process->pdir), virtaddr,
			block,
			PAGE_PRESENT | PAGE_READWRITE | PAGE_USER);
	}

	return addr;
}

static uint32_t syscall_munmap(registers_t *regs) {
	uintptr_t addr = regs->ebx;
	uintptr_t length = regs->ecx;
	printf("munmap(0x%x, 0x%x);\n", regs->ebx, regs->ecx);
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
		page_t p = get_page(get_table(i, current_process->pdir), i);
		if (p == 0) {
			continue;
		}

		if (p & PAGE_USER) {
			pmm_free_blocks(p & ~0xFFF, 1);
			map_page(get_table(i, current_process->pdir), i, 0, 0);
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

static uint32_t syscall_sleep(registers_t *regs) {
	unsigned long target = ticks + regs->ebx;
	while (ticks <= target) {
		switch_task();
	}
	return 0;
}

static uint32_t syscall_dumpregs(registers_t *regs) {
	dump_regs(regs);
	return 0;
}

static void syscall_handler(registers_t *regs) {
	switch (regs->eax) {
		case 0x01:
			syscall_exit(regs);
			break;
		case 0x02:
			regs->eax = syscall_sleep(regs);
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
		case 0x08:
			regs->eax = syscall_create(regs);
			break;
		case 0x0A:
			regs->eax = syscall_unlink(regs);
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
		case 0x2b:
			regs->eax =syscall_times(regs);
			break;
		case 0x4e:
			regs->eax = syscall_gettimeofday(regs);
			break;
		case 0xFF:
			regs->eax = syscall_dumpregs(regs);
			break;
		case 0x200:
			regs->eax = syscall_mmap_anon(regs);
			break;
		case 0x201:
			regs->eax = syscall_munmap(regs);
			break;
		default:
			regs->eax = (uint32_t)-1;
			break;
	}
}

void syscall_init() {
	isr_set_handler(0x80, syscall_handler);
}
