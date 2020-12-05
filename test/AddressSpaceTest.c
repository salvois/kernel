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

static void initializePhysicalMemory(uint8_t *fakePhysicalMemory, Frame *frames, size_t frameCount) {
    memzero(frames, frameCount * sizeof(Frame));
    PhysicalAddress base = virt2phys(fakePhysicalMemory);
    FrameNumber baseFrame = floorToFrame(base);
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], baseFrame, addToFrameNumber(baseFrame, frameCount));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], addToFrameNumber(baseFrame, frameCount), addToFrameNumber(baseFrame, frameCount));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], addToFrameNumber(baseFrame, frameCount), addToFrameNumber(baseFrame, frameCount));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_firstFrame = baseFrame;
    PhysicalMemory_add(base, addToPhysicalAddress(base, frameCount * PAGE_SIZE));
}

static void AddressSpaceTest_initialize() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    Frame frames[1];
    initializePhysicalMemory(fakePhysicalMemory, frames, 1);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        Boot_kernelPageDirectory.entries[i] = i;
    Task task;

    int result = AddressSpace_initialize(&task);

    const PageTable *allocatedPageDirectory = (const PageTable *) fakePhysicalMemory;
    ASSERT(result == 0);
    ASSERT(task.addressSpace.root == virt2phys(allocatedPageDirectory).v);
    ASSERT(task.addressSpace.tlbShootdownPageCount == 0);
    ASSERT(task.addressSpace.shootdownFrameListHead.next == &task.addressSpace.shootdownFrameListHead);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(allocatedPageDirectory->entries[i] == (i < 768 ? 0 : i));
}

static void AddressSpaceTest_initializeOutOfMemory() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    Frame frames[1];
    initializePhysicalMemory(fakePhysicalMemory, frames, 0);
    Task task;

    int result = AddressSpace_initialize(&task);

    ASSERT(result == -ENOMEM);
}

static void AddressSpaceTest_map() {
    const size_t totalMemoryFrames = 3;
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[totalMemoryFrames * PAGE_SIZE];
    Frame frames[totalMemoryFrames];
    initializePhysicalMemory(fakePhysicalMemory, frames, totalMemoryFrames);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        Boot_kernelPageDirectory.entries[i] = i;
    Task task;
    AddressSpace_initialize(&task);
    FrameNumber frameNumber = PhysicalMemory_allocate(&task, permamapMemoryRegion);

    int mapResult = AddressSpace_map(&task, makeVirtualAddress((3 << 22) | (7 << 12)), frameNumber);

    const PageTable *pageDirectory = (const PageTable *) &fakePhysicalMemory[(totalMemoryFrames - 1) * PAGE_SIZE];
    const PageTable *pageTable = (const PageTable *) &fakePhysicalMemory[(totalMemoryFrames - 3) * PAGE_SIZE];
    ASSERT(frame2virt(frameNumber) == &fakePhysicalMemory[(totalMemoryFrames - 2) * PAGE_SIZE]);
    ASSERT(mapResult == 0);
    ASSERT(task.addressSpace.root == virt2phys(pageDirectory).v);
    ASSERT(task.addressSpace.tlbShootdownPageCount == 0);
    ASSERT(task.addressSpace.shootdownFrameListHead.next == &task.addressSpace.shootdownFrameListHead);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(pageDirectory->entries[i] == (
                i == 3 ? (virt2phys(pageTable).v | ptPresent | ptWriteable | ptUser)
                : i < 768 ? 0
                : i));
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(pageTable->entries[i] == (
                i == 7 ? (frameNumber.v * PAGE_SIZE | ptPresent | ptWriteable | ptUser)
                : 0));
}

static void AddressSpaceTest_mapMultiple() {
    const size_t totalMemoryFrames = 4;
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[totalMemoryFrames * PAGE_SIZE];
    Frame frames[totalMemoryFrames];
    initializePhysicalMemory(fakePhysicalMemory, frames, totalMemoryFrames);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        Boot_kernelPageDirectory.entries[i] = i;
    Task task;
    AddressSpace_initialize(&task);
    FrameNumber frameNumber = PhysicalMemory_allocate(&task, permamapMemoryRegion);

    AddressSpace_map(&task, makeVirtualAddress((3 << 22) | (7 << 12)), frameNumber);
    AddressSpace_map(&task, makeVirtualAddress((3 << 22) | (8 << 12)), frameNumber);
    AddressSpace_map(&task, makeVirtualAddress((25 << 22) | (16 << 12)), frameNumber);
    AddressSpace_map(&task, makeVirtualAddress((25 << 22) | (114 << 12)), frameNumber);

    const PageTable *pageDirectory = (const PageTable *) &fakePhysicalMemory[(totalMemoryFrames - 1) * PAGE_SIZE];
    const PageTable *pageTable3 = (const PageTable *) &fakePhysicalMemory[(totalMemoryFrames - 3) * PAGE_SIZE];
    const PageTable *pageTable25 = (const PageTable *) &fakePhysicalMemory[(totalMemoryFrames - 4) * PAGE_SIZE];
    ASSERT(frame2virt(frameNumber) == &fakePhysicalMemory[(totalMemoryFrames - 2) * PAGE_SIZE]);
    ASSERT(task.addressSpace.root == virt2phys(pageDirectory).v);
    ASSERT(task.addressSpace.tlbShootdownPageCount == 0);
    ASSERT(task.addressSpace.shootdownFrameListHead.next == &task.addressSpace.shootdownFrameListHead);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(pageDirectory->entries[i] == (
                i == 3 ? (virt2phys(pageTable3).v | ptPresent | ptWriteable | ptUser)
                : i == 25 ? (virt2phys(pageTable25).v | ptPresent | ptWriteable | ptUser)
                : i < 768 ? 0
                : i));
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(pageTable3->entries[i] == (
                i == 7 ? (frameNumber.v * PAGE_SIZE | ptPresent | ptWriteable | ptUser)
                : i == 8 ? (frameNumber.v * PAGE_SIZE | ptPresent | ptWriteable | ptUser)
                : 0));
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(pageTable25->entries[i] == (
                i == 16 ? (frameNumber.v * PAGE_SIZE | ptPresent | ptWriteable | ptUser)
                : i == 114 ? (frameNumber.v * PAGE_SIZE | ptPresent | ptWriteable | ptUser)
                : 0));
}

