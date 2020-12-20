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

static void Boot_PhysicalMemoryRegionTest_initialize() {
    PhysicalMemoryRegion region;
    PhysicalMemoryRegion_initialize(&region, frameNumber(1000), frameNumber(2000));
    ASSERT(region.freeListHead.next == &region.freeListHead);
    ASSERT(region.freeListHead.prev == &region.freeListHead);
    ASSERT(region.begin.v == 1000);
    ASSERT(region.end.v == 2000);
    ASSERT(region.freeFrameCount == 0);
}

static void Boot_PhysicalMemoryRegionTest_add() {
    PhysicalMemoryRegion region;
    PhysicalMemoryRegion_initialize(&region, frameNumber(10), frameNumber(30));
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemory_frameDescriptors = frames;
    
    PhysicalMemoryRegion_add(&region, frameNumber(20), frameNumber(22));
    
    ASSERT(region.freeFrameCount == 2);
    ASSERT(region.freeListHead.next == &frames[21].node);
    ASSERT(frames[21].node.next == &frames[20].node);
    ASSERT(frames[20].node.next == &region.freeListHead);
}

static void Boot_PhysicalMemoryRegionTest_remove() {
    PhysicalMemoryRegion region;
    PhysicalMemoryRegion_initialize(&region, frameNumber(10), frameNumber(30));
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemoryRegion_add(&region, frameNumber(20), frameNumber(30));
    
    PhysicalMemoryRegion_remove(&region, frameNumber(27), frameNumber(29));
    
    ASSERT(region.freeFrameCount == 8);
    ASSERT(region.freeListHead.next == &frames[29].node);
    ASSERT(frames[29].node.next == &frames[26].node);
}


/******************************************************************************
 * PhysicalMemory
 ******************************************************************************/

static void Boot_PhysicalMemoryTest_add() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(10));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(15), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    PhysicalMemory_frameDescriptors = frames;
    
    PhysicalMemory_add(physicalAddress(0), physicalAddress(30 * PAGE_SIZE));
    
    ASSERT(PhysicalMemory_regions[0].freeFrameCount == 10);
    ASSERT(PhysicalMemory_regions[0].freeListHead.next == &frames[9].node);
    ASSERT(PhysicalMemory_regions[0].freeListHead.prev == &frames[0].node);

    ASSERT(PhysicalMemory_regions[1].freeFrameCount == 5);
    ASSERT(PhysicalMemory_regions[1].freeListHead.next == &frames[19].node);
    ASSERT(PhysicalMemory_regions[1].freeListHead.prev == &frames[15].node);

    ASSERT(PhysicalMemory_regions[2].freeFrameCount == 10);
    ASSERT(PhysicalMemory_regions[2].freeListHead.next == &frames[29].node);
    ASSERT(PhysicalMemory_regions[2].freeListHead.prev == &frames[20].node);
}

static void Boot_PhysicalMemoryTest_addUnaligned() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(10));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(15), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    PhysicalMemory_frameDescriptors = frames;
    
    PhysicalMemory_add(physicalAddress(16), physicalAddress(29 * PAGE_SIZE + 16));
    
    ASSERT(PhysicalMemory_regions[0].freeFrameCount == 10);
    ASSERT(PhysicalMemory_regions[0].freeListHead.next == &frames[9].node);
    ASSERT(PhysicalMemory_regions[0].freeListHead.prev == &frames[0].node);

    ASSERT(PhysicalMemory_regions[1].freeFrameCount == 5);
    ASSERT(PhysicalMemory_regions[1].freeListHead.next == &frames[19].node);
    ASSERT(PhysicalMemory_regions[1].freeListHead.prev == &frames[15].node);

    ASSERT(PhysicalMemory_regions[2].freeFrameCount == 10);
    ASSERT(PhysicalMemory_regions[2].freeListHead.next == &frames[29].node);
    ASSERT(PhysicalMemory_regions[2].freeListHead.prev == &frames[20].node);
}

static void Boot_PhysicalMemoryTest_remove() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(10));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(15), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_add(physicalAddress(0), physicalAddress(30 * PAGE_SIZE));
    
    PhysicalMemory_remove(physicalAddress(9 * PAGE_SIZE), physicalAddress(16 * PAGE_SIZE));
    
    ASSERT(PhysicalMemory_regions[0].freeFrameCount == 9);
    ASSERT(PhysicalMemory_regions[0].freeListHead.next == &frames[8].node);
    ASSERT(PhysicalMemory_regions[0].freeListHead.prev == &frames[0].node);

    ASSERT(PhysicalMemory_regions[1].freeFrameCount == 4);
    ASSERT(PhysicalMemory_regions[1].freeListHead.next == &frames[19].node);
    ASSERT(PhysicalMemory_regions[1].freeListHead.prev == &frames[16].node);
}

