#ifndef ARCH_TASK_H
#define ARCH_TASK_H 1

/* switch directly to next task */
extern void switch_task(void);
/* internal methods */
extern void __attribute__((noreturn)) __switch_direct(void);
extern void __attribute__((noreturn)) return_to_regs(void);
extern void __attribute__((noreturn)) __call_ktask(void);

#endif
