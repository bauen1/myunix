#ifndef ASSERT_H
#define ASSERT_H 1

/* __attribute__((noreturn)) void __assert_failed(const char *exp, const char *file, const int line); */
#define __assert_failed(e, f, l) ((void)_halt())

#define assert(exp) ((void)((exp) ? 0: __assert_failed(#exp, __FILE__, __LINE__)))

#endif
