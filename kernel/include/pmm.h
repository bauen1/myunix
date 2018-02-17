#ifndef PMM_H
#define PMM_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BLOCK_SIZE 4096

extern uint32_t *block_map;
extern uint32_t block_map_size;

void pmm_set_block(uintptr_t block);
void pmm_unset_block(uintptr_t block);
bool pmm_test_block(uintptr_t block);

void pmm_init(void *mem_map, size_t mem_size);

uint32_t pmm_count_free_blocks();

uint32_t pmm_find_first_free();
uint32_t pmm_find_first_free_region(size_t size);
void *pmm_alloc_blocks(size_t size);
void pmm_free_blocks(void *p, size_t size);

#endif
