#ifndef PMM_H
#define PMM_H 1

#include <stdbool.h>

#define BLOCK_SIZE 4096

void pmm_set_block(uintptr_t block_addr);
void pmm_unset_block(uintptr_t block_addr);
bool pmm_test_block(uintptr_t block_addr);

void pmm_init(void *mem_map, size_t mem_size);

uint32_t pmm_count_free_blocks();

#endif
