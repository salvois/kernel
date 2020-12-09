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
#include "test.h"
#include "kernel.h"
#include "hardware/hardware.h"

static void AcpiTest_doSearchRootSystemDescriptorPointer_present() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    AcpiRootSystemDescPointer expectedRsdp = {
        .signature = "RSD PTR ",
        .checksum = 0xE2,
        .oemId = "VBOX  ",
        .revision = 2,
        .rsdtPhysicalAddress = 0x7FFF0000,
        .length = 36,
        .xsdtPhysicalAddress = 0x7FFF0030,
        .extendedChecksum = 0x2E
    };
    memzero(fakePhysicalMemory, PAGE_SIZE);
    AcpiRootSystemDescPointer *rsdp = (AcpiRootSystemDescPointer *) &fakePhysicalMemory[48];
    *rsdp = expectedRsdp;
    
    const AcpiRootSystemDescPointer* actualRsdp = Acpi_doSearchRootSystemDescriptorPointer(virt2phys(fakePhysicalMemory), virt2phys(fakePhysicalMemory + PAGE_SIZE));
    
    ASSERT(memcmp(actualRsdp, &expectedRsdp, sizeof(AcpiRootSystemDescPointer)) == 0);
}

static void AcpiTest_doSearchRootSystemDescriptorPointer_wrongChecksum() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    AcpiRootSystemDescPointer expectedRsdp = {
        .signature = "RSD PTR ",
        .checksum = 0,
        .oemId = "VBOX  ",
        .revision = 2,
        .rsdtPhysicalAddress = 0x7FFF0000,
        .length = 36,
        .xsdtPhysicalAddress = 0x7FFF0030,
        .extendedChecksum = 0x2E
    };
    memzero(fakePhysicalMemory, PAGE_SIZE);
    AcpiRootSystemDescPointer *rsdp = (AcpiRootSystemDescPointer *) &fakePhysicalMemory[48];
    *rsdp = expectedRsdp;
    
    const AcpiRootSystemDescPointer* actualRsdp = Acpi_doSearchRootSystemDescriptorPointer(virt2phys(fakePhysicalMemory), virt2phys(fakePhysicalMemory + PAGE_SIZE));
    
    ASSERT(actualRsdp == NULL);
}

static void AcpiTest_doSearchRootSystemDescriptorPointer_missing() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    memzero(fakePhysicalMemory, PAGE_SIZE);
    
    const AcpiRootSystemDescPointer* actualRsdp = Acpi_doSearchRootSystemDescriptorPointer(virt2phys(fakePhysicalMemory), virt2phys(fakePhysicalMemory + PAGE_SIZE));
    
    ASSERT(actualRsdp == NULL);
}

static void AcpiTest_mapToTemporaryArea_firstMap() {
    memzero(&Boot_kernelPageDirectory, sizeof(PageTable));
    theFakeHardware = (FakeHardware) { .lastTlbInvalidationAddress = makeVirtualAddress(0) };

    void *mapped = Acpi_mapToTemporaryArea(physicalAddress(0x12345678), 1);

    ASSERT((uintptr_t) mapped == 0xF8B45678);
    ASSERT(Boot_kernelPageDirectory.entries[0x3E2] == (0x12000000 | ptLargePage | ptPresent));
    ASSERT(Boot_kernelPageDirectory.entries[0x3E3] == (0x12400000 | ptLargePage | ptPresent));
    ASSERT(theFakeHardware.lastTlbInvalidationAddress.v == 0xF8C00000);
}

static void AcpiTest_mapToTemporaryArea_alreadyMapped() {
    memzero(&Boot_kernelPageDirectory, sizeof(PageTable));
    Boot_kernelPageDirectory.entries[0x3E2] = 0x12000000 | ptLargePage | ptPresent;
    Boot_kernelPageDirectory.entries[0x3E3] = 0x12400000 | ptLargePage | ptPresent;
    theFakeHardware = (FakeHardware) { .lastTlbInvalidationAddress = makeVirtualAddress(0) };

    void *mapped = Acpi_mapToTemporaryArea(physicalAddress(0x1234ABCD), 1);

    ASSERT((uintptr_t) mapped == 0xF8B4ABCD);
    ASSERT(theFakeHardware.lastTlbInvalidationAddress.v == 0);
}

static void *fakeMapToTemporaryArea(PhysicalAddress address, int tempAreaIndex) {
    return (void *) address.v;
}

static void AcpiTest_doFindConfig() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    memzero(fakePhysicalMemory, PAGE_SIZE);
    memzero(&Acpi_fadt, sizeof(Acpi_fadt));
    memzero(&Acpi_hpet, sizeof(Acpi_hpet));
    AcpiRootSystemDescTable *rsdt = (AcpiRootSystemDescTable *) fakePhysicalMemory;
    rsdt->header.signature = ACPI_RSDT_SIGNATURE;
    rsdt->header.length = sizeof(AcpiDescriptionHeader) + 3 * sizeof(uint32_t);
    rsdt->entries[0] = (uint32_t) &fakePhysicalMemory[1024];
    rsdt->entries[1] = (uint32_t) &fakePhysicalMemory[512];
    rsdt->entries[2] = (uint32_t) &fakePhysicalMemory[2048];
    AcpiFadt expectedFadt = {
        .header.signature = ACPI_FADT_SIGNATURE,
        .sciInt = 9,
        .pm1aEvtBlk = 0x00004000,
        .pm1bEvtBlk = 0x00000000,
        .pm1EvtLen = 4,
        .pmTmrBlk = 0x00004008,
        .pmTmrLen = 4,
        .flags = 0x00000541
    };
    AcpiHpet expectedHpet = {
        .header.signature = ACPI_HPET_SIGNATURE,
        .eventTimerBlockId = 0x12345678,
        .baseAddress.address = 0x12345678DEADBEEF,
        .hpetNumber = 1,
        .minimumClockTick = 0x55AA,
        .pageProtection = 42
    };
    *(AcpiDescriptionHeader *) &fakePhysicalMemory[1024] = (AcpiDescriptionHeader) {
        .signature = 0xDEADBEEF
    };
    *(AcpiFadt *) &fakePhysicalMemory[512] = expectedFadt;
    *(AcpiHpet *) &fakePhysicalMemory[2048] = expectedHpet;

    Acpi_doFindConfig(physicalAddress((uintptr_t) fakePhysicalMemory), fakeMapToTemporaryArea);

    ASSERT(memcmp(&Acpi_fadt, &expectedFadt, sizeof(AcpiFadt)) == 0);
    ASSERT(memcmp(&Acpi_hpet, &expectedHpet, sizeof(AcpiHpet)) == 0);
}

void AcpiTest_run() {
    RUN_TEST(AcpiTest_doSearchRootSystemDescriptorPointer_present);
    RUN_TEST(AcpiTest_doSearchRootSystemDescriptorPointer_wrongChecksum);
    RUN_TEST(AcpiTest_doSearchRootSystemDescriptorPointer_missing);
    RUN_TEST(AcpiTest_mapToTemporaryArea_firstMap);
    RUN_TEST(AcpiTest_mapToTemporaryArea_alreadyMapped);
    RUN_TEST(AcpiTest_doFindConfig);
}
