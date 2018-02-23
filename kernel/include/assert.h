#ifndef ASSERT_H
#define ASSERT_H 1

__attribute__((noreturn))
void __assert_failed(const char *exp, const char *file, int line);

#define assert(exp) ((void)((exp) ? 0: __assert_failed(#exp, __FILE__, __LINE__)))

__attribute__((noreturn))
void __stack_chk_fail();

#endif