static void AddressSpaceTest_mapOutOfMemory() {
    const size_t totalMemoryFrames = 2;
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[totalMemoryFrames * PAGE_SIZE];
    Frame frames[totalMemoryFrames];
    initializePhysicalMemory(fakePhysicalMemory, frames, totalMemoryFrames);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        Boot_kernelPageDirectory.entries[i] = i;
    Task task;
    AddressSpace_initialize(&task);
    FrameNumber frameNumber = PhysicalMemory_allocate(&task, permamapMemoryRegion);

    int mapResult = AddressSpace_map(&task, makeVirtualAddress(0x200000), frameNumber);
    
    const PageTable *pageDirectory = (const PageTable *) &fakePhysicalMemory[(totalMemoryFrames - 1) * PAGE_SIZE];
    ASSERT(mapResult == -ENOMEM);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(pageDirectory->entries[i] == (i < 768 ? 0 : i));
}

static void AddressSpaceTest_mapOverAlreadyMapped() {
    const size_t totalMemoryFrames = 4;
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[totalMemoryFrames * PAGE_SIZE];
    Frame frames[totalMemoryFrames];
    initializePhysicalMemory(fakePhysicalMemory, frames, totalMemoryFrames);
    theFakeHardware = (FakeHardware) { .lastTlbInvalidationAddress = makeVirtualAddress(0), .tlbInvalidationCount = 0 };
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        Boot_kernelPageDirectory.entries[i] = i;
    Task task;
    AddressSpace_initialize(&task);
    FrameNumber previousFrameNumber = PhysicalMemory_allocate(&task, permamapMemoryRegion);
    FrameNumber newFrameNumber = PhysicalMemory_allocate(&task, permamapMemoryRegion);
    const VirtualAddress virtualAddress = makeVirtualAddress((3 << 22) | (7 << 12));
    AddressSpace_map(&task, virtualAddress, previousFrameNumber);

    int mapResult = AddressSpace_map(&task, virtualAddress, newFrameNumber);

    const PageTable *pageDirectory = (const PageTable *) &fakePhysicalMemory[(totalMemoryFrames - 1) * PAGE_SIZE];
    const PageTable *pageTable = (const PageTable *) &fakePhysicalMemory[(totalMemoryFrames - 4) * PAGE_SIZE];
    ASSERT(frame2virt(previousFrameNumber) == &fakePhysicalMemory[(totalMemoryFrames - 2) * PAGE_SIZE]);
    ASSERT(frame2virt(newFrameNumber) == &fakePhysicalMemory[(totalMemoryFrames - 3) * PAGE_SIZE]);
    ASSERT(mapResult == 0);
    ASSERT(task.addressSpace.root == virt2phys(pageDirectory).v);
    ASSERT(task.addressSpace.tlbShootdownPageCount == 1);
    ASSERT(task.addressSpace.shootdownFrameListHead.next == &frames[totalMemoryFrames - 2].node);
    ASSERT(frames[totalMemoryFrames - 2].node.next == &task.addressSpace.shootdownFrameListHead);
    ASSERT(theFakeHardware.tlbInvalidationCount == 0);
    ASSERT(theFakeHardware.lastTlbInvalidationAddress.v == virtualAddress.v);
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(pageDirectory->entries[i] == (
                i == 3 ? (virt2phys(pageTable).v | ptPresent | ptWriteable | ptUser)
                : i < 768 ? 0
                : i));
    for (size_t i = 0; i < PAGE_SIZE / sizeof(PageTableEntry); i++)
        ASSERT(pageTable->entries[i] == (
                i == 7 ? (newFrameNumber.v * PAGE_SIZE | ptPresent | ptWriteable | ptUser)
                : 0));
}

void AddressSpaceTest_run() {
    RUN_TEST(AddressSpaceTest_initialize);
    RUN_TEST(AddressSpaceTest_initializeOutOfMemory);
    RUN_TEST(AddressSpaceTest_map);
    RUN_TEST(AddressSpaceTest_mapMultiple);
    RUN_TEST(AddressSpaceTest_mapOutOfMemory);
    RUN_TEST(AddressSpaceTest_mapOverAlreadyMapped);
}
