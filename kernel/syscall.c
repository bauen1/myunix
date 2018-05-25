// FIXME: use invlpg
// FIXME: handle overflow into next page in copy_{from,to}_userspace
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

// 0 on success, size mapped on failure
// TODO: CRITICAL FIXME: returns 0 incase of early failure
static intptr_t map_userspace_to_kernel(page_directory_t *pdir, uintptr_t ptr, uintptr_t kptr, size_t n) {
//	printf("map_userspace_to_kernel(0x%x, 0x%x, 0x%x, 0x%x)\n", (uintptr_t)pdir, ptr, kptr, n);
	for (uintptr_t i = 0; i < n; i++) {
		uintptr_t u_virtaddr = ptr + (i * BLOCK_SIZE);
		uintptr_t k_virtaddr = kptr + (i * BLOCK_SIZE);

		page_table_t *table = get_table(u_virtaddr, pdir);
		if (table == NULL) {
			printf(" table == NULL; returning early: %u\n", i);
			return i;
		}
		uintptr_t page = get_page(table, u_virtaddr);
		if (! (page & PAGE_TABLE_PRESENT)) {
			printf(" not present; returning early: %u\n", i);
			return i;
		}
		if (! (page & PAGE_TABLE_READWRITE)) {
			printf(" not read-write; returning early: %u\n", i);
			return i;
		}
		if (! (page & PAGE_TABLE_USER)) {
			printf(" not user; returning early: %u\n", i);
			return i;
		}
		map_page(get_table(k_virtaddr, kernel_directory), k_virtaddr, page, PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
		__asm__ __volatile__ ("invlpg (%0)" : : "b" (k_virtaddr) : "memory");
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

// only copies if all data was successfully mapped
static intptr_t copy_from_userspace(page_directory_t *pdir, uintptr_t ptr, size_t n, void *buffer) {
	size_t size_in_blocks = (BLOCK_SIZE - 1 + n + (ptr & 0xfff)) / BLOCK_SIZE;
	uintptr_t kptr = find_vspace(kernel_directory, size_in_blocks);
	if (kptr == 0) {
		return -1;
	}
	intptr_t v = map_userspace_to_kernel(pdir, ptr, kptr, size_in_blocks);
	if (v != 0) {
		unmap_from_kernel(kptr, v);
		return -1;
	}

	uintptr_t kptr2 = kptr + (ptr & 0xFFF);

	memcpy(buffer, (void *)kptr2, n);

	unmap_from_kernel(kptr, v);
	return n;
}

static intptr_t copy_to_userspace(page_directory_t *pdir, uintptr_t ptr, size_t n, void *buffer) {
	size_t size_in_blocks = (BLOCK_SIZE - 1 + n + (ptr & 0xfff)) / BLOCK_SIZE;
	uintptr_t kptr = find_vspace(kernel_directory, size_in_blocks);
	if (kptr == 0) {
		printf("kptr == 0\n");
		return -1;
	}
	intptr_t v = map_userspace_to_kernel(pdir, ptr, kptr, size_in_blocks);
	printf("v: %u\n", v);
	if (v != 0) {
		unmap_from_kernel(kptr, v);
		return -1;
	}

	uintptr_t kptr2 = kptr + (ptr & 0xFFF);

	printf("memcpy(dest: 0x%x, src: 0x%x, len: 0x%x);\n", kptr2, buffer, n);
	memcpy((void *)kptr2, buffer, n);

	unmap_from_kernel(kptr, v);
	return n;
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
	intptr_t r = copy_from_userspace(current_process->pdir, regs->ebx, 255, buffer);
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
	intptr_t r = copy_from_userspace(current_process->pdir, regs->ebx, 255, buffer);
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
	intptr_t r = copy_from_userspace(current_process->pdir, regs->ebx, 255, buffer);
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

static void __attribute__((noreturn)) syscall_exit(registers_t *regs) {
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
			page_directory_t *pdir = current_process->pdir;
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
			page_directory_t *pdir = current_process->pdir;
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

static void syscall_mmap_anon(registers_t *regs) {
	// FIXME: validate len etc...
	uintptr_t addr = regs->ebx;
	size_t len = regs->ecx;
	// regs->edx prot
	printf("mmap_anon(addr: 0x%x, len: 0x%x, prot: %u)\n", addr, len, regs->edx);
	if (len > BLOCK_SIZE*200) {
		regs->eax = -1;
		return;
	}

	len = len / BLOCK_SIZE;

	if (addr == 0) {
		addr = find_vspace(current_process->pdir, len);
	}

	uint32_t prot;
	switch (regs->edx) {
		case (1):
			prot = PAGE_TABLE_PRESENT | PAGE_TABLE_USER;
			break;;
		case (3):
			prot = PAGE_TABLE_PRESENT | PAGE_TABLE_USER | PAGE_TABLE_READWRITE;
			break;;
		default:
		case (0):
			prot = PAGE_TABLE_PRESENT | PAGE_TABLE_USER;
			break;;
	}

	printf("addr: 0x%x prot: 0x%x\n", addr, prot);

	uintptr_t kptr = find_vspace(kernel_directory, 1);
	assert(kptr != 0);
	for (size_t i = 0; i < len; i++) {
		uintptr_t virtaddr = addr + i * BLOCK_SIZE;
		uintptr_t block = pmm_alloc_blocks_safe(1); // FIXME: this can panic
		// FIXME: free already mapped pages
		assert(get_page(get_table(virtaddr, current_process->pdir), virtaddr) == PAGE_VALUE_RESERVED);

		printf("u_virtaddr: 0x%x block: 0x%x\n", virtaddr, block);
		map_page(get_table(kptr, kernel_directory), kptr, block, PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE);
		memset((void *)kptr, 0, BLOCK_SIZE);
		map_page(get_table(kptr, kernel_directory), kptr, 0, 0);
		__asm__ __volatile__ ("invlpg (%0)" : : "b" (kptr) : "memory");
		map_page(get_table(virtaddr, current_process->pdir), virtaddr,
			block,
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE | PAGE_TABLE_USER);
	}

	regs->eax = addr;
}

static void syscall_munmap(registers_t *regs) {
	// regs->ebx addr
	// regs->ecx length
	printf("munmap(0x%x, 0x%x);\n", regs->ebx, regs->ecx);
	if (((regs->ebx & 0xFFF) != 0) || (regs->ecx == 0)) {
		regs->eax = -1;
	}

	uintptr_t len = regs->ecx;
	if ((len & 0xFFF) > 0) {
		len = (len & 0xFFF) + 0x1000;
	}

	for (uintptr_t i = 0; i < len; i += BLOCK_SIZE) {
		page_t p = get_page(get_table(i, current_process->pdir), i);
		if (p == 0) {
			continue;
		}

		if (p & PAGE_TABLE_USER) {
			pmm_free_blocks(p & ~0xFFF, 1);
			map_page(get_table(i, current_process->pdir), i, 0, 0);
			continue;
		}

		assert(0);
		regs->eax = -1;
		return;
	}
	regs->eax = 0;
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
		case 0x200:
			syscall_mmap_anon(regs);
			break;
		case 0x201:
			syscall_munmap(regs);
			break;
		default:
			break;
	}
}

void syscall_init() {
	isr_set_handler(0x80, syscall_handler);
}
