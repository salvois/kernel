#include "kernel.h"

FakeHardware theFakeHardware;

uint32_t Cpu_readLocalApic(size_t offset) {
    switch (offset) {
        case lapicEoi: return theFakeHardware.lapicEoi;
        case lapicSpuriousInterrupt: return theFakeHardware.lapicSpuriousInterrupt;
        case lapicInterruptCommandLow: return theFakeHardware.lapicInterruptCommandLow;
        case lapicInterruptCommandHigh: return theFakeHardware.lapicInterruptCommandHigh;
        case lapicTimerLvt: return theFakeHardware.lapicTimerLvt;
        case lapicPerformanceCounterLvt: return theFakeHardware.lapicPerformanceCounterLvt;
        case lapicLint0Lvt: return theFakeHardware.lapicLint0Lvt;
        case lapicLint1Lvt: return theFakeHardware.lapicLint1Lvt;
        case lapicTimerInitialCount: return theFakeHardware.lapicTimerInitialCount;
        case lapicTimerCurrentCount: return theFakeHardware.lapicTimerCurrentCount;
        case lapicTimerDivider: return theFakeHardware.lapicTimerDivider;
        default: assert(false); return 0xDEADBEEF;
    }
}

void Cpu_writeLocalApic(size_t offset, uint32_t value) {
    switch (offset) {
        case lapicEoi: theFakeHardware.lapicEoi = value; break;
        case lapicSpuriousInterrupt: theFakeHardware.lapicSpuriousInterrupt = value; break;
        case lapicInterruptCommandLow: theFakeHardware.lapicInterruptCommandLow = value; break;
        case lapicInterruptCommandHigh: theFakeHardware.lapicInterruptCommandHigh = value; break;
        case lapicTimerLvt: theFakeHardware.lapicTimerLvt = value; break;
        case lapicPerformanceCounterLvt: theFakeHardware.lapicPerformanceCounterLvt = value; break;
        case lapicLint0Lvt: theFakeHardware.lapicLint0Lvt = value; break;
        case lapicLint1Lvt: theFakeHardware.lapicLint1Lvt = value; break;
        case lapicTimerInitialCount: theFakeHardware.lapicTimerInitialCount = value; break;
        case lapicTimerCurrentCount: theFakeHardware.lapicTimerCurrentCount = value; break;
        case lapicTimerDivider: theFakeHardware.lapicTimerDivider = value; break;
        default: assert(false);
    }
}
