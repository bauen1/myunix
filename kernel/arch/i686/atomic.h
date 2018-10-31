#ifndef ARCH_ATOMIC_H
#define ARCH_ATOMIC_H 1

typedef volatile int spin_t[1];

extern void spin_init(spin_t lock);
extern void spin_lock(spin_t lock);
extern void spin_unlock(spin_t lock);

#endif
