#include <stdint.h>

#include <mmio.h>

void mmio_write32(volatile uintptr_t ptr, uint32_t value) {
	*((volatile uint32_t *)ptr) = value;
}

uint32_t mmio_read32(volatile uint32_t ptr) {
	return *(volatile uint32_t *)ptr;
}

void mmio_write16(volatile uintptr_t ptr, uint16_t value) {
	*((volatile uint16_t *)ptr) = value;
}

uint16_t mmio_read16(volatile uintptr_t ptr) {
	return *((volatile uint16_t *)ptr);
}

void mmio_write8(volatile uintptr_t ptr, uint8_t value) {
	*((volatile uint8_t *)ptr) = value;
}

uint8_t mmio_read8(volatile uintptr_t ptr) {
	return *((volatile uint8_t *)ptr);
}

