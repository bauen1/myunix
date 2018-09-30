// TODO: ensure interrupts are off
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <boot.h>
#include <console.h>
#include <cpu.h>
#include <isr.h>
#include <paging.h>
#include <process.h>
#include <pmm.h>
#include <string.h>
#include <vmm.h>
#include <heap.h>

page_directory_t *kernel_directory;

page_directory_t *page_directory_reference(page_directory_t *pdir) {
	assert(pdir != NULL);
	if (pdir->__refcount != -1) {
		pdir->__refcount++;
	}
	return pdir;
}

page_directory_t *page_directory_new() {
	page_directory_t *pdir = kcalloc(1, sizeof(page_directory_t));
	assert(pdir != NULL);
	pdir->physical_address = pmm_alloc_blocks_safe(1);
	pdir->physical_tables = (uintptr_t *)find_vspace(kernel_directory, 1);
	map_page(get_table_alloc((uintptr_t)pdir->physical_tables, kernel_directory), (uintptr_t)pdir->physical_tables,
		pdir->physical_address,
		PAGE_PRESENT | PAGE_READWRITE);
	memset((void *)pdir->physical_tables, 0, BLOCK_SIZE);
	return page_directory_reference(pdir);
}

void page_directory_free(page_directory_t *pdir) {
	assert(pdir != NULL);
	if (pdir->__refcount == -1) {
		// technically we're not supposed to do anything, but this is most likely a bug
		assert(0 && "tried to free immortal page_directory (most likely kernel directory)");
		return;
	}

	pdir->__refcount--;
	if (pdir->__refcount == 0) {
		// XXX: walk the page directory and ensure there are no mappings left, freeing page tables in the process
		// TODO: maybe this could be split into page_table_new() / page_table_free()
		for (uintptr_t i = 0; i < 1024; i++) {
			uintptr_t phys_table = pdir->physical_tables[i];
			if (phys_table & PAGE_PRESENT) {
				page_table_t *table = pdir->tables[i];
				assert(table != NULL);
				for (uintptr_t j = 0; j < 1024; j++) {
					page_t page = table->pages[j];
					assert(page == 0);
				}

				uintptr_t phys = phys_table & ~0x3FF;
				uintptr_t virtaddr = (uintptr_t)table;
				map_page(get_table(virtaddr, kernel_directory), virtaddr, 0, 0);
				invalidate_page(virtaddr);
				pmm_free_blocks(phys, 1);
				pdir->physical_tables[i] = 0;
				pdir->tables[i] = NULL;
			} else {
				assert(pdir->tables[i] == NULL);
			}
		}
		pmm_free_blocks(pdir->physical_address, 1);
		uintptr_t virtaddr = (uintptr_t)pdir->physical_tables;
		map_page(get_table(virtaddr, kernel_directory), virtaddr, 0, 0);
		invalidate_page(virtaddr);
		kfree(pdir);
	}
}

void invalidate_page(uintptr_t virtaddr) {
	assert((virtaddr & 0x3FF) == 0);
	__asm__ __volatile__("invlpg (%0)" : : "b" (virtaddr) : "memory");
}

page_table_t *get_table(uintptr_t virtaddr, page_directory_t *directory) {
	assert(directory != NULL);
	uintptr_t i = virtaddr >> 22;
	assert(i < 1024);
	return directory->tables[i];
}

page_table_t *get_table_alloc(uintptr_t virtaddr, page_directory_t *directory) {
	assert(directory != NULL);
	page_table_t *table = get_table(virtaddr, directory);
	if (table == NULL) {
		/* there are too many corner cases this would break */
		assert(directory != kernel_directory);

		uintptr_t index = (virtaddr >> 22) & 0x3FF;
		uintptr_t phys = pmm_alloc_blocks_safe(1);
		uintptr_t virt = find_vspace(kernel_directory, 1);
		assert(virt != 0);

		directory->physical_tables[index] = phys |
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE | PAGE_TABLE_USER;
		directory->tables[index] = (page_table_t *)virt;
		// XXX: depends on all kernel tables being pre-allocated
		map_page(get_table(virt, kernel_directory), virt, phys,
			PAGE_PRESENT | PAGE_READWRITE);
		invalidate_page(virtaddr);
		table = directory->tables[index];
	}
	return table;
}

page_t get_page(page_table_t *table, uintptr_t virtaddr) {
	assert(table != NULL);
	uintptr_t i = virtaddr >> 12 & 0x3FF;
	return table->pages[i];
}

