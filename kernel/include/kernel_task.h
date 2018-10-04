#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H 1

#include <stddef.h>
#include <task.h>

typedef struct {
	task_t task;
	const char *name;
} ktask_t;

typedef int (ktask_func)(const char *name, void *extra);
void ktask_spawn(ktask_func func, const char *name, void *extra);
void __attribute__((noreturn)) __ktask_exit(uint32_t status);

#endif
