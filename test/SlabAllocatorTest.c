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

static void SlabAllocatorTest_initialize() {
    Task task;
    SlabAllocator allocator;
    const size_t itemSize = 48;
    SlabAllocator_initialize(&allocator, itemSize, &task);
    const size_t expectedItemsPerSlab = PAGE_SIZE / itemSize;
    ASSERT(allocator.freeItems == NULL);
    ASSERT(allocator.itemSize == itemSize);
    ASSERT(allocator.itemsPerSlab == expectedItemsPerSlab);
    ASSERT(allocator.brk == NULL);
    ASSERT(allocator.limit == NULL);
    ASSERT(allocator.task == &task);
}

static void SlabAllocatorTest_allocateFirst() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    Frame frames[30];
    initializePhysicalMemory(fakePhysicalMemory, frames, 1);
    Task task;
    SlabAllocator allocator;
    const size_t itemSize = 48;
    SlabAllocator_initialize(&allocator, itemSize, &task);

    void *item = SlabAllocator_allocate(&allocator);
    
    const size_t expectedItemsPerSlab = PAGE_SIZE / itemSize;
    ASSERT(Frame_getTask(&frames[0]) == &task);
    ASSERT(allocator.freeItems == NULL);
    ASSERT(allocator.itemSize == itemSize);
    ASSERT(allocator.itemsPerSlab == expectedItemsPerSlab);
    ASSERT(allocator.brk == &fakePhysicalMemory[itemSize]);
    ASSERT(allocator.limit == &fakePhysicalMemory[expectedItemsPerSlab * itemSize]);
    ASSERT(allocator.task == &task);
    ASSERT(item == &fakePhysicalMemory[0]);
}

static void SlabAllocatorTest_allocateSecond() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    Frame frames[30];
    initializePhysicalMemory(fakePhysicalMemory, frames, 1);
    Task task;
    SlabAllocator allocator;
    const size_t itemSize = 48;
    SlabAllocator_initialize(&allocator, itemSize, &task);
    SlabAllocator_allocate(&allocator);
        
    void *item = SlabAllocator_allocate(&allocator);
    
    const size_t expectedItemsPerSlab = PAGE_SIZE / itemSize;
    ASSERT(Frame_getTask(&frames[0]) == &task);
    ASSERT(allocator.freeItems == NULL);
    ASSERT(allocator.itemSize == itemSize);
    ASSERT(allocator.itemsPerSlab == expectedItemsPerSlab);
    ASSERT(allocator.brk == &fakePhysicalMemory[2 * itemSize]);
    ASSERT(allocator.limit == &fakePhysicalMemory[expectedItemsPerSlab * itemSize]);
    ASSERT(allocator.task == &task);
    ASSERT(item == &fakePhysicalMemory[48]);
}

static void SlabAllocatorTest_allocateBeyondPage() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[2 * PAGE_SIZE];
    Frame frames[30];
    initializePhysicalMemory(fakePhysicalMemory, frames, 2);
    Task task;
    SlabAllocator allocator;
    const size_t itemSize = 48;
    const size_t expectedItemsPerSlab = PAGE_SIZE / itemSize;
    SlabAllocator_initialize(&allocator, itemSize, &task);
    for (size_t i = 0; i < expectedItemsPerSlab; i++)
        SlabAllocator_allocate(&allocator);
        
    void *item = SlabAllocator_allocate(&allocator);
    
    ASSERT(Frame_getTask(&frames[0]) == &task);
    ASSERT(Frame_getTask(&frames[1]) == &task);
    ASSERT(allocator.freeItems == NULL);
    ASSERT(allocator.itemSize == itemSize);
    ASSERT(allocator.itemsPerSlab == expectedItemsPerSlab);
    ASSERT(allocator.brk == &fakePhysicalMemory[itemSize]);
    ASSERT(allocator.limit == &fakePhysicalMemory[expectedItemsPerSlab * itemSize]);
    ASSERT(allocator.task == &task);
    ASSERT(item == &fakePhysicalMemory[0]);
}

static void SlabAllocatorTest_deallocateFirst() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    Frame frames[30];
    initializePhysicalMemory(fakePhysicalMemory, frames, 1);
    Task task;
    SlabAllocator allocator;
    const size_t itemSize = 48;
    SlabAllocator_initialize(&allocator, itemSize, &task);
    void *item = SlabAllocator_allocate(&allocator);
        
    SlabAllocator_deallocate(&allocator, item);
    
    const size_t expectedItemsPerSlab = PAGE_SIZE / itemSize;
    ASSERT(Frame_getTask(&frames[0]) == &task);
    ASSERT(allocator.freeItems == (SlabAllocator_FreeItem *) &fakePhysicalMemory[0]);
    ASSERT(allocator.freeItems->next == NULL);
    ASSERT(allocator.itemSize == itemSize);
    ASSERT(allocator.itemsPerSlab == expectedItemsPerSlab);
    ASSERT(allocator.brk == &fakePhysicalMemory[itemSize]);
    ASSERT(allocator.limit == &fakePhysicalMemory[expectedItemsPerSlab * itemSize]);
    ASSERT(allocator.task == &task);
}

static void SlabAllocatorTest_deallocateSecond() {
    __attribute__ ((aligned(PAGE_SIZE))) uint8_t fakePhysicalMemory[PAGE_SIZE];
    Frame frames[30];
    initializePhysicalMemory(fakePhysicalMemory, frames, 1);
    Task task;
    SlabAllocator allocator;
    const size_t itemSize = 48;
    SlabAllocator_initialize(&allocator, itemSize, &task);
    void *firstItem = SlabAllocator_allocate(&allocator);
    void *secondItem = SlabAllocator_allocate(&allocator);
        
    SlabAllocator_deallocate(&allocator, firstItem);
    SlabAllocator_deallocate(&allocator, secondItem);
    
    const size_t expectedItemsPerSlab = PAGE_SIZE / itemSize;
    ASSERT(Frame_getTask(&frames[0]) == &task);
    ASSERT(allocator.freeItems == (SlabAllocator_FreeItem *) &fakePhysicalMemory[itemSize]);
    ASSERT(allocator.freeItems->next == (SlabAllocator_FreeItem *) &fakePhysicalMemory[0]);
    ASSERT(allocator.freeItems->next->next == NULL);
    ASSERT(allocator.itemSize == itemSize);
    ASSERT(allocator.itemsPerSlab == expectedItemsPerSlab);
    ASSERT(allocator.brk == &fakePhysicalMemory[2 * itemSize]);
    ASSERT(allocator.limit == &fakePhysicalMemory[expectedItemsPerSlab * itemSize]);
    ASSERT(allocator.task == &task);
}

void SlabAllocatorTest_run() {
    RUN_TEST(SlabAllocatorTest_initialize);
    RUN_TEST(SlabAllocatorTest_allocateFirst);
    RUN_TEST(SlabAllocatorTest_allocateSecond);
    RUN_TEST(SlabAllocatorTest_allocateBeyondPage);
    RUN_TEST(SlabAllocatorTest_deallocateFirst);
    RUN_TEST(SlabAllocatorTest_deallocateSecond);
}
