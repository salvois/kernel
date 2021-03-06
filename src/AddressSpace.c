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

#if 0
#define ADDRESSSPACE_LOG_PRINTF(...) Log_printf(__VA_ARGS__)
#else
#define ADDRESSSPACE_LOG_PRINTF(...)
#endif

/*
 * 32-bit 2 levels (4 KiB page): 0xFFC00000 shift 22, 0x3FF000 shift 12
 * 32-bit 1 level (4 MiB page): 0xFFC00000 shift 22
 * 32-bit PAE 3 levels (4 KiB page): 0xC0000000 shift 30, 0x3FE00000 shift 21, 0x1FF000 shift 12
 * 32-bit PAE 2 levels (2 MiB page): 0xC0000000 shift 30, 0x3FE00000 shift 21
 * 64-bit 4 levels (4 KiB page): 0xFF8000000000 shift 39, 0x7FC0000000 shift 30, 0x3FE00000 shift 21, 0x1FF000 shift 12
 * 64-bit 3 levels (2 MiB page): 0xFF8000000000 shift 39, 0x7FC0000000 shift 30, 0x3FE00000 shift 21
 * 64-bit 2 levels (1 GiB page): 0xFF8000000000 shift 39, 0x7FC0000000 shift 30
 */
#define ADDRESSSPACE_HEIGHT 1
#define PAGE_TABLE_SHIFT 10
#define PAGE_TABLE_LENGTH (1 << PAGE_TABLE_SHIFT)

static PageTable *resolvePageTableEntry(PageTableEntry pte) {
    return phys2virt(physicalAddress(pte & ~(PAGE_SIZE - 1)));
}

/**
 * Creates an address space made only of the empty top page table.
 * @param task Task to attach the new address space to.
 * @return 0 on success, or a negative error code.
 */
int AddressSpace_initialize(Task *task) {
    FrameNumber rootFrame = PhysicalMemory_allocate(task, permamapMemoryRegion);
    if (rootFrame.v == 0) return -ENOMEM;
    PageTable *pd = frame2virt(rootFrame);
    memzero(pd, PAGE_SIZE - 1024);
    memcpy(&pd->entries[768], &Boot_kernelPageDirectory.entries[768], 1024);
    task->addressSpace.root = rootFrame.v << PAGE_SHIFT;
    task->addressSpace.tlbShootdownPageCount = 0;
    LinkedList_initialize(&task->addressSpace.shootdownFrameListHead);
    return 0;
}

/**
 * Adds the specified frame to the set of frames not yet ready for reuse (shootdown pending).
 * @param task Task the mapping is being removed from.
 * @param virtualAddress Virtual address the mapping is being removed from.
 * @param frame Frame number of the frame being unmapped.
 */
static void AddressSpace_enqueueShootdownFrame(Task *task, VirtualAddress virtualAddress, FrameNumber frameNumber) {
    if (task->addressSpace.tlbShootdownPageCount < TASK_MAX_TLB_SHOOTDOWN_PAGES) {
        task->addressSpace.tlbShootdownPages[task->addressSpace.tlbShootdownPageCount] = virtualAddress;
    }
    task->addressSpace.tlbShootdownPageCount++;
    Frame *f = getFrame(frameNumber);
    LinkedList_insertBefore(&f->node, &task->addressSpace.shootdownFrameListHead);
}

static PageTable* AddressSpace_findLeaf(Task *task, VirtualAddress virtualAddress) {
    PageTable *table = resolvePageTableEntry(task->addressSpace.root);
    int height = ADDRESSSPACE_HEIGHT;
    while (height > 0) {
        size_t subindex = virtualAddress.v >> ((height - 1) * PAGE_TABLE_SHIFT + PAGE_SHIFT) & (PAGE_TABLE_LENGTH - 1);
        if ((table->entries[subindex] & ptPresent) == 0) return NULL;
        table = resolvePageTableEntry(table->entries[subindex]);
        height--;
    }
    return table;
}

static PageTable *AddressSpace_findLeafAllocating(Task *task, VirtualAddress virtualAddress) {
    PageTable *table = resolvePageTableEntry(task->addressSpace.root);
    int height = ADDRESSSPACE_HEIGHT;
    while (height > 0) {
        size_t subindex = virtualAddress.v >> (height * PAGE_TABLE_SHIFT + PAGE_SHIFT) & (PAGE_TABLE_LENGTH - 1);
        if ((table->entries[subindex] & ptPresent) == 0) {
            FrameNumber frameNumber = PhysicalMemory_allocate(task, permamapMemoryRegion);
            if (frameNumber.v == 0) return NULL;
            PageTable *subtable = frame2virt(frameNumber);
            memzero(subtable, sizeof(PageTable));
            table->entries[subindex] = frameNumber.v << PAGE_SHIFT | ptPresent | ptWriteable | ptUser;
        }
        table = resolvePageTableEntry(table->entries[subindex]);
        height--;
    }
    return table;
}

