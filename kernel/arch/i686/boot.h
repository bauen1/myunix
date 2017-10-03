#ifndef BOOT_H
#define BOOT_H 1

__attribute__((noreturn)) extern void _halt();
#define halt() ((void)_halt())

extern void *start;
extern void *code;
extern void *rodata;
extern void *data;
extern void *bss;
extern void *end;

__attribute__((noreturn)) extern void __stack_chk_fail();
const uintptr_t __stack_chk_guard;

extern void *stack;

#endif
