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
#ifndef PHYSICALMEMORY_H_INCLUDED
#define PHYSICALMEMORY_H_INCLUDED

#include "Types.h"
#include "Multiboot.h"

/** log2 of the size in bytes of a memory page or page frame. */
#define PAGE_SHIFT 12
/** Size in bytes of a memory page or page frame. */
#define PAGE_SIZE (1UL << PAGE_SHIFT)
/** Virtual address the kernel space begin at. */
#define HIGH_HALF_BEGIN 0xC0000000u
/** Highest possible address of physical memory usable for ISA DMA. */
#define ISADMA_MEMORY_REGION_FRAME_END ((FrameNumber) { 16 << 20 >> PAGE_SHIFT })
/** Highest possible address of permanently mapped physical memory. */
#define PERMAMAP_MEMORY_REGION_FRAME_END ((FrameNumber) { 896 << 20 >> PAGE_SHIFT })
/** The highest possible address for physical memory. */
#define MAX_PHYSICAL_ADDRESS 0xFFFFF000u // 4 GiB - 4096 bytes

/** Types of independently managed physical memory regions. */
typedef enum PhysicalMemoryRegionType {
    /** Memory with low addresses that can be used for ISA DMA. */
    isadmaMemoryRegion,
    /** Memory that is 1:1 permanently mapped in the higher-half virtual address
     * space, with an offset, for data that the kernel needs to access directly. */
    permamapMemoryRegion,
    /** All other memory, for user mode data. */
    otherMemoryRegion,
    /** Number of supported physical memory regions. */
    physicalMemoryRegionCount
} PhysicalMemoryRegionType;

typedef struct PhysicalMemoryRegion {
    LinkedList_Node freeListHead;
    FrameNumber begin;
    FrameNumber end;
    size_t freeFrameCount;
} PhysicalMemoryRegion;

/** Descriptor of a physical memory frame. */
typedef struct Frame {
    Task *task;
    VirtualAddress virtualAddress;
    LinkedList_Node node;
} Frame;

#define VIRTUAL_ADDRESS_UNMAPPED ((VirtualAddress) { 0 })
#define VIRTUAL_ADDRESS_CAPABILITY_SPACE ((VirtualAddress) { 1 })

extern Frame *PhysicalMemory_frameDescriptors;
extern FrameNumber PhysicalMemory_firstFrame;
extern size_t PhysicalMemory_totalMemoryFrames;
extern PhysicalMemoryRegion PhysicalMemory_regions[physicalMemoryRegionCount];

static inline VirtualAddress makeVirtualAddress(uintptr_t v) { return (VirtualAddress) { v }; }
static inline PhysicalAddress physicalAddress(uintptr_t v) { return (PhysicalAddress) { v }; }
static inline PhysicalAddress addToPhysicalAddress(PhysicalAddress pa, ptrdiff_t d) { return (PhysicalAddress) { pa.v + d }; }
static inline FrameNumber frameNumber(uintptr_t v) { return (FrameNumber) { v }; }
static inline FrameNumber addToFrameNumber(FrameNumber fn, ptrdiff_t d) { return (FrameNumber) { fn.v + d }; }

static inline Frame *Frame_fromNode(LinkedList_Node *node) {
    return (Frame *) ((uint8_t *) node - offsetof(Frame, node));
}

static inline FrameNumber getFrameNumber(const Frame *frame) {
    return frameNumber((uintptr_t) (frame - PhysicalMemory_frameDescriptors) + PhysicalMemory_firstFrame.v);
}

static inline Frame *getFrame(FrameNumber frameNumber) {
    return &PhysicalMemory_frameDescriptors[frameNumber.v - PhysicalMemory_firstFrame.v];
}

/** Rounds the specified physical address down to a frame number. */
static inline FrameNumber floorToFrame(PhysicalAddress phys) {
    return frameNumber(phys.v >> PAGE_SHIFT);
}

/** Rounds the specified physical address up to a frame number. */
static inline FrameNumber ceilToFrame(PhysicalAddress phys) {
    return frameNumber((phys.v + PAGE_SIZE - 1) >> PAGE_SHIFT);
}

/** Rounds the specified physical address up to a frame number. */
static inline PhysicalAddress frame2phys(FrameNumber frameNumber) {
    return physicalAddress(frameNumber.v << PAGE_SHIFT);
}

/** Returns the virtual address of the specified permanently mapped physical address. */
static inline void *phys2virt(PhysicalAddress phys) {
    return (void*) ( phys.v + HIGH_HALF_BEGIN );
}

/** Returns the physical address of the specified permanently mapped virtual address. */
static inline PhysicalAddress virt2phys(const void *virt) {
    return physicalAddress((uintptr_t) virt - HIGH_HALF_BEGIN);
}

/** Returns the virtual address of the specified permanently mapped frame number. */
static inline void *frame2virt(FrameNumber frameNumber) {
    return phys2virt(physicalAddress(frameNumber.v << PAGE_SHIFT));
}

/** Returns the frame number of the specified permanently mapped virtual address. */
static inline FrameNumber virt2frame(void *virt) {
    return frameNumber(virt2phys(virt).v >> PAGE_SHIFT);
}

void PhysicalMemoryRegion_initialize(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end);
void PhysicalMemoryRegion_add(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end);
void PhysicalMemoryRegion_remove(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end);
FrameNumber PhysicalMemoryRegion_allocate(PhysicalMemoryRegion *pmr, Task *task);
void PhysicalMemoryRegion_deallocate(PhysicalMemoryRegion *pmr, FrameNumber frameNumber);

void PhysicalMemory_add(PhysicalAddress begin, PhysicalAddress end);
void PhysicalMemory_remove(PhysicalAddress begin, PhysicalAddress end);
void PhysicalMemory_initializeRegions();
PhysicalAddress PhysicalMemory_findKernelEnd(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress imageEnd);
PhysicalAddress PhysicalMemory_findMultibootModulesEnd(const MultibootMbi *mbi);
PhysicalAddress PhysicalMemory_findPhysicalAddressUpperBound(const MultibootMbi *mbi);
void PhysicalMemory_addFreeMemoryBlocks(const MultibootMbi *mbi);
void PhysicalMemory_markInitialMemoryAsAllocated(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress kernelEnd);
void PhysicalMemory_initializeFromMultibootV1(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress imageEnd);

FrameNumber PhysicalMemory_allocate(Task *task, PhysicalMemoryRegionType preferredRegion);
void PhysicalMemory_deallocate(FrameNumber frameNumber);

#endif
