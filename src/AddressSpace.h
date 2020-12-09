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
#ifndef ADDRESSSPACE_H_INCLUDED
#define ADDRESSSPACE_H_INCLUDED

#include "Types.h"
#include "PhysicalMemory.h"

/*
 * On 32-bit higher half mode, the virtual address space is mapped as following in the kernel:
 * [0xC0000000, 0xF8000000) maps all physical memory from [0x00000000, 0x3800000) 896 MiB
 * [0xF8000000, 0xF8800000) temporary mapping area #1, 8 MiB
 * [0xF8800000, 0xF9000000) temporary mapping area #2, 8 MiB
 * ...
 * [0xFE000000, 0xFE800000) temporary mapping area #13, 8 MiB
 * [0xFEC00000, 0xFEC01000) I/O APIC
 * [0xFEE00000, 0xFEE01000) Local APIC
 */

typedef struct PageTable {
    PageTableEntry entries[PAGE_SIZE / sizeof(PageTableEntry)];
} PageTable;

extern PageTable Boot_kernelPageDirectory;

/** Flags of a page table entry (any level). */
enum PageTableFlags {
    ptPresent = 1 << 0,
    ptWriteable = 1 << 1,
    ptUser = 1 << 2,
    ptWriteThrough = 1 << 3,
    ptCacheDisable = 1 << 4,
    ptAccessed = 1 << 5,
    ptDirty = 1 << 6,
    ptLargePage = 1 << 7,
    ptPat = 1 << 7,
    ptGlobal = 1 << 8,
    ptIgnored = 7 << 9
};

static inline VirtualAddress AddressSpace_getTemporaryMappingArea(int index) {
    return makeVirtualAddress(0xF8000000 + 0x800000 * index);
}

int  AddressSpace_initialize(Task *task);
int  AddressSpace_map(Task *task, VirtualAddress virtualAddress, FrameNumber frameNumber);
int  AddressSpace_mapCopy(Task *destTask, VirtualAddress destVirt, Task *srcTask, VirtualAddress srcVirt);
int  AddressSpace_mapFromNewFrame(Task *task, VirtualAddress virtualAddress, PhysicalMemoryRegionType preferredRegion);
void AddressSpace_unmap(Task *task, VirtualAddress virtualAddress, uintptr_t payload);

#endif
