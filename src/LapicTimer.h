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
#ifndef LAPICTIMER_H_INCLUDED
#define LAPICTIMER_H_INCLUDED

#include "Types.h"

/** Converts the specified count of ticks of the Local APIC timer to nanoseconds. */
static inline uint32_t LapicTimer_convertTicksToNanoseconds(const LapicTimer *lt, uint32_t ticks) {
    return mul(ticks, lt->nsPerTick) >> 20;
}
/** Converts the specified number of nanoseconds to count of ticks of the Local APIC timer. */
static inline uint32_t LapicTimer_convertNanosecondsToTicks(const LapicTimer *lt, uint32_t ns) {
    return mul(ns, lt->ticksPerNs) >> 23;
}

/** Returns the current count of the Local APIC timer. */
static inline uint32_t LapicTimer_getCurrentCount(LapicTimer *lt) {
    return Cpu_readLocalApic(lapicTimerCurrentCount);
}

__attribute__((section(".boot"))) void LapicTimer_initialize(LapicTimer *lt);

#endif
