#include "tscStopwatch.h"

/*
 * Times in TSC ticks (1.8 GHz):
 * bare metal, no CR3 reload on system call, doSystemCall: 83, touchMemory 64 KiB: 8644
 * bare metal, one CR3 reload on system call, doSystemCall: 175 (2x, +92), touchMemory 64 KiB: 10072 (1.16x)
 * bare metal, two CR3 reloads on system call, doSystemCall: 245 (3x, +92+70), touchMemory 64 KiB: 10072 (1.16x)
 * VirtualBox, no CR3 reload on system call, doSystemCall: 1560, touchMemory 64 KiB: 9936
 * VirtualBox, one CR3 reload on system call, doSystemCall: 1744 (+184), touchMemory 64 KiB: 10064 (1.01x)
 * VirtualBox, two CR3 reloads on system call, doSystemCall: 1816 (+184+72), touchMemory 64 KiB: 10064 (1.01x)
 * VirtualBox, no CR3 reload on system call, doSystemCall: 1632, touchMemory 1 MiB: 155000
 * VirtualBox, two CR3 reloads on system call, doSystemCall: 1932 (+300), touchMemory 1 MiB: 160000 (1.03x)
 */

#define WORKINGSET_LENGTH (64 * 1024 / sizeof(uint32_t))
static volatile uint32_t workingSet[WORKINGSET_LENGTH];

static uint64_t touchMemory(){
    uint64_t startTicks = tscStopwatchBegin();
    for (int i = 0; i < WORKINGSET_LENGTH; i++) {
        workingSet[i] = i;
    }
    uint64_t endTicks = tscStopwatchEnd();
    return endTicks - startTicks;
}

static uint64_t doSystemCall() {
    uint64_t startTicks = tscStopwatchBegin();
    asm volatile (
    "   mov %%esp, %%ecx\n"
    "   lea 1f, %%edx\n"
    "   sysenter\n"
    "1:\n"
    : : "a" (1) : "ecx", "edx", "memory");
    uint64_t endTicks = tscStopwatchEnd();
    return endTicks - startTicks;
}

__attribute__((noreturn))
int _start() {
    const int logMaxCount = 20;
    const int maxCount = 1 << logMaxCount;
    int count = 0;
    uint64_t cumulativeTouchMemoryTicks = 0;
    uint64_t cumulativeSyscallTicks = 0;
    while (1) {
        cumulativeTouchMemoryTicks += touchMemory();
        cumulativeSyscallTicks += doSystemCall();
        count++;
        if (count == maxCount) {
            uint32_t averageTouchMemoryTicks = cumulativeTouchMemoryTicks >> logMaxCount;
            uint32_t averageSyscallTicks = cumulativeSyscallTicks >> logMaxCount;
            cumulativeTouchMemoryTicks = 0;
            cumulativeSyscallTicks = 0;
            count = 0;
            asm volatile (
            "   mov %%esp, %%ecx\n"
            "   lea 1f, %%edx\n"
            "   sysenter\n"
            "1:\n"
            : : "a" (127), "b" (averageSyscallTicks), "S" (averageTouchMemoryTicks) : "ecx", "edx", "memory");
        }
    }
}
