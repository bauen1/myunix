#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H 1

#include <stddef.h>
#include <task.h>

typedef unsigned int (ktask_func)(const char *name, void *extra);

typedef struct {
	task_t task;
	const char *name;
	ktask_func *func;
	void *extra;
} ktask_t;

void ktask_spawn(ktask_func *func, const char *name, void *extra);
__attribute__((noreturn)) void __ktask_exit(unsigned int status);

#endif
