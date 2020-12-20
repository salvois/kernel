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

/******************************************************************************
 * PhysicalMemoryRegion
 ******************************************************************************/

static void PhysicalMemoryRegionTest_allocate() {
    PhysicalMemoryRegion region;
    PhysicalMemoryRegion_initialize(&region, frameNumber(10), frameNumber(30));
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemoryRegion_add(&region, frameNumber(20), frameNumber(22));
    Task task;
    
    FrameNumber frameNumber = PhysicalMemoryRegion_allocate(&region, &task);
    
    ASSERT(frameNumber.v == 21);
    ASSERT(region.freeFrameCount == 1);
    ASSERT(region.freeListHead.next == &frames[20].node);
    ASSERT(frames[20].node.next == &region.freeListHead);
}

static void PhysicalMemoryRegionTest_allocateOutOfMemory() {
    PhysicalMemoryRegion region;
    PhysicalMemoryRegion_initialize(&region, frameNumber(10), frameNumber(30));
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemoryRegion_add(&region, frameNumber(20), frameNumber(22));
    Task task;
    for (int i = 0; i < 2; i++)
        PhysicalMemoryRegion_allocate(&region, &task);

    FrameNumber frameNumber = PhysicalMemoryRegion_allocate(&region, &task);
    
    ASSERT(frameNumber.v == 0);
    ASSERT(region.freeFrameCount == 0);
    ASSERT(region.freeListHead.next == &region.freeListHead);
}

static void PhysicalMemoryRegionTest_deallocate() {
    PhysicalMemoryRegion region;
    PhysicalMemoryRegion_initialize(&region, frameNumber(10), frameNumber(30));
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemoryRegion_add(&region, frameNumber(20), frameNumber(22));
    Task task;
    FrameNumber frameNumber = PhysicalMemoryRegion_allocate(&region, &task);
    PhysicalMemoryRegion_allocate(&region, &task);
    
    PhysicalMemoryRegion_deallocate(&region, frameNumber);
    
    ASSERT(frameNumber.v == 21);
    ASSERT(region.freeFrameCount == 1);
    ASSERT(region.freeListHead.next == &frames[21].node);
    ASSERT(frames[21].node.next == &region.freeListHead);
}


/******************************************************************************
 * PhysicalMemory
 ******************************************************************************/

static void PhysicalMemoryTest_allocate() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(10));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(15), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_add(physicalAddress(0), physicalAddress(30 * PAGE_SIZE));
    Task task;
    
    FrameNumber frameNumber = PhysicalMemory_allocate(&task, 1);

    ASSERT(frameNumber.v == 19);
    
    ASSERT(PhysicalMemory_regions[0].freeFrameCount == 10);
    ASSERT(PhysicalMemory_regions[0].freeListHead.next == &frames[9].node);
    ASSERT(PhysicalMemory_regions[0].freeListHead.prev == &frames[0].node);

    ASSERT(PhysicalMemory_regions[1].freeFrameCount == 4);
    ASSERT(PhysicalMemory_regions[1].freeListHead.next == &frames[18].node);
    ASSERT(PhysicalMemory_regions[1].freeListHead.prev == &frames[15].node);

    ASSERT(PhysicalMemory_regions[2].freeFrameCount == 10);
    ASSERT(PhysicalMemory_regions[2].freeListHead.next == &frames[29].node);
    ASSERT(PhysicalMemory_regions[2].freeListHead.prev == &frames[20].node);
}

static void PhysicalMemoryTest_allocatePreferredRegionExhausted() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(10));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(15), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_add(physicalAddress(0), physicalAddress(30 * PAGE_SIZE));
    Task task;
    for (int i = 0; i < 5; i++)
        PhysicalMemory_allocate(&task, 1);

    FrameNumber frameNumber = PhysicalMemory_allocate(&task, 1);

    ASSERT(frameNumber.v == 9);
    
    ASSERT(PhysicalMemory_regions[0].freeFrameCount == 9);
    ASSERT(PhysicalMemory_regions[0].freeListHead.next == &frames[8].node);
    ASSERT(PhysicalMemory_regions[0].freeListHead.prev == &frames[0].node);

    ASSERT(PhysicalMemory_regions[1].freeFrameCount == 0);
    ASSERT(PhysicalMemory_regions[1].freeListHead.next == &PhysicalMemory_regions[1].freeListHead);
    ASSERT(PhysicalMemory_regions[1].freeListHead.prev == &PhysicalMemory_regions[1].freeListHead);

    ASSERT(PhysicalMemory_regions[2].freeFrameCount == 10);
    ASSERT(PhysicalMemory_regions[2].freeListHead.next == &frames[29].node);
    ASSERT(PhysicalMemory_regions[2].freeListHead.prev == &frames[20].node);
}

static void PhysicalMemoryTest_allocatePreferredRegionAndLowerRegionsExhausted() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(10));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(15), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_add(physicalAddress(0), physicalAddress(30 * PAGE_SIZE));
    Task task;
    for (int i = 0; i < 15; i++)
        PhysicalMemory_allocate(&task, 1);

    FrameNumber frameNumber = PhysicalMemory_allocate(&task, 1);

    ASSERT(frameNumber.v == 0);
    
    ASSERT(PhysicalMemory_regions[0].freeFrameCount == 0);
    ASSERT(PhysicalMemory_regions[0].freeListHead.next == &PhysicalMemory_regions[0].freeListHead);
    ASSERT(PhysicalMemory_regions[0].freeListHead.prev == &PhysicalMemory_regions[0].freeListHead);

    ASSERT(PhysicalMemory_regions[1].freeFrameCount == 0);
    ASSERT(PhysicalMemory_regions[1].freeListHead.next == &PhysicalMemory_regions[1].freeListHead);
    ASSERT(PhysicalMemory_regions[1].freeListHead.prev == &PhysicalMemory_regions[1].freeListHead);

    ASSERT(PhysicalMemory_regions[2].freeFrameCount == 10);
    ASSERT(PhysicalMemory_regions[2].freeListHead.next == &frames[29].node);
    ASSERT(PhysicalMemory_regions[2].freeListHead.prev == &frames[20].node);
}

static void PhysicalMemoryTest_deallocate() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(10));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(15), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_add(physicalAddress(0), physicalAddress(30 * PAGE_SIZE));
    Task task;
    FrameNumber frameNumber = PhysicalMemory_allocate(&task, 1);
    PhysicalMemory_allocate(&task, 1);
    
    PhysicalMemory_deallocate(frameNumber);

    ASSERT(frameNumber.v == 19);
    
    ASSERT(PhysicalMemory_regions[1].freeFrameCount == 4);
    ASSERT(PhysicalMemory_regions[1].freeListHead.next == &frames[19].node);
    ASSERT(frames[19].node.next == &frames[17].node);
    ASSERT(PhysicalMemory_regions[1].freeListHead.prev == &frames[15].node);
}

void PhysicalMemoryTest_run() {
    RUN_TEST(PhysicalMemoryRegionTest_allocate);
    RUN_TEST(PhysicalMemoryRegionTest_allocateOutOfMemory);
    RUN_TEST(PhysicalMemoryRegionTest_deallocate);
    RUN_TEST(PhysicalMemoryTest_allocate);
    RUN_TEST(PhysicalMemoryTest_allocatePreferredRegionExhausted);
    RUN_TEST(PhysicalMemoryTest_allocatePreferredRegionAndLowerRegionsExhausted);
    RUN_TEST(PhysicalMemoryTest_deallocate);
}
