#ifndef TASK_H
#define TASK_H 1

extern void kidle(void);
extern void switch_task(void);
extern void __attribute__((noreturn)) return_to_regs(void);

#endif
