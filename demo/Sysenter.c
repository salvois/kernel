#include "tscStopwatch.h"

__attribute__((noreturn)) int _start() {
    unsigned count = 0;
    uint64_t t = tscStopwatchBegin();
    while (1) {
        asm volatile (
        "   mov %%esp, %%ecx\n"
        "   lea 1f, %%edx\n"
        "   sysenter\n"
        "1:\n"
        : : "a" (1), "b" (count) : "ecx", "edx", "memory");
        count++;
        if ((count & 0xFFFFFFF) == 0) {
            t = tscStopwatchEnd() - t;
            asm volatile (
            "   mov %%esp, %%ecx\n"
            "   lea 1f, %%edx\n"
            "   sysenter\n"
            "1:\n"
            : : "a" (127), "b" ((uint32_t) (t >> 28)) : "ecx", "edx", "memory");
            t = tscStopwatchBegin();
        }
    }
}
