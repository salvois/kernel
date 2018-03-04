/*
FreeDOS-32 kernel
Copyright (C) 2008-2018  Salvatore ISAJA

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

/**
 * Calibrates the Local APIC Timer of the current CPU using the ACPI Power Management timer as a reference.
 * A divider of 16 is used for the LAPIC Timer.
 */
__attribute__((section(".boot"))) void LapicTimer_initialize(LapicTimer *lt) {
    Cpu_writeLocalApic(lapicTimerLvt, lapicTimerVector); // one-shot, not masked, vector lapicTimerVector
    Cpu_writeLocalApic(lapicTimerDivider, 0x03); // divide by 16
    unsigned acpiTicks = ACPI_PMTIMER_FREQUENCY / 8;
    Cpu_writeLocalApic(lapicTimerInitialCount, 0xFFFFFFFF); // start the LAPIC Timer with max count
    unsigned realAcpiTicks = AcpiPmTimer_busyWait(acpiTicks);
    Cpu_writeLocalApic(lapicTimerLvt, 0x10000); // one-shot, masked, vector 0, to stop the counting
    uint32_t lapicTimerDiff = 0xFFFFFFFF - Cpu_readLocalApic(lapicTimerCurrentCount);
    Log_printf("LAPIC Timer calibrated from %d ACPI PM Timer ticks. Diff=%d.\n", realAcpiTicks, lapicTimerDiff);
    /* Compute the coefficients to convert between LAPIC timer ticks and nanoseconds.
     * For the nsPerTick coefficient, a shift of at most 20 allows frequencies as low as 500 KHz.
     * For the ticksPerNs coefficient, a shift of at most 23 allows frequencies as high as 500 GHz.
     */
    lt->nsPerTick = div(292935555ULL * realAcpiTicks + lapicTimerDiff - 1, lapicTimerDiff); // 10^9*2^20/3579545
    lt->ticksPerNs = div(30027ULL * lapicTimerDiff + realAcpiTicks - 1, realAcpiTicks); // 2^23*3579545/10^9
    Log_printf("LAPIC Timer calibration: nsPerTick=0x%08X >> 20, ticksPerNs=0x%08X >> 23.\n", lt->nsPerTick, lt->ticksPerNs);
    lt->maxTicks = LapicTimer_convertNanosecondsToTicks(lt, 4000000000U);
    lt->currentNanoseconds = 0;
    lt->nextExpirationNanoseconds = 0;
    lt->lastInitialCount = 0;
    Cpu_writeLocalApic(lapicTimerLvt, lapicTimerVector); // one-shot, not masked, vector lapicTimerVector
    Cpu_writeLocalApic(lapicTimerDivider, 0x03); // divide by 16
    Cpu_writeLocalApic(lapicTimerInitialCount, 0);
}
