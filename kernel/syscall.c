#include <assert.h>
#include <console.h>
#include <cpu.h>
#include <isr.h>
#include <pit.h>
#include <pmm.h>
#include <process.h>
#include <string.h>
#include <vmm.h>

// 0 on success, size mapped on failure
// TODO: CRTICAL FIXME: returns 0 incase of early failure
static size_t map_userspace_to_kernel(uint32_t *pdir, uintptr_t ptr, uintptr_t kptr, size_t n) {
//	printf("map_userspace_to_kernel(0x%x, 0x%x, 0x%x, 0x%x)\n", (uintptr_t)pdir, ptr, kptr, n);
	for (uintptr_t i = 0; i < n; i++) {
		uintptr_t u_virtaddr = ptr + (i * BLOCK_SIZE);
		uintptr_t k_virtaddr = kptr + (i * BLOCK_SIZE);

		uint32_t *table = get_table(u_virtaddr, pdir);
		if (table == NULL) {
			printf(" table == NULL; returning early: %u\n", i);
			return i;
		}
		uintptr_t page = get_page(table, u_virtaddr);
		if (! (page && PAGE_TABLE_PRESENT)) {
			printf(" not present; returning early: %u\n", i);
			return i;
		}
		if (! (page && PAGE_TABLE_READWRITE)) {
			printf(" not read-write; returning early: %u\n", i);
			return i;
		}
		if (! (page && PAGE_TABLE_USER)) {
			printf(" not user; returning early: %u\n", i);
			return i;
		}
		map_page(get_table(k_virtaddr, kernel_directory), k_virtaddr, page, PAGE_TABLE_PRESENT);
	}
	return 0;
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

// only copies if all data was sucessfully mapped
__attribute__((used))
static size_t copy_from_userspace(uint32_t *pdir, uintptr_t ptr, size_t n, void *buffer) {
	size_t size_in_blocks = (BLOCK_SIZE - 1 + n) / BLOCK_SIZE;
	uintptr_t kptr = find_vspace(kernel_directory, size_in_blocks);
	if (kptr == 0) {
		return -1;
	}
	size_t v = map_userspace_to_kernel(pdir, ptr, kptr, size_in_blocks);
	if (v != 0) {
		unmap_from_kernel(kptr, v);
		return -1;
	}

	uintptr_t kptr2 = kptr + (ptr & 0xFFF);

	memcpy(buffer, (void *)kptr2, n);

	unmap_from_kernel(kptr, v);
	return n;
}

__attribute__((used))
static size_t copy_to_userspace(uint32_t *pdir, uintptr_t ptr, size_t n, void *buffer) {
	// TODO: implement
}

/*
static void syscall_execve(registers_t *regs) {
//	printf("execve()\n");
	regs->eax = -1;
}

static void syscall_fork(registers_t *regs) {
//	printf("fork()\n");
	regs->eax = -1;
}

static void syscall_fstat(registers_t *regs) {
//	printf("fstat()\n");
	regs->eax = 0;
}

static void syscall_stat(registers_t *regs) {
//	printf("stat()\n");
	regs->eax = 0;
}

static void syscall_link(registers_t *regs) {
//	printf("link()\n");
	regs->eax = -1;
}

static void syscall_lseek(registers_t *regs) {
//	printf("lseek()\n");
	regs->eax = 0;
}
*/
static void syscall_times(registers_t *regs) {
//	printf("times()\n");
	regs->eax = -1;
}

static void syscall_mkdir(registers_t *regs) {
	// regs->ebx 256 path
	// regs->ecx mode
	char buffer[256];
	size_t r = copy_from_userspace(current_process->pdir, regs->ebx, 256, buffer); // FIXME: possible off by one error ?
	if (r < 0) {
		regs->eax = -1;
		return;
	}

	// FIXME: creating directories not in the root would be cool
	fs_mkdir(fs_root, buffer, regs->ecx);
	regs->eax = 0;
}

static void syscall_rmdir(registers_t *regs) {
	// regs->ebx path
	// just call syscall_unlink ?
	regs->eax = -1;
}

static void syscall_create(registers_t *regs) {
	// regs->ebx path
	// regs->ecx mode
	char buffer[256];
	size_t r = copy_from_userspace(current_process->pdir, regs->ebx, 256, buffer); // FIXME ^
	if (r < 0) {
		regs->eax = -1;
		return;
	}

	// FIXME: creating directories not in the root would be cool
	fs_create(fs_root, buffer, regs->ecx);
	regs->eax = 0;
	return;
}

static void syscall_unlink(registers_t *regs) {
	// regs->ebx path
	char buffer[256];
	size_t r = copy_from_userspace(current_process->pdir, regs->ebx, 256, buffer); // FIXME: ^
	if (r < 0) {
		regs->eax = -1;
		return;
	}

	// FIXME: creating directories not in the root would be cool
	fs_unlink(fs_root, buffer);
	regs->eax = 0;
	return;
}

/*
static void syscall_wait(registers_t *regs) {
//	printf("wait()\n");
	regs->eax = -1;
}
*/

static void syscall_gettimeofday(registers_t *regs) {
//	printf("gettimeofday()\n");
	regs->eax = -1;
}

static void syscall_getpid(registers_t *regs) {
//	printf("getpid()\n");
	regs->eax = (uint32_t)current_process->pid;
}

static void syscall_exit(registers_t *regs) {
	(void)regs;
	printf("pid %u exit(%u)\n", current_process->pid, regs->ebx);
	while (1) {
		switch_task();
	}
}

static void syscall_open(registers_t *regs) {
//	printf("open()\n");
	regs->eax = -1;
}

static void syscall_close(registers_t *regs) {
//	printf("close()\n");
	regs->eax = -1;
}

static void syscall_read(registers_t *regs) {
//	printf("read(%u, 0x%x, 0x%x) (eip = 0x%x)\n", regs->ebx, regs->ecx, regs->edx, regs->eip);
	// regs->ebx int fd
	// regs->ecx char *buf
	// regs->edx int len
	if (current_process->fd_table) {
		if (regs->ebx > 16) {
			regs->eax = -1;
			return;
		}
		if (current_process->fd_table->entries[regs->ebx]) {
			uintptr_t ptr = regs->ecx;
			size_t n = regs->edx;
			size_t n_blocks = (BLOCK_SIZE - 1 + n) / BLOCK_SIZE;
			uint32_t *pdir = current_process->pdir;
			uintptr_t kptr = find_vspace(kernel_directory, n_blocks); // FIXME
//			uintptr_t kptr2 = kptr + (ptr & 0xFFFF);
			uintptr_t kptr2 = kptr + (ptr & 0xFFF);
			size_t v = map_userspace_to_kernel(pdir, ptr, kptr, n_blocks);
			if (v != 0) {
				printf("v: %u\n", v);
				unmap_from_kernel(kptr, v);
				regs->eax = -1;
				return;
			}
			fs_node_t *node = current_process->fd_table->entries[regs->ebx];
			regs->eax = fs_read(node, 0, n, (uint8_t *)kptr2);
			unmap_from_kernel(kptr, n_blocks);
		}
	} else {
		regs->eax = -1;
		return;
	}
}

static void syscall_write(registers_t *regs) {
//	printf("write(%u, 0x%x, 0x%x) (eip = 0x%x)\n", regs->ebx, regs->ecx, regs->edx, regs->eip);
	// regs->ebx int fd
	// regs->ecx char *buf
	// regs->edx int len
	if (current_process->fd_table) {
		if (regs->ebx > 16) {
			regs->eax = -1;
			return;
		}
		if (current_process->fd_table->entries[regs->ebx]) {
			// FIXME: handle oveflow of pointer into next block correctly
			uintptr_t ptr = regs->ecx;
			size_t n = regs->edx;
			size_t n_blocks = (BLOCK_SIZE - 1 + n) / BLOCK_SIZE;
			uint32_t *pdir = current_process->pdir;
			uintptr_t kptr = find_vspace(kernel_directory, n_blocks); // FIXME
			uintptr_t kptr2 = kptr + (ptr & 0xFFF);
			size_t v = map_userspace_to_kernel(pdir, ptr, kptr, n_blocks);
			if (v != 0) {
				printf("syscall_write early abort, v: %u\n", v);
				unmap_from_kernel(kptr, v);
				regs->eax = -1;
				return;
			}
			fs_node_t *node = current_process->fd_table->entries[regs->ebx];
			regs->eax = fs_write(node, 0, n, (uint8_t *)kptr2);
			unmap_from_kernel(kptr, n_blocks);
		}
	} else {
		regs->eax = -1;
		return;
	}
}

static void syscall_sleep(registers_t *regs) {
	unsigned long target = ticks + regs->ebx;
	while (ticks <= target) {
		switch_task();
	}
	regs->eax = 0;
}

static void syscall_dumpregs(registers_t *regs) {
	dump_regs(regs);
}

static void syscall_handler(registers_t *regs) {
	switch (regs->eax) {
		case 0x01:
			syscall_exit(regs);
			break;
		case 0x02:
			syscall_sleep(regs);
			break;
		case 0x03:
			syscall_read(regs);
			break;
		case 0x04:
			syscall_write(regs);
			break;
		case 0x05:
			syscall_open(regs);
			break;
		case 0x06:
			syscall_close(regs);
			break;
		case 0x08:
			syscall_create(regs);
			break;
		case 0x0A:
			syscall_unlink(regs);
			break;
		case 0x14:
			syscall_getpid(regs);
			break;
		case 0x27:
			syscall_mkdir(regs);
			break;
		case 0x28:
			syscall_rmdir(regs);
			break;
		case 0x2b:
			syscall_times(regs);
			break;
		case 0x4e:
			syscall_gettimeofday(regs);
			break;
		case 0xFF:
			syscall_dumpregs(regs);
			break;
		default:
			break;
	}
}

void syscall_init() {
	isr_set_handler(0x80, syscall_handler);
}