void map_page(page_table_t *table, uintptr_t virtaddr, uintptr_t physaddr, enum page_flags flags) {
	assert(table != NULL);
	assert((virtaddr & 0x3FF) == 0);
	assert(((uintptr_t)table & 0x3FF) == 0);

	uintptr_t index = (virtaddr >> 12) & 0x3FF;
	assert(index < 1024);
	table->pages[index] = (page_t)((uint32_t)physaddr | flags);
	// TODO: add a method to map a page in the kernel directory and call invlpg there
}

/* helper function */
// directly maps from (including) start to end to kernel space
void map_pages(uintptr_t start, uintptr_t end, enum page_flags flags, const char *name) {
	if ((start & 0x3FF) != 0) {
		printf("%s: WARN: start %p misaligned, adjusting to %p!\n", __func__, start, (start & ~0x3FF));
		start = start & ~0x3FF;
	}
	if ((end & 0x3FF) != 0) {
		printf("%s: WARN: end %p misaligned, adjusting to %p!\n", __func__, end, ((end & ~0x3FF) + BLOCK_SIZE));
		end = (end & ~0x3FF) + BLOCK_SIZE;
	}

	if (name != NULL) {
		printf("%s: 0x%x - 0x%x => 0x%x - 0x%x flags: 0x%x\n", name, (uintptr_t)start, (uintptr_t)end, (uintptr_t)start, (uintptr_t)end, flags);
	}
	for (uintptr_t i = start; i < end; i += BLOCK_SIZE) {
		map_page(get_table(i, kernel_directory), i, i, flags);
	}
}

// XXX: don't use this unless you absolutely have to (it may break, only works good in early boot)
void map_direct_kernel(uintptr_t v) {
	if ((v & 0x3FF) != 0) {
		printf("WARN: map_direct_kernel pointer not aligned! (0x%x)\n", v);
		assert(0);
	}
	page_table_t *table = get_table(v, kernel_directory);
	page_t o_page = get_page(table, v);
	if (o_page != 0) {
		if ((o_page & ~0x3FF) == v) {
			if (o_page == (v | PAGE_PRESENT | PAGE_READWRITE)) {
				printf(" mapped using same value 0x%x => 0x%x\n", v, v);
			} else {
				printf("already mapped ?!! v: 0x%x page: 0x%x\n", v, o_page);
			}
		} else {
			printf("we're in deep trouble!\n");
			printf("v: 0x%x page: 0x%x\n", v, get_page(table, v));
			assert(0);
		}
	}

	map_page(get_table(v, kernel_directory), v, v, PAGE_PRESENT | PAGE_READWRITE);
	invalidate_page(v);
}

// TODO: optimise (it's too slow)
// TODO: mark found pages with PAGE_VALUE_RESERVED
inline uintptr_t vmm_find_dma_region(size_t size) {
	assert(size != 0);

	for (uint32_t i = 0; i < block_map_size / 32; i++) {
		if (block_map[i] == 0xFFFFFFFF) {
			// skip
			continue;
		}

		// check every bit in block_map[i]
		for (uint8_t j = 0; j < 32; j++) {
			uint32_t start = i * 32 + j;
			uint32_t len = 0;

			while (len < size) {
				uintptr_t v_addr = (start + len) * BLOCK_SIZE;
#ifdef DEBUG
				printf("  start: 0x%x (len: 0x%x)\n", start, len);
#endif
				if (pmm_test_block(start + len)) {
					// block used
					break;
				}
				if (get_page(get_table(v_addr, kernel_directory), v_addr) != 0) {
					// block mapped
					break;
				} else {
					len++;
				}
			}

			if (len == size) {
				return start;
			} else {
				i = i + len / 32;
				j = j + len;
			}
		}
	}

	printf("CRITICAL: NO DMA REGION OF SIZE %u FOUND!!\n", (uintptr_t)size);
	printf("pmm_count_free_blocks(): 0x%x\n", pmm_count_free_blocks());
	return 0;
}

// XXX: don't use this, it won't break this, but it may break
void *dma_malloc(size_t m) {
	assert(m != 0);
	size_t n = (BLOCK_SIZE - 1 + m) / BLOCK_SIZE;
	uintptr_t v = vmm_find_dma_region(n);
	assert(v != 0);
	for (size_t i = 0; i < n; i++) {
		pmm_set_block(v + i);
		map_direct_kernel((v + i) * BLOCK_SIZE);
		memset((void *)((v + i) * BLOCK_SIZE), 0, BLOCK_SIZE);
	}

	return (void *)(v * BLOCK_SIZE);
}

