/*
FreeDOS-32 kernel
Copyright (C) 2008-2020  Salvatore ISAJA

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include "kernel.h"

FakeHardware theFakeHardware;

PageTable Boot_kernelPageDirectory;

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
