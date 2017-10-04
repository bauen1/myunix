#include <stdint.h>
#include <cpu.h>

inline uint8_t inb(uint16_t port) {
	uint8_t data;
	__asm__ __volatile__ ("inb %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline uint16_t inw(uint16_t port) {
	uint16_t data;
	__asm__ __volatile__ ("inw %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline uint32_t inl(uint16_t port) {
	uint32_t data;
	__asm__ __volatile__ ("inl %1, %0" : "=a" (data) : "dN" (port));
	return data;
}

inline void outb(uint16_t port, uint8_t data) {
	__asm__ __volatile("outb %1, %0" : : "dN" (port), "a" (data));
}

inline void outw(uint16_t port, uint16_t data) {
	__asm__ __volatile("outw %1, %0" : : "dN" (port), "a" (data));
}

inline void outl(uint16_t port, uint32_t data) {
	__asm__ __volatile("outl %1, %0" : : "dN" (port), "a" (data));
}


inline void io_wait() {
	__asm__ __volatile__("outb %0, $0x80" : : "a"(0));
}
