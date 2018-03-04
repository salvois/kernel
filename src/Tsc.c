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

/** Calibrates the TSC of the current CPU using the ACPI Power Management timer as a reference. */
void __attribute__((section(".boot"))) Tsc_initialize(Tsc *tsc) {
    unsigned acpiTicks = ACPI_PMTIMER_FREQUENCY / 8;
    uint64_t initial = Tsc_read();
    unsigned realAcpiTicks = AcpiPmTimer_busyWait(acpiTicks);
    uint64_t final = Tsc_read();
    if (final - initial > UINT32_MAX) {
        panic("TSC way too fast!\n");
    }
    uint32_t tscDiff = final - initial;
    Log_printf("TSC calibrated from %d ACPI PM Timer ticks. Diff=%d.\n", realAcpiTicks, tscDiff);
    /* Compute the coefficients to convert between TSC ticks and nanoseconds.
     * For the nsPerTick coefficient, a shift of at most 20 allows frequencies as low as 500 KHz.
     * For the ticksPerNs coefficient, a shift of at most 23 allows frequencies as high as 500 GHz.
     */
    tsc->nsPerTick = div(292935555ULL * realAcpiTicks + tscDiff - 1, tscDiff); // 10^9*2^20/3579545
    tsc->usPerTick = div(1199864032ULL * realAcpiTicks + tscDiff - 1, tscDiff); // 10^6*2^32/3579545
    tsc->ticksPerNs = div(30027ULL * tscDiff + realAcpiTicks - 1, realAcpiTicks); // 2^23*3579545/10^9
    Log_printf("TSC calibration: nsPerTick=0x%08X >> 20, usPerTick=0x%08X >> 32, ticksPerNs=0x%08X >> 23.\n", tsc->nsPerTick, tsc->usPerTick, tsc->ticksPerNs);
}

/**
 * Runs in a tight loop while reading the TSC until at least the specified number of ticks has elapsed.
 * @return The number of TSC ticks actually passed.
 */
unsigned Tsc_busyWait(unsigned ticks) {
    unsigned countedTicks = 0;
    uint64_t prev = Tsc_read();
    while (countedTicks < ticks) {
        uint64_t curr = Tsc_read();
        countedTicks += curr - prev;
        prev = curr;
    }
    return countedTicks;
}
