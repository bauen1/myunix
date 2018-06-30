// TODO: rewrite inline's as macro's to ensure they get inlined
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <console.h>
#include <pmm.h>
#include <string.h>

uint32_t *block_map;
uint32_t block_map_size; // number of block_map entries / 32
static uint32_t block_map_last;

/* mark block block as used */
inline void pmm_set_block(uintptr_t block) {
	assert(block < block_map_size);
	block_map[block / 32] |= (1 << (block % 32));
}

/* mark block as available  */
inline void pmm_unset_block(uintptr_t block) {
	assert(block < block_map_size);
	block_map[block / 32] &= ~(1 << (block % 32));
}

/* test if block is used  */
inline bool pmm_test_block(uintptr_t block) {
	assert(block < block_map_size);
	return (block_map[block / 32] & (1 << (block % 32)));
}

/* find the first free block */
inline uint32_t pmm_find_first_free() {
	for (uintptr_t i = 0; i < block_map_size / 32; i++) {
		if (block_map[i] == 0xFFFFFFFF) {
			// all blocks used, skip
			continue;
		}
		for (unsigned int j = 0; j < 32; j++) {
			uint32_t bit = (1 << j);
			if (block_map[i] & bit) {
				// skip
				continue;
			} else {
				return i * 32 + j;
			}
		}
	}

	return 0;
}

// TODO: optimise
inline uint32_t pmm_find_region(size_t size) {
	assert(size != 0);

	if (size == 1) {
		return pmm_find_first_free();
	}

	for (uintptr_t i = 0; i < block_map_size / 32; i++) {
		if (block_map[i] == 0xFFFFFFFF) {
			// all blocks used, skip
			continue;
		}
		for (unsigned int j = 0; j < 32; j++) {
			uint32_t bit = (1 << j);
			if (block_map[i] & bit) {
				// skip
				continue;
			} else {
				uint32_t start = i * 32 + j;
				size_t len = 1; // start = blocks[0]
				while (len < size) {
					if (pmm_test_block(start + len)) {
						// used
						break;
					} else {
						len++;
						assert((len + start) < block_map_size);
					}
				}
				if (len == size) {
					return start;
				} else {
					i = i + len / 32;
					j = j + len % 32;
				}
			}
			assert(0);
		}
	}

	return 0;
}

uintptr_t pmm_alloc_blocks(size_t size) {
	assert(size != 0);
	uint32_t block = pmm_find_region(size);
	if (block == 0) {
		return 0;
	}

	for (size_t i = 0; i < size; i++) {
		pmm_set_block(block + i);
	}

	return block * BLOCK_SIZE;
}

void pmm_free_blocks(uintptr_t p, size_t size) {
	assert(size != 0);

	uint32_t block = ((uint32_t)p) / BLOCK_SIZE;

	for (size_t i = 0; i < size; i++) {
		pmm_unset_block(block + i);
	}
}

uintptr_t pmm_alloc_blocks_safe(size_t size) {
#ifdef DEBUG
	printf("pmm_alloc_blocks_safe(size: %u)\n", size);
#endif
	uintptr_t v = pmm_alloc_blocks(size);
	if (v == 0) {
		printf("Out Of Memory!\n");
		printf("while pmm_alloc_blocks_size(size: %u); failed!!\n", size);
		assert(0);
	}

	return v;
}

uint32_t pmm_count_free_blocks() {
	uint32_t count = 0;
	for (uint32_t i = 0; i < block_map_size; i++) {
		if (! pmm_test_block(i)) {
			count++;
		}
	}

	return count;
}

void pmm_init(void *mem_map, size_t mem_size) {
	block_map_size = mem_size / BLOCK_SIZE;
	block_map = (uint32_t *)mem_map;
	block_map_last = 0;
	printf("pmm_init at 0x%x with size 0x%x\n", (uint32_t)block_map, (uint32_t)block_map_size);

	memset(block_map, 0xFF, block_map_size);
}