static void Boot_PhysicalMemoryTest_removeUnaligned() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(10));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(15), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_add(physicalAddress(0), physicalAddress(30 * PAGE_SIZE));
    
    PhysicalMemory_remove(physicalAddress(9 * PAGE_SIZE + 16), physicalAddress(15 * PAGE_SIZE + 16));
    
    ASSERT(PhysicalMemory_regions[0].freeFrameCount == 9);
    ASSERT(PhysicalMemory_regions[0].freeListHead.next == &frames[8].node);
    ASSERT(PhysicalMemory_regions[0].freeListHead.prev == &frames[0].node);

    ASSERT(PhysicalMemory_regions[1].freeFrameCount == 4);
    ASSERT(PhysicalMemory_regions[1].freeListHead.next == &frames[19].node);
    ASSERT(PhysicalMemory_regions[1].freeListHead.prev == &frames[16].node);
}

static void Boot_PhysicalMemoryTest_initializeFromMultibootV1() {
    Frame frames[30];
    memzero(frames, sizeof(frames));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], frameNumber(0),  frameNumber(7));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], frameNumber(10), frameNumber(20));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], frameNumber(20), frameNumber(30));
    MultibootMemoryMap memoryMaps[3] = {
        (MultibootMemoryMap) { .size = 20, .base_addr_low =  0 * PAGE_SIZE, .length_low =  7 * PAGE_SIZE, .type = 1 },
        (MultibootMemoryMap) { .size = 20, .base_addr_low =  7 * PAGE_SIZE, .length_low =  3 * PAGE_SIZE, .type = 3 },
        (MultibootMemoryMap) { .size = 20, .base_addr_low = 10 * PAGE_SIZE, .length_low = 18 * PAGE_SIZE, .type = 1 }
    };
    Elf32_Shdr sectionHeaders[1] = {
        (Elf32_Shdr) { .sh_addr = 12 * PAGE_SIZE, .sh_size = PAGE_SIZE },
    };
    MultibootModule modules[3] = {
        (MultibootModule) { .mod_start = 15 * PAGE_SIZE, .mod_end = 16 * PAGE_SIZE, .string = 1 },
    };
    MultibootMbi mbi = {
        .flags = MULTIBOOTMBI_MEMORY_MAP_PROVIDED | MULTIBOOTMBI_ELF_SECTIONS_PROVIDED | MULTIBOOTMBI_MODULES_PROVIDED,
        .mmap_length = 3 * sizeof(MultibootMemoryMap),
        .mmap_addr = virt2phys(memoryMaps).v,
        .mods_count = 3,
        .mods_addr = virt2phys(modules).v,
        .syms.elf.num = 1,
        .syms.elf.size = sizeof(Elf32_Shdr),
        .syms.elf.addr = virt2phys(sectionHeaders).v
    };

    PhysicalAddress kernelEnd = PhysicalMemory_findKernelEnd(&mbi, physicalAddress(11 * PAGE_SIZE), physicalAddress(12 * PAGE_SIZE));
    PhysicalAddress multibootModulesEnd = PhysicalMemory_findMultibootModulesEnd(&mbi);
    PhysicalAddress physicalAddressUpperBound = PhysicalMemory_findPhysicalAddressUpperBound(&mbi);
    PhysicalMemory_totalMemoryFrames = physicalAddressUpperBound.v >> PAGE_SHIFT;
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_addFreeMemoryBlocks(&mbi);
    PhysicalMemory_markInitialMemoryAsAllocated(&mbi, physicalAddress(11 * PAGE_SIZE), kernelEnd);

    const bool availableFrames[30] = {
        false, true, true, true, true, true, true, false, false, false,
        true, false, false, true, true, false, true, true, true, true,
        true, true, true, true, true, true, true, true, false, false
    };
    ASSERT(kernelEnd.v == 13 * PAGE_SIZE);
    ASSERT(multibootModulesEnd.v == 16 * PAGE_SIZE);
    ASSERT(physicalAddressUpperBound.v == 28 * PAGE_SIZE);
    for (size_t i = 0; i < 30; i++)
        ASSERT((!availableFrames[i] && frames[i].task == NULL) || (availableFrames[i] && frames[i].task != NULL));
}


void Boot_PhysicalMemoryTest_run() {
    RUN_TEST(Boot_PhysicalMemoryRegionTest_initialize);
    RUN_TEST(Boot_PhysicalMemoryRegionTest_add);
    RUN_TEST(Boot_PhysicalMemoryRegionTest_remove);
    RUN_TEST(Boot_PhysicalMemoryTest_add);
    RUN_TEST(Boot_PhysicalMemoryTest_addUnaligned);
    RUN_TEST(Boot_PhysicalMemoryTest_remove);
    RUN_TEST(Boot_PhysicalMemoryTest_removeUnaligned);
    RUN_TEST(Boot_PhysicalMemoryTest_initializeFromMultibootV1);
}
