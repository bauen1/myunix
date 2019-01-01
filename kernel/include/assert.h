#ifndef ASSERT_H
#define ASSERT_H 1

#include <stdint.h>

#define static_assert(cond) typedef char __static_assert_##__FILE__##__LINE__[(cond) ? 1:-1]

void __attribute__((noreturn)) __assert_failed(const char *exp, const char *file, int line);

// TODO: make assert a vararg macro and allow passing a message

#ifdef NDEBUG
#define assert(exp) ((void)((exp) ? ((void) 0) : __assert_failed((void *)0, __FILE__, __LINE__)))
#else
#define assert(exp) ((void)((exp) ? ((void) 0) : __assert_failed(#exp, __FILE__, __LINE__)))
#endif

void __attribute__((noreturn)) __stack_chk_fail(void);

void __attribute__((noreturn)) __panic(const char *msg, const char *file, int line);
#define panic(msg) ((void)__panic(#msg, __FILE__,  __LINE__))

#endif
