#ifndef BOOT_H
#define BOOT_H 1

extern void *__start;
extern void *__text_start;
extern void *__text_end;
extern void *__rodata_start;
extern void *__rodata_end;
extern void *__data_start;
extern void *__data_end;
extern void *__bss_start;
extern void *__bss_end;
extern void *__end;

extern const uintptr_t __stack_chk_guard;

extern void *stack;

#endif
