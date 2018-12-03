#include <assert.h>
#include <atomic.h>
#include <task.h>

void spin_init(spin_t lock) {
	lock[0] = 0;
}

inline int arch_atomic_swap(volatile int *location, int value) {
	__asm__ __volatile__ ("xchg %0, %1" : "=r"(value), "=m"(*location) : "0"(value) : "memory");
	return value;
}

void spin_lock(spin_t lock) {
	scheduler_lock();

	while ( arch_atomic_swap(lock, 1) != 0) {
		// XXX: shouldn't happen basically
		panic("couldn't acquire spin lock on single processor system: deadlock");
	}
}

void spin_unlock(spin_t lock) {
	int u = arch_atomic_swap(lock, 0);
	if (u != 1) {
		panic("trying to unlock lock that wasn't locked");
	}

	scheduler_unlock();
}
