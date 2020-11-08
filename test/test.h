#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED

extern int exitCode;

#define ASSERT(condition) \
    if (!(condition)) { \
        __builtin_printf("%%TEST_FAILED%% time=0 testname=%s (%s) message=Assertion %s failed in %s in %s:%u\n", __func__, __FILE__, #condition, __func__, __FILE__, __LINE__); \
        exitCode = -1; \
    }

#define RUN_TEST(name) \
    __builtin_printf("%%TEST_STARTED%%  %s (%s)\n", #name, __FILE__); \
    name(); \
    __builtin_printf("%%TEST_FINISHED%% time=0 %s (%s)\n", #name, __FILE__);

#define RUN_SUITE(name) \
    __builtin_printf("%%SUITE_STARTING%% %s\n", #name); \
    __builtin_printf("%%SUITE_STARTED%%\n"); \
    name(); \
    __builtin_printf("%%SUITE_FINISHED%% time=0\n");

#endif
