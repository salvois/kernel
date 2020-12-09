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
#include "PhysicalMemory.h"

/** The global unique instance of the ACPI FADT structure. */
AcpiFadt Acpi_fadt;
/** The global unique instance of the ACPI HPET structure. */
AcpiHpet Acpi_hpet;
/** The global unique instance of the ACPI Power Management timer. */
AcpiPmTimer AcpiPmTimer_theInstance;

uint16_t Acpi_readPm1Status() {
    uint16_t a = 0;
    uint16_t b = 0;
    if (Acpi_fadt.pm1aEvtBlk != 0) a = inpw(Acpi_fadt.pm1aEvtBlk);
    if (Acpi_fadt.pm1bEvtBlk != 0) a = inpw(Acpi_fadt.pm1bEvtBlk);
    return a | b;
}

void Acpi_writePm1Status(uint16_t value) {
    if (Acpi_fadt.pm1aEvtBlk != 0) outpw(Acpi_fadt.pm1aEvtBlk, value);
    if (Acpi_fadt.pm1bEvtBlk != 0) outpw(Acpi_fadt.pm1bEvtBlk, value);
}

uint16_t Acpi_readPm1Enable() {
    uint16_t a = 0;
    uint16_t b = 0;
    if (Acpi_fadt.pm1aEvtBlk != 0) a = inpw(Acpi_fadt.pm1aEvtBlk + Acpi_fadt.pm1EvtLen / 2);
    if (Acpi_fadt.pm1bEvtBlk != 0) a = inpw(Acpi_fadt.pm1bEvtBlk + Acpi_fadt.pm1EvtLen / 2);
    return a | b;
}

void Acpi_writePm1Enable(uint16_t value) {
    if (Acpi_fadt.pm1aEvtBlk != 0) outpw(Acpi_fadt.pm1aEvtBlk + Acpi_fadt.pm1EvtLen / 2, value);
    if (Acpi_fadt.pm1bEvtBlk != 0) outpw(Acpi_fadt.pm1bEvtBlk + Acpi_fadt.pm1EvtLen / 2, value);
}

uint16_t Acpi_readPm1Control() {
    uint16_t a = 0;
    uint16_t b = 0;
    if (Acpi_fadt.pm1aCntBlk != 0) a = inpw(Acpi_fadt.pm1aCntBlk);
    if (Acpi_fadt.pm1bCntBlk != 0) a = inpw(Acpi_fadt.pm1bCntBlk);
    return a | b;
}

void Acpi_writePm1Control(uint16_t value) {
    if (Acpi_fadt.pm1aCntBlk != 0) outpw(Acpi_fadt.pm1aCntBlk, value);
    if (Acpi_fadt.pm1bCntBlk != 0) outpw(Acpi_fadt.pm1bCntBlk, value);
}

__attribute__((section(".boot")))
const AcpiRootSystemDescPointer *Acpi_doSearchRootSystemDescriptorPointer(PhysicalAddress begin, PhysicalAddress end) {
    Log_printf("Searching for the ACPI Root System Descriptor Pointer structure at physical address [%p, %p).\n", begin, end);
    assert((begin.v & 0xF) == 0); // the structure must be aligned to a 16-byte boundary
    for ( ; begin.v < end.v; begin.v += 16) {
        const AcpiRootSystemDescPointer *rsdp = phys2virt(begin);
        if (memcmp(rsdp->signature, "RSD PTR ", 8) == 0) {
            uint8_t checksum = 0;
            const uint8_t *base = (const uint8_t*) rsdp;
            for (size_t i = 0; i < 20; i++) {
                checksum += base[i];
            }
            if (checksum == 0) return rsdp;
        }
    }
    return NULL;
}

__attribute__((section(".boot")))
static const AcpiRootSystemDescPointer *Acpi_searchRootSystemDescriptorPointer() {
    uint16_t ebdaRealModeSegment = *(const uint16_t *) phys2virt(physicalAddress(0x40E));
    PhysicalAddress ebda = physicalAddress((uintptr_t) ebdaRealModeSegment << 4);
    const AcpiRootSystemDescPointer *rsdp = Acpi_doSearchRootSystemDescriptorPointer(ebda, addToPhysicalAddress(ebda, 1024));
    if (rsdp == NULL) rsdp = Acpi_doSearchRootSystemDescriptorPointer(physicalAddress(0xE0000), physicalAddress(0x100000));
    if (rsdp != NULL)
        Log_printf("ACPI Root System Descriptor Pointer structure revision %i found at %p, checksum=0x%02X, OEM '%.6s'. RSDT at %p, length=%i, XSDT at %p, extended checksum=0x%02X.\n",
                rsdp->revision, rsdp, rsdp->checksum, rsdp->oemId,
                rsdp->rsdtPhysicalAddress, rsdp->length,
                (uint32_t) rsdp->xsdtPhysicalAddress, rsdp->extendedChecksum);
    else
        Log_printf("ACPI Root System Descriptor Pointer structure not found.\n");
    return rsdp;
}

__attribute__((section(".boot")))
void *Acpi_mapToTemporaryArea(PhysicalAddress address, int tempAreaIndex) {
    VirtualAddress virtualAddress = AddressSpace_getTemporaryMappingArea(tempAreaIndex);
    ptrdiff_t offset = address.v & 0x3FFFFF;
    uintptr_t base = address.v & ~0x3FFFFF;
    int pageNumber = virtualAddress.v >> 22;
    PageTable *pd = (PageTable *) &Boot_kernelPageDirectory;
    PageTableEntry pte = base | ptLargePage | ptPresent;
    if (pd->entries[pageNumber] != pte) {
        pd->entries[pageNumber] = pte;
        pd->entries[pageNumber + 1] = (base + 0x400000) | ptLargePage | ptPresent;
        AddressSpace_invalidateTlbAddress(virtualAddress);
        AddressSpace_invalidateTlbAddress(addToVirtualAddress(virtualAddress, 0x400000));
    }
    return (void *) (virtualAddress.v + offset);
}

