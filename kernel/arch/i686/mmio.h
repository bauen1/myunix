#ifndef ARCH_MMIO_H
#define ARCH_MMIO_H 1

#include <stdint.h>

void mmio_write32(volatile uintptr_t ptr, uint32_t value);
uint32_t mmio_read32(volatile uint32_t ptr);
void mmio_write16(volatile uintptr_t ptr, uint16_t value);
uint16_t mmio_read16(volatile uintptr_t ptr);
void mmio_write8(volatile uintptr_t ptr, uint8_t value);
uint8_t mmio_read8(volatile uintptr_t ptr);

#endif
