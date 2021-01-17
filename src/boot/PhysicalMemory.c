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

/********************************************************************
 * PhysicalMemoryRegion
 ********************************************************************/

__attribute__((section(".boot")))
void PhysicalMemoryRegion_initialize(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end) {
    assert(begin.v <= end.v);
    LinkedList_initialize(&pmr->freeListHead);
    pmr->begin = begin;
    pmr->end = end;
    pmr->freeFrameCount = 0;
}

__attribute__((section(".boot")))
void PhysicalMemoryRegion_add(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end) {
    assert(begin.v < end.v);
    size_t count = end.v - begin.v;
    for (size_t i = 0; i < count; i++) {
        Frame *frame = getFrame(addToFrameNumber(begin, i));
        if (Frame_getTask(frame) != &PhysicalMemory_freeFramesDummyOwner) {
            frame->virtualAddress = makeVirtualAddress(0);
            Frame_setTaskAndType(frame, &PhysicalMemory_freeFramesDummyOwner, FrameType_unmapped);
            LinkedList_insertAfter(&frame->node, &pmr->freeListHead);
            pmr->freeFrameCount++;
        }
    }
}

__attribute__((section(".boot")))
void PhysicalMemoryRegion_remove(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end) {
    assert(begin.v < end.v);
    size_t count = end.v - begin.v;
    for (size_t i = 0; i < count; i++) {
        Frame *frame = getFrame(addToFrameNumber(begin, i));
        if (Frame_getTask(frame) == &PhysicalMemory_freeFramesDummyOwner) {
            frame->virtualAddress = makeVirtualAddress(0);
            Frame_setTaskAndType(frame, NULL, FrameType_unmapped);
            LinkedList_remove(&frame->node);
            pmr->freeFrameCount--;
        }
    }
}


/******************************************************************************
 * PhysicalMemory
 ******************************************************************************/

__attribute__((section(".boot")))
void PhysicalMemory_initializeRegions() {
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[isadmaMemoryRegion], frameNumber(0), ISADMA_MEMORY_REGION_FRAME_END);
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[permamapMemoryRegion], ISADMA_MEMORY_REGION_FRAME_END, PERMAMAP_MEMORY_REGION_FRAME_END);
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[otherMemoryRegion], PERMAMAP_MEMORY_REGION_FRAME_END, frameNumber(UINT32_MAX));
}

__attribute__((section(".boot")))
void PhysicalMemory_setFrameDescriptors(void *addr) {
    PhysicalMemory_frameDescriptors = addr;
    FrameNumber end = virt2frame((void *) ((uintptr_t) addr + PhysicalMemory_totalMemoryFrames * sizeof(Frame)));
    if (end.v >= PERMAMAP_MEMORY_REGION_FRAME_END.v || end.v >= PhysicalMemory_totalMemoryFrames)
        panic("Unable to store the frame descriptor table. Aborting.\n");
    Log_printf("Frame descriptor table at %p (%u bytes) for %u frames of memory.\n",
               addr, PhysicalMemory_totalMemoryFrames * sizeof(Frame), PhysicalMemory_totalMemoryFrames);
}

__attribute__((section(".boot")))
void PhysicalMemory_add(PhysicalAddress begin, PhysicalAddress end) {
    FrameNumber beginFrame = floorToFrame(begin);
    FrameNumber endFrame = ceilToFrame(end);
    for (int i = 0; i < physicalMemoryRegionCount; i++) {
        FrameNumber b = (beginFrame.v >= PhysicalMemory_regions[i].begin.v) ? beginFrame : PhysicalMemory_regions[i].begin;
        FrameNumber e = (endFrame.v   <= PhysicalMemory_regions[i].end.v)   ? endFrame   : PhysicalMemory_regions[i].end;
        if (b.v < e.v)
            PhysicalMemoryRegion_add(&PhysicalMemory_regions[i], b, e);
        if (e.v == end.v) break;
    }
}

__attribute__((section(".boot")))
void PhysicalMemory_remove(PhysicalAddress begin, PhysicalAddress end) {
    FrameNumber beginFrame = floorToFrame(begin);
    FrameNumber endFrame = ceilToFrame(end);
    for (int i = 0; i < physicalMemoryRegionCount; i++) {
        FrameNumber b = (beginFrame.v >= PhysicalMemory_regions[i].begin.v) ? beginFrame : PhysicalMemory_regions[i].begin;
        FrameNumber e = (endFrame.v   <= PhysicalMemory_regions[i].end.v)   ? endFrame   : PhysicalMemory_regions[i].end;
        if (b.v < e.v)
            PhysicalMemoryRegion_remove(&PhysicalMemory_regions[i], b, e);
        if (e.v == end.v) break;
    }
}

__attribute__((section(".boot")))
static void PhysicalMemory_findKernelEnd_callback(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end) {
    PhysicalAddress *kernelEnd = (PhysicalAddress *) closure;
    PhysicalAddress sectionEnd = end;
    if (/*(sh->sh_flags & 2) &&*/ (sectionEnd.v > kernelEnd->v))
        *kernelEnd = sectionEnd;
}

