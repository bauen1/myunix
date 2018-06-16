#ifndef ASSERT_H
#define ASSERT_H 1

#define static_assert(cond) typedef char __static_assert_##__FILE__##__LINE__[(cond) ? 1:-1]

void __attribute__((noreturn)) __assert_failed(const char *exp, const char *file, int line);

#ifdef NDEBUG
#define assert(exp) ((void)((exp) ? 0: __assert_failed((void *)0, __FILE__, __LINE__)))
#else
#define assert(exp) ((void)((exp) ? 0: __assert_failed(#exp, __FILE__, __LINE__)))
#endif

void __attribute__((noreturn)) __stack_chk_fail();

void print_stack_trace(unsigned int max_frames);

#endif
