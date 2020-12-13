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

static void MultiProcessorSpecificationTest_doSearchRootSystemDescriptorPointer_present() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    MpFloatingPointer expectedMpfp = {
        .signature = MPFLOATINGPOINTER_SIGNATURE,
        .mpConfigPhysicalAddress = 0x000E1300,
        .mpFloatingPointerSize = 1,
        .version = 4,
        .checksum = 0x7F,
        .defaultConfig = 0,
        .imcrp = 0
    };
    memzero(fakePhysicalMemory, PAGE_SIZE);
    MpFloatingPointer *mpfp = (MpFloatingPointer *) &fakePhysicalMemory[48];
    *mpfp = expectedMpfp;
    
    const MpFloatingPointer* actualMpfp = MultiProcessorSpecification_searchMpFloatingPointer(virt2phys(fakePhysicalMemory), virt2phys(fakePhysicalMemory + PAGE_SIZE));
    
    ASSERT(memcmp(actualMpfp, &expectedMpfp, sizeof(MpFloatingPointer)) == 0);
}

static void MultiProcessorSpecificationTest_doSearchRootSystemDescriptorPointer_wrongChecksum() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    MpFloatingPointer expectedMpfp = {
        .signature = MPFLOATINGPOINTER_SIGNATURE,
        .mpConfigPhysicalAddress = 0x000E1300,
        .mpFloatingPointerSize = 1,
        .version = 4,
        .checksum = 0,
        .defaultConfig = 0,
        .imcrp = 0
    };
    memzero(fakePhysicalMemory, PAGE_SIZE);
    MpFloatingPointer *mpfp = (MpFloatingPointer *) &fakePhysicalMemory[48];
    *mpfp = expectedMpfp;
    
    const MpFloatingPointer* actualMpfp = MultiProcessorSpecification_searchMpFloatingPointer(virt2phys(fakePhysicalMemory), virt2phys(fakePhysicalMemory + PAGE_SIZE));
    
    ASSERT(actualMpfp == NULL);
}

static void MultiProcessorSpecificationTest_doSearchRootSystemDescriptorPointer_missing() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    memzero(fakePhysicalMemory, PAGE_SIZE);
    
    const MpFloatingPointer* actualMpfp = MultiProcessorSpecification_searchMpFloatingPointer(virt2phys(fakePhysicalMemory), virt2phys(fakePhysicalMemory + PAGE_SIZE));
    
    ASSERT(actualMpfp == NULL);
}

static void MultiProcessorSpecificationTest_scanProcessors_callback(void *closure, int lapicId) {
    int *callCount = (int *) closure;
    ASSERT(*callCount < 2);
    ASSERT(*callCount != 0 || lapicId == 0x00);
    ASSERT(*callCount != 1 || lapicId == 0x02);
    (*callCount)++;
}

static void MultiProcessorSpecificationTest_scanProcessors() {
    struct {
        MpConfigHeader header;
        MpConfigProcessor processors[3];
        MpConfigIoapic ioapics[1];
    } fakeMemory = {
        .header = { .entryCount = 4 },
        .processors = {
            { .entryType = mpConfigProcessor, .lapicId = 0x00, .flags = 1 },
            { .entryType = mpConfigProcessor, .lapicId = 0x01, .flags = 0 },
            { .entryType = mpConfigProcessor, .lapicId = 0x02, .flags = 1 }
        },
        .ioapics = { { .entryType = mpConfigIoapic } }
    };
    int callCount = 0;

    MultiProcessorSpecification_scanProcessors(&fakeMemory.header, &callCount, MultiProcessorSpecificationTest_scanProcessors_callback);
}

void MultiProcessorSpecificationTest_run() {
    RUN_TEST(MultiProcessorSpecificationTest_doSearchRootSystemDescriptorPointer_present);
    RUN_TEST(MultiProcessorSpecificationTest_doSearchRootSystemDescriptorPointer_wrongChecksum);
    RUN_TEST(MultiProcessorSpecificationTest_doSearchRootSystemDescriptorPointer_missing);
    RUN_TEST(MultiProcessorSpecificationTest_scanProcessors);
}
