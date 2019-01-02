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
	block_map[block / 32] |= ((uint32_t)1 << (block % 32));
}

/* mark block as available  */
inline void pmm_unset_block(uintptr_t block) {
	assert(block < block_map_size);
	block_map[block / 32] &= ~((uint32_t)1 << (block % 32));
}

/* test if block is used  */
inline bool pmm_test_block(uintptr_t block) {
	assert(block < block_map_size);
	return (block_map[block / 32] & ((uint32_t)1 << (block % 32)));
}

/* find the first free block */
// FIXME: fix corner cases
static uint32_t pmm_find_first_free() {
	for (uintptr_t i = 0; i < block_map_size / 32; i++) {
		if (block_map[i] == 0xFFFFFFFF) {
			// all blocks used, skip
			continue;
		}

		for (unsigned int j = 0; j < 32; j++) {
			uint32_t bit = ((uint32_t)1 << j);

			if (!(block_map[i] & bit)) {
				return i * 32 + j;
			}
		}
	}

	return 0;
}

// TODO: optimise
// FIXME: fix corner cases
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

		// find the first free block inside this byte
		for (unsigned int j = 0; j < 32; j++) {
			uint32_t bit = (1 << j);
			if (block_map[i] & bit) {
				// skip
				continue;
			}

			uint32_t block_start = i * 32 + j;
			size_t block_length = 1;

			while (block_length < size) {
				const uint32_t block = block_start + block_length;
				assert(block < block_map_size);
				if (pmm_test_block(block)) {
					// used
					break;
				} else {
					block_length++;
				}
			}
			if (block_length == size) {
				return block_start;
			} else {
				i = i + block_length / 32;
				j = j + block_length % 32;
			}
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
	uintptr_t v = pmm_alloc_blocks(size);
	if (v == 0) {
		printf("%s(size: %u): OUT OF MEMORY!\n", __func__, (uintptr_t)size);
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
	printf("%s(mem_map: %p; mem_size: 0x%8x)\n", __func__, mem_map, (uintptr_t)mem_size);
	block_map_size = mem_size / BLOCK_SIZE;
	block_map = (uint32_t *)mem_map;
	block_map_last = 0;

	memset(block_map, 0xFF, block_map_size);
}
