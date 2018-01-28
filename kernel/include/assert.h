#ifndef ASSERT_H
#define ASSERT_H 1

__attribute__((noreturn))
void __assert_failed(const char *exp, const char *file, const int *line);

#ifdef NDEBUG
#define assert(exp) ((void)0)
#else
#define assert(exp) ((void)((exp) ? 0: __assert_failed(#exp, __FILE__, __LINE__)))
#endif

#endif
