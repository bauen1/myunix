#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pmm.h>
#include <console.h>
#include <string.h>

static uint32_t *block_map;
static uint32_t block_map_size;

void pmm_set_block(uintptr_t block_addr) {
	uint32_t block = block_addr / BLOCK_SIZE;
	block_map[block / 32] |= (1 << (block % 32));
}

void pmm_unset_block(uintptr_t block_addr) {
	uint32_t block = block_addr / BLOCK_SIZE;
	block_map[block / 32] &= ~(1 << (block % 32));
}

bool pmm_test_block(uintptr_t block_addr) {
	uint32_t block = block_addr / BLOCK_SIZE;
	return (block_map[block / 32] & (1 << (block % 32)));
}

uint32_t pmm_count_free_blocks() {
	uint32_t count = 0;
	for (uint32_t i = 0; i <= block_map_size; i++) {
		if (! pmm_test_block(i * 0x1000)) {
			count++;
		}
	}
	return count;
}

void pmm_init(void *mem_map, size_t mem_size) {
	block_map_size = mem_size / BLOCK_SIZE;
	block_map = (uint32_t *)mem_map;
	printf("pmm_init at 0x%x with size 0x%x\n", (uint32_t)block_map, (uint32_t)block_map_size);

	memset(block_map, 0xFF, block_map_size);
}
