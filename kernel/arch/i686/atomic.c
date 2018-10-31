#include <atomic.h>
#include <task.h>

void spin_init(spin_t lock) {
	lock[0] = 0;
}

static inline int arch_atomic_swap(volatile int *location, int value) {
	__asm__ __volatile__ ("xchg %0, %1" : "=r"(value), "=m"(*location) : "0"(value) : "memory");
	return value;
}

void spin_lock(spin_t lock) {
	scheduler_lock();

	while ( arch_atomic_swap(lock, 1) != 0) {
		__asm__ __volatile__ ("cli\nhlt"); // XXX: shouldn't happen basically
	}
}

void spin_unlock(spin_t lock) {
	int u = arch_atomic_swap(lock, 0);
	if (u != 1) {
		__asm__ __volatile__ ("cli\nhlt");
	}

	scheduler_unlock();
}
