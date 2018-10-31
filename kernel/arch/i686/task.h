#ifndef ARCH_TASK_H
#define ARCH_TASK_H 1

#include <stdint.h>

/* save all registers and call switch_task(void *) */
extern void call_switch_task(void *);
/* internal methods */
extern void __attribute__((noreturn)) return_to_regs(void);
extern void __attribute__((noreturn)) __call_ktask(void);

extern uintptr_t read_eip();

#endif