static void AddressSpace_initiateTlbShootdown(Task *task) {
    if (task->addressSpace.tlbShootdownPageCount < TASK_MAX_TLB_SHOOTDOWN_PAGES) {
        for (size_t i = 0; i < task->addressSpace.tlbShootdownPageCount; i++) {
            AddressSpace_invalidateTlbAddress(task->addressSpace.tlbShootdownPages[i]);
        }
    } else {
        AddressSpace_invalidateTlb();
    }
    if (task->threadCount > 1) {
        for (size_t j = 0; j < Cpu_cpuCount; j++) {
            if (Cpu_cpus[j]->currentThread->task == task) {
                Cpu_sendTlbShootdownIpi(Cpu_cpus[j]);
            }
        }
    }
}

/**
 * Map a frame number to a user virtual address.
 * @param task Task to map the page into.
 * @param virtualAddress Virtual address to map into.
 * @param frame Frame number to map.
 * @return 0 on success, or a negative error code.
 */
int AddressSpace_map(Task *task, VirtualAddress virtualAddress, FrameNumber fn) {
    ADDRESSSPACE_LOG_PRINTF("Mapping virtual address %p to frame %p for task %p (address space root=%p).\n", virtualAddress, fn.v, task, task->addressSpace.root);
    PageTable *pt = AddressSpace_findLeafAllocating(task, virtualAddress);
    if (pt == NULL) return -ENOMEM;
    size_t index = virtualAddress.v >> PAGE_SHIFT & (PAGE_TABLE_LENGTH - 1);
    if (pt->entries[index] & ptPresent) {
        AddressSpace_enqueueShootdownFrame(task, virtualAddress, frameNumber(pt->entries[index] >> PAGE_SHIFT));
    }
    pt->entries[index] = fn.v << PAGE_SHIFT | ptPresent | ptWriteable | ptUser;
    if (task->addressSpace.tlbShootdownPageCount > 0) {
        AddressSpace_initiateTlbShootdown(task);
    }
    return 0;
}

/**
 * Map pages from pages of a possibly different address space.
 * @param destTask Task to map pages into.
 * @param destBeginVirt Virtual address to start mapping into.
 * @param srcTask Task to map pages from.
 * @param srcBeginVirt Virtual address to start mapping from.
 * @param count Number of pages to map.
 * @return 0 on success, or a negative error code.
 */
int AddressSpace_mapCopy(Task *destTask, VirtualAddress destVirt, Task *srcTask, VirtualAddress srcVirt) {
    PageTable *srcPt = AddressSpace_findLeaf(srcTask, srcVirt);
    if (srcPt == NULL) return -EFAULT;
    size_t srcIndex = srcVirt.v >> PAGE_SHIFT & (PAGE_TABLE_LENGTH - 1);
    PageTable *destPt = AddressSpace_findLeafAllocating(destTask, destVirt);
    if (destPt == NULL) return -ENOMEM;
    size_t destIndex = destVirt.v >> PAGE_SHIFT & (PAGE_TABLE_LENGTH - 1);
    if (destPt->entries[destIndex] & ptPresent) {
        AddressSpace_enqueueShootdownFrame(destTask, destVirt, frameNumber(destPt->entries[destIndex] >> PAGE_SHIFT));
    }
    destPt->entries[destIndex] = srcPt->entries[srcIndex];
    if (destTask->addressSpace.tlbShootdownPageCount > 0) {
        AddressSpace_initiateTlbShootdown(destTask);
    }
    return 0;
}

// TODO: Map pages from frame capabilities

/**
 * Map a page from a newly allocated frame.
 * @param task Task to map the page into.
 * @param virtualAddress Virtual address to mapping into.
 * @return 0 on success, or a negative error code.
 */
int AddressSpace_mapFromNewFrame(Task *task, VirtualAddress virtualAddress, PhysicalMemoryRegionType preferredRegion) {
    FrameNumber frameNumber = PhysicalMemory_allocate(task, preferredRegion);
    if (frameNumber.v == 0) return -ENOMEM;
//    memzero(frame2virt(frame), PAGE_SIZE);
    int res = AddressSpace_map(task, virtualAddress, frameNumber);
    if (res < 0) PhysicalMemory_deallocate(frameNumber);
    return res;
}

//Allocate frames and return frame capabilities


/**
 * Unmap a page.
 * @param task Task to unmap the page from.
 * @param virtualAddress Virtual address to unmap.
 * @param payload Word to insert into the non-present page (e.g. for swapping). Bit 0 must be clear.
 */
void AddressSpace_unmap(Task *task, VirtualAddress virtualAddress, uintptr_t payload) {
    assert((payload & ptPresent) == 0);
    size_t index = virtualAddress.v >> PAGE_SHIFT & (PAGE_TABLE_LENGTH - 1);
    PageTable *pt = AddressSpace_findLeaf(task, virtualAddress);
    if (pt == NULL) return;
    if (pt->entries[index] & ptPresent) {
        AddressSpace_enqueueShootdownFrame(task, virtualAddress, frameNumber(pt->entries[index] >> PAGE_SHIFT));
    }
    pt->entries[index] = payload;
    if (task->addressSpace.tlbShootdownPageCount > 0) {
        AddressSpace_initiateTlbShootdown(task);
    }
}

//TODO: Deallocate frame capabilities
//TODO: Unmap pages and deallocate frame capabilities
//TODO: Copy memory block possibly across address spaces
