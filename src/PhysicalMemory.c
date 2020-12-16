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

/** Table of descriptors for each frame of all installed physical memory. */
Frame *PhysicalMemory_frameDescriptors;
/** Frame number of the first frame in PhysicalMemory_frameDescriptors, usually 0 except in tests. */
FrameNumber PhysicalMemory_firstFrame;
/** Number of frames of all installed physical memory. */
size_t PhysicalMemory_totalMemoryFrames;
/** Descriptors for each region specified by enum PhysicalMemoryRegionType. */
PhysicalMemoryRegion PhysicalMemory_regions[physicalMemoryRegionCount];
/** Dummy task to identify free frames. */
Task PhysicalMemory_freeFramesDummyOwner;


/********************************************************************
 * PhysicalMemoryRegion
 ********************************************************************/

FrameNumber PhysicalMemoryRegion_allocate(PhysicalMemoryRegion *pmr, Task *task) {
    if (UNLIKELY(pmr->freeFrameCount == 0))
        return frameNumber(0);
    Frame *frame = Frame_fromNode(pmr->freeListHead.next);
    assert(frame->task == &PhysicalMemory_freeFramesDummyOwner);
    frame->task = task;
    frame->virtualAddress = VIRTUAL_ADDRESS_UNMAPPED;
    LinkedList_remove(pmr->freeListHead.next);
    pmr->freeFrameCount--;
    return getFrameNumber(frame);
}

void PhysicalMemoryRegion_deallocate(PhysicalMemoryRegion *pmr, FrameNumber frameNumber) {
    Frame *frame = getFrame(frameNumber);
    assert(frame->task != &PhysicalMemory_freeFramesDummyOwner);
    frame->virtualAddress = VIRTUAL_ADDRESS_UNMAPPED;
    frame->task = &PhysicalMemory_freeFramesDummyOwner;
    LinkedList_insertAfter(&frame->node, &pmr->freeListHead);
    pmr->freeFrameCount++;
}


/******************************************************************************
 * PhysicalMemory
 ******************************************************************************/

FrameNumber PhysicalMemory_allocate(Task *task, PhysicalMemoryRegionType preferredRegion) {
    assert(preferredRegion < physicalMemoryRegionCount);
    for (int i = preferredRegion; i >= 0; i--) {
        FrameNumber frameNumber = PhysicalMemoryRegion_allocate(&PhysicalMemory_regions[i], task);
        if (frameNumber.v != 0) return frameNumber;
    }
    return frameNumber(0);
}

void PhysicalMemory_deallocate(FrameNumber frameNumber) {
    for (int i = physicalMemoryRegionCount - 1; i >= 0; i--) {
        PhysicalMemoryRegion *region = &PhysicalMemory_regions[i];
        if (frameNumber.v >= region->begin.v) {
            PhysicalMemoryRegion_deallocate(region, frameNumber);
            return;
        }
    }
    assert(false);
}
