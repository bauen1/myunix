#ifndef TASK_H
#define TASK_H 1

extern void kidle(void);
/* swicth directly to next, ONLY CALL WITH INTERRUPTS DISABLED ! */
extern void switch_task(void);
/* internal methods */
extern void __attribute__((noreturn)) __switch_direct(void);
extern void __attribute__((noreturn)) return_to_regs(void);

#endif