__attribute__((section(".boot")))
void Acpi_doFindConfig(PhysicalAddress rsdtPhysicalAddress, void *(*mapToTemporaryArea)(PhysicalAddress address, int tempAreaIndex)) {
    const AcpiRootSystemDescTable *rsdt = mapToTemporaryArea(rsdtPhysicalAddress, 0);
    int entryCount = (rsdt->header.length - sizeof(AcpiDescriptionHeader)) / sizeof(uint32_t);
    for (int i = 0; i < entryCount; i++) {
        const AcpiDescriptionHeader *header = mapToTemporaryArea(physicalAddress(rsdt->entries[i]), 1);
        Video_printf("  ACPI Root System Descriptor Table entry %i/%i at %p is '%.4s'.\n", i, entryCount, header, &header->signature);
        if (header->signature == ACPI_FADT_SIGNATURE) {
            Acpi_fadt = *(const AcpiFadt *) header;
            Log_printf("    SCI interrupt vector: %d.\n", Acpi_fadt.sciInt);
            Log_printf("    PM1 Event registers at: a=%p, b=%p, len=%d.\n", Acpi_fadt.pm1aEvtBlk, Acpi_fadt.pm1bEvtBlk, Acpi_fadt.pm1EvtLen);
            Log_printf("    Power Management Timer at %p, %u bytes wide, 32-bit=%i (FADT flags=0x%08X).\n",
                    Acpi_fadt.pmTmrBlk, Acpi_fadt.pmTmrLen, (Acpi_fadt.flags & 0x100) != 0, Acpi_fadt.flags);
        } else if (header->signature == ACPI_HPET_SIGNATURE) {
            Acpi_hpet = *(const AcpiHpet *) header;
            Video_printf("    HPET at %p, Event Timer Block Id=0x%08X, base address=0x %08X %08X, HPET number=%i, minimum clock tick=%i, page protection=0x%02X.\n",
                    &Acpi_hpet, Acpi_hpet.eventTimerBlockId,
                    (uint32_t) (Acpi_hpet.baseAddress.address >> 32), (uint32_t) Acpi_hpet.baseAddress.address,
                    Acpi_hpet.hpetNumber, Acpi_hpet.minimumClockTick, Acpi_hpet.pageProtection);
        }
    }
    if (Acpi_fadt.header.signature != ACPI_FADT_SIGNATURE)
        panic("ACPI FADT not found. Aborting.");
}

/**
 * Attempts to find ACPI tables in memory and read ACPI configuration.
 * Called during kernel initialization on the bootstrap processor.
 */
__attribute__((section(".boot")))
void Acpi_findConfig() {
    const AcpiRootSystemDescPointer *rsdp = Acpi_searchRootSystemDescriptorPointer();
    if (rsdp != NULL)
        Acpi_doFindConfig(physicalAddress(rsdp->rsdtPhysicalAddress), Acpi_mapToTemporaryArea);
}

__attribute__((section(".boot")))
void Acpi_switchToAcpiMode() {
    if (Acpi_fadt.header.signature != ACPI_FADT_SIGNATURE) {
        Log_printf("ACPI FADT table not found. Will not run in ACPI mode.\n");
        return;
    }
    if ((Acpi_readPm1Control() & 1) == 0) { // sciEn
        Log_printf("System is in legacy mode. Switching to ACPI mode...\n");
        outp(Acpi_fadt.smiCmd, Acpi_fadt.acpiEnable);
        while ((Acpi_readPm1Control() & 1) == 0) { // sciEn
            Cpu_relax();
        }
    }
    Log_printf("System is in ACPI mode.\n");
}

/**
 * Initializes the kernel state of the ACPI Power management timer.
 * The ACPI Power Management Timer is driven by a 3.579545 MHz clock.
 * The counter of the timer can be either 32-bit or 24-bit wide.
 * When the system is in ACPI mode, according to the specification a SCI
 * (System Control Interrupt) should be generated every time the MSB changes
 * (either from 0 to 1 or from 1 to 0). Unfortunately it appears this is
 * not always the case (e.g. Bochs, VirtualBox), so if we want to rely on
 * the ACPI PM Timer to measure elapsed time we need a different interrupt
 * (e.g. from the RTC) to frequently check for wrap-around.
 */
__attribute__((section(".boot")))
void AcpiPmTimer_initialize() {
    if (Acpi_fadt.flags & 0x100) { // timer is 32-bit
        AcpiPmTimer_theInstance.timerMax = 0x100000000ULL;
        AcpiPmTimer_theInstance.timerMaxNanoseconds = 1199864031881LL; // 1199.86 seconds

    } else { // PM Timer is 24-bit
        AcpiPmTimer_theInstance.timerMax = 0x01000000ULL;
        AcpiPmTimer_theInstance.timerMaxNanoseconds = 4686968875LL; // 4.69 seconds
    }
    AcpiPmTimer_theInstance.prevCount = Acpi_readPmTimer();
    AcpiPmTimer_theInstance.cumulativeNanoseconds = 0;
}

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