// TODO: optimise the next 2 functions by walking in page table increments
// finds free (continuous) virtual address space and maps it to PAGE_VALUE_RESERVED
// n in blocks
// FIXME: allocates tables for ranges that will be too small
// returns 0 in case of failure
uintptr_t find_vspace(page_directory_t *dir, size_t n) {
	assert(dir != NULL);
	assert(n != 0);
	// XXX: skip special block 0
	for (uintptr_t i = 1; i < (0x100000000 / BLOCK_SIZE); i++) {
		uintptr_t v_addr = i * BLOCK_SIZE;
		/**
		XXX: this depends on the fact that all kernel tables are pre-allocated
		so we can call get_table_alloc without modifiying the kernel directory
		*/
		page_table_t *table = get_table_alloc(v_addr, dir);
		if (get_page(table, v_addr) != 0) {
			// page mapped, skip
			continue;
		}

		uintptr_t start = v_addr;
		uintptr_t length = 1;
		while (length < n) {
			uintptr_t v_addr2 = v_addr + length*BLOCK_SIZE;
			table = get_table_alloc(v_addr2, dir);
			if (get_page(table, v_addr2) == 0) {
				length++;
			} else {
				break;
			}
		}
		if (length == n) {
			for (uintptr_t i = start;
				i < start + (length*BLOCK_SIZE);
				i += BLOCK_SIZE) {
				/*
				XXX: depends on get_table not returning NULL after a call to get_table_alloc
				*/
				map_page(get_table(i, dir), i, PAGE_VALUE_RESERVED, 0);
			}
#ifdef DEBUG
			printf(" = 0x%x\n", start);
#endif
			return start;
		} else {
			i = i + length;
		}
	}

	return -1;
}

static void dump_table(page_table_t *table, uintptr_t table_addr, char *prefix) {
	assert(table != NULL);
	assert(prefix != NULL);
	assert((table_addr & 0x3FF) == 0);

	for (uintptr_t page_i = 0; page_i < 1024; page_i++) {
		page_t page = table->pages[page_i];
		if (page != 0) {
			uintptr_t v_addr = (table_addr) | (page_i << 12);

			printf("%s0x%8x => 0x%8x ", prefix, v_addr, page);
			if (page & PAGE_USER) {
				printf("u");
			} else {
				printf("k");
			}
			if (page & PAGE_READWRITE) {
				printf("w");
			} else {
				printf("r");
			}
			if (page & PAGE_PRESENT) {
				printf("p");
			} else {
				printf("-");
			}
			printf("\n");
		}
	}
}

void dump_directory(page_directory_t *directory) {
	printf("%s(directory: %p)\n", __func__, directory);
	assert(directory != NULL);
	printf("--- directory (virt %p, phys: %p) ---\n", (uintptr_t)directory, directory->physical_address);
	for (uintptr_t i = 0; i < 1024; i++) {
		uintptr_t phys_table = directory->physical_tables[i];
		if (phys_table == 0) {
			assert(directory->tables[i] == NULL);
			continue;
		}

		page_table_t *table = directory->tables[i];
		printf("0x%8x table: (virt %p, phys: %p) (", i << 22, (uintptr_t)table, phys_table);
		if (phys_table & PAGE_TABLE_PRESENT) {
			printf("present)\n");
			dump_table(table, i << 22, "  ");
		} else {
			printf(")\n");
		}
	}
}

