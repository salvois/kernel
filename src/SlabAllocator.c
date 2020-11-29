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

SlabAllocator taskAllocator;
SlabAllocator threadAllocator;
SlabAllocator channelAllocator;

/**
 * Initializes the specified SlabAllocator for a specified kind of element.
 * @param sa Allocator to initialize.
 * @param itemSize Size in bytes of each element managed by the allocator (must be multiple of a proper alignment and 16 bytes).
 */
void SlabAllocator_initialize(SlabAllocator *sa, size_t itemSize, Task *task) {
    assert((itemSize & 0xF) == 0);
    sa->freeItems = NULL;
    sa->itemSize = itemSize;
    sa->itemsPerSlab = PAGE_SIZE / itemSize;
    sa->brk = NULL;
    sa->limit = NULL;
    sa->task = task;
}

/**
 * Allocates an element from the slab allocator.
 * @param sa Allocator to allocate from.
 * @return A pointer to the element, or NULL on failure.
 */
void *SlabAllocator_allocate(SlabAllocator *sa) {
    assert(sa->brk <= sa->limit);
    void *freeItem = (void *) sa->freeItems;
    if (freeItem != NULL) {
        sa->freeItems = sa->freeItems->next;
    } else {
        if (UNLIKELY(sa->brk == sa->limit)) { // includes the case for brk == limit == NULL for an empty allocator
            FrameNumber frameNumber = PhysicalMemory_allocate(sa->task, permamapMemoryRegion);
            if (UNLIKELY(frameNumber.v == 0)) return NULL;
            void *page = frame2virt(frameNumber);
            sa->brk = (uint8_t *) page;
            sa->limit = (uint8_t *) page + sa->itemSize * sa->itemsPerSlab;
        }
        freeItem = sa->brk;
        sa->brk += sa->itemSize;
    }
    return freeItem;
}

/**
 * Releases an element to the appropriate slab allocator.
 * @param sa Allocator to release the item to.
 * @param item Item to deallocate (undefined behavior if not allocated from the specified allocator).
 */
void SlabAllocator_deallocate(SlabAllocator *sa, void *item) {
    SlabAllocator_FreeItem* fi = (SlabAllocator_FreeItem *) item;
    fi->next = sa->freeItems;
    sa->freeItems = fi;
}
