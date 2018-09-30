#ifndef ARCH_BOOT_H
#define ARCH_BOOT_H 1

extern void *_start; /* start of kernel / .text */
#define __text_start _start
extern void *_etext; /* end of .text / start of .data */
#define __text_end _etext
#define __data_start _etext
extern void *_edata; /* end of .data / start of .bss */
#define __data_end _edata
#define __bss_start _edata
#define __bss_end _end
extern void *_end; /* end of kernel / .bss */

extern const uintptr_t __stack_chk_guard;

extern void *stack;

#endif
