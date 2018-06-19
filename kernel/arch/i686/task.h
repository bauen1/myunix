#ifndef TASK_H
#define TASK_H 1

extern void kidle(void);
/* switch directly to next task */
extern void switch_task(void);
/* internal methods */
extern void __attribute__((noreturn)) __switch_direct(void);
extern void __attribute__((noreturn)) return_to_regs(void);

#endif