static void page_fault(registers_t *regs) {
	uintptr_t address;
	__asm__ __volatile__("mov %%cr2, %0" : "=r"(address));
	printf("! page_fault !\n");
	const char *action;
	const char *cause;
	const char *mode;

	if (regs->err_code & 0x1) {
		cause = "protection violation";
	} else {
		cause = "non-present page";
	}

	if (regs->err_code & 0x2) {
		action = "write";
	} else {
		action = "read";
	}

	if (regs->err_code & 0x4) {
		mode = "user";
	} else {
		mode = "kernel";
	}

	if (regs->err_code & 0x8) {
		cause = "page directory entry reserved bit set";
	}

	if (regs->err_code & 0x10) {
		action = "instruction fetch";
	}

	printf("[vmm] page_fault in %s mode at eip=%p during %s caused by %s of address %p (exception code: 0x%8x)\n", mode, (uintptr_t)regs->eip, action, cause, address, regs->err_code);

	page_directory_t *pdir;
	if (regs->err_code & 0x4) {
		assert(current_process != NULL);
		pdir = current_process->task.pdir;
	} else {
		pdir = kernel_directory;
	}
	assert(pdir != NULL);
	// TODO: consider table flags
	page_table_t *table = get_table(address, pdir);
	if (table == NULL) {
		printf("0x%8x table = NULL!\n", address);
	} else {
		printf("0x%8x table: %p\n", address, table);
		page_t page = get_page(table, address);
		printf("0x%8x => 0x%8x ", address, page);
		if (page & PAGE_USER) {
			printf("u");
		} else {
			printf("k");
		}
		if (page & PAGE_READWRITE) {
			printf("w");
		} else {
			printf("r");
		}
		if (page & PAGE_PRESENT) {
			printf("p");
		} else {
			printf("-");
		}
		printf("\n");
	}

	dump_regs(regs);

	if (regs->err_code & 0x4) {
		printf("usermode\n");
		printf("current_process: 0x%x\n", (uintptr_t)current_process);
		printf("current_process->kstack: 0x%x\n", current_process->kstack);
		printf("current_process->kstack_size: 0x%x\n", (uintptr_t)current_process->kstack_size);
		printf("current_process->task.registers: 0x%x\n", (uintptr_t)current_process->task.registers);
		printf("current_process->task.esp: 0x%x\n", current_process->task.esp);
		printf("current_process->task.ebp: 0x%x\n", current_process->task.ebp);
		printf("current_process->task.eip: 0x%x\n", current_process->task.eip);
		printf("current_process->task.pdir: 0x%x\n", (uintptr_t)current_process->task.pdir);
		printf("current_process->name: '%s'\n", current_process->name);
		printf("current_process->pid: 0x%x\n", current_process->pid);
		printf("current_process->fd_table: 0x%x\n", (uintptr_t)current_process->fd_table);
		printf("\n");
		dump_directory(current_process->task.pdir);
	} else {
		printf("KERNEL MODE PAGE FAULT\n");
		print_stack_trace(200);
		dump_directory(kernel_directory);
		halt();
	}
	halt();
}

page_directory_t _kernel_dir;

void vmm_init() {
	// XXX: a lot of code depends on all the kernel tables being pre-allocated to avoid calling get_table_alloc (which could result in an infinite loop)
	isr_set_handler(14, page_fault);

	// TODO: allocate the kernel_directory
	_kernel_dir.physical_tables = (uintptr_t *)pmm_alloc_blocks_safe(1);
	printf("physical tables: %p\n", _kernel_dir.physical_tables);
	_kernel_dir.physical_address = (uintptr_t)_kernel_dir.physical_tables;
	memset(_kernel_dir.physical_tables, 0, sizeof(uintptr_t) * 1024);
	memset(_kernel_dir.tables, 0, sizeof(page_table_t *) * 1024);
	_kernel_dir.__refcount = -1;

	kernel_directory = &_kernel_dir;

	printf("real kernel directory: %p\n", kernel_directory->physical_address);

	for (unsigned int i = 0; i < 1024; i++) {
		uintptr_t physical_t = pmm_alloc_blocks_safe(1);
		memset((void *)physical_t, 0, BLOCK_SIZE);
		// XXX: Don't map page tables with USER bit set, since no usercode should ever need to use the kernel directory
		kernel_directory->physical_tables[i] = (uintptr_t)(physical_t |
			PAGE_TABLE_PRESENT | PAGE_TABLE_READWRITE |
			PAGE_TABLE_WRITE_THROUGH | PAGE_TABLE_CACHE_DISABLE);
		kernel_directory->tables[i] = (page_table_t *)physical_t;
	}

	for (unsigned int i = 0; i < 1024; i++) {
		// directly map the page tables
		map_direct_kernel((uintptr_t)kernel_directory->tables[i]);
	}

	map_direct_kernel(kernel_directory->physical_address);

	// catch NULL pointer dereferences
	map_page(get_table(0, kernel_directory), 0, 0, 0);
}

void vmm_enable() {
	load_page_directory(kernel_directory->physical_address);
	enable_paging();
}
