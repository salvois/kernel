#ifndef ASSERT_H_INCLUDED
#define ASSERT_H_INCLUDED

#if defined(NDEBUG) && !defined(__DOXYGEN__)
#define assert(condition)
#else
void __assert_fail(const char *condition, const char *filename, unsigned line, const char *function);
/** Aborts execution if the specified condition is false when NDEBUG is not defined. */
#define assert(condition) if (!(condition)) __assert_fail(#condition, __FILE__, __LINE__, __func__)
#endif

#endif
