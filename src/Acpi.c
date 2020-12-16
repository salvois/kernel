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

/** The global unique instance of the ACPI FADT structure. */
AcpiFadt Acpi_fadt;
/** The global unique instance of the ACPI HPET structure. */
AcpiHpet Acpi_hpet;
/** The global unique instance of the ACPI Power Management timer. */
AcpiPmTimer AcpiPmTimer_theInstance;

/**
 * Runs in a tight loop while reading the ACPI PM Timer counter until
 * at least the specified number of ticks have elapsed.
 * @return The number of ACPI PM Timer ticks actually passed.
 */
unsigned AcpiPmTimer_busyWait(unsigned ticks) {
    unsigned countedTicks = 0;
    uint64_t prev = Acpi_readPmTimer();
    while (countedTicks < ticks) {
        uint64_t curr = Acpi_readPmTimer();
        if (curr < prev) curr += AcpiPmTimer_theInstance.timerMax;
        countedTicks += curr - prev;
        prev = curr;
    }
    return countedTicks;
}