__attribute__((section(".boot")))
PhysicalAddress PhysicalMemory_findKernelEnd(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress imageEnd) {
    PhysicalAddress kernelEnd = imageEnd;
    MultibootMbi_scanElfSections(mbi, &kernelEnd, PhysicalMemory_findKernelEnd_callback);
    Log_printf("imageBegin=%p imageEnd=%p kernelEnd=%p\n", imageBegin.v, imageEnd.v, kernelEnd.v);
    return kernelEnd;
}

__attribute__((section(".boot")))
static void PhysicalMemory_findMultibootModulesEnd_callback(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end, PhysicalAddress name) {
    PhysicalAddress *allModulesEnd = (PhysicalAddress *) closure;
    if (end.v > allModulesEnd->v) *allModulesEnd = end;
}

__attribute__((section(".boot")))
PhysicalAddress PhysicalMemory_findMultibootModulesEnd(const MultibootMbi *mbi) {
    PhysicalAddress allModulesEnd = physicalAddress(0);
    MultibootMbi_scanModules(mbi, &allModulesEnd, PhysicalMemory_findMultibootModulesEnd_callback);
    return allModulesEnd;
}

__attribute__((section(".boot")))
static void PhysicalMemory_findPhysicalAddressUpperBound_callback(void *closure, uint64_t begin, uint64_t length, int type) {
    if (type != 1) return;
    uint64_t *physicalAddressUpperBound = (uint64_t *) closure;
    uint64_t end = begin + length;
    if (end > *physicalAddressUpperBound) *physicalAddressUpperBound = end;
}

__attribute__((section(".boot")))
PhysicalAddress PhysicalMemory_findPhysicalAddressUpperBound(const MultibootMbi *mbi) {
    uint64_t physicalAddressUpperBound = 0;
    MultibootMbi_scanMemoryMap(mbi, &physicalAddressUpperBound, PhysicalMemory_findPhysicalAddressUpperBound_callback);
    if (physicalAddressUpperBound > MAX_PHYSICAL_ADDRESS)
        physicalAddressUpperBound = MAX_PHYSICAL_ADDRESS;
    if (physicalAddressUpperBound == 0)
        panic("Memory information not available.\nSystem halted.\n");
    return physicalAddress(physicalAddressUpperBound);
}

__attribute__((section(".boot")))
static void PhysicalMemory_addFreeMemoryBlocks_callback(void *closure, uint64_t begin, uint64_t length, int type) {
    if (type != 1 || begin >= MAX_PHYSICAL_ADDRESS) return;
    uint64_t end = begin + length;
    if (end > MAX_PHYSICAL_ADDRESS) end = MAX_PHYSICAL_ADDRESS;
    PhysicalMemory_add(physicalAddress(begin), physicalAddress(end));
}

__attribute__((section(".boot")))
void PhysicalMemory_addFreeMemoryBlocks(const MultibootMbi *mbi) {
    MultibootMbi_scanMemoryMap(mbi, NULL, PhysicalMemory_addFreeMemoryBlocks_callback);
}

__attribute__((section(".boot")))
static void PhysicalMemory_markInitialMemoryAsAllocated_callback(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end, PhysicalAddress name) {
    PhysicalMemory_remove(begin, end);
}

__attribute__((section(".boot")))
void PhysicalMemory_markInitialMemoryAsAllocated(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress kernelEnd) {
    PhysicalMemory_remove(physicalAddress(0), physicalAddress(PAGE_SIZE));
    PhysicalMemory_remove(imageBegin, kernelEnd);
    MultibootMbi_scanModules(mbi, NULL, PhysicalMemory_markInitialMemoryAsAllocated_callback);
}

__attribute__((section(".boot")))
void PhysicalMemory_initializeFromMultibootV1(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress imageEnd) {
    Log_printf("Initializing the physical memory manager from the MBIv1 at %p.\n", mbi);
    PhysicalAddress kernelEnd = PhysicalMemory_findKernelEnd(mbi, imageBegin, imageEnd);
    PhysicalAddress multibootModulesEnd = PhysicalMemory_findMultibootModulesEnd(mbi);
    PhysicalAddress firstFreeAddress = multibootModulesEnd.v > kernelEnd.v ? multibootModulesEnd : kernelEnd;
    PhysicalAddress physicalAddressUpperBound = PhysicalMemory_findPhysicalAddressUpperBound(mbi);

    PhysicalMemory_totalMemoryFrames = physicalAddressUpperBound.v >> PAGE_SHIFT;
    PhysicalMemory_setFrameDescriptors(frame2virt(ceilToFrame(firstFreeAddress)));
    PhysicalMemory_addFreeMemoryBlocks(mbi);
    PhysicalMemory_markInitialMemoryAsAllocated(mbi, imageBegin, kernelEnd);

    PhysicalAddress frameDescriptorBegin = virt2phys(PhysicalMemory_frameDescriptors);
    PhysicalAddress frameDescriptorEnd = addToPhysicalAddress(frameDescriptorBegin, PhysicalMemory_totalMemoryFrames * sizeof(Frame));
    PhysicalMemory_remove(frameDescriptorBegin, frameDescriptorEnd);
}
