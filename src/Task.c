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

Task *Task_create(Task *ownerTask, uintptr_t *cap) {
    Task *task = SlabAllocator_allocate(&taskAllocator);
    if (task == NULL) return NULL;
    task->ownerTask = ownerTask;
    if (AddressSpace_initialize(task) < 0) return NULL;
    SlabAllocator_initialize(&task->capabilitySpace, sizeof(Capability), task);
    task->threadCount = 0;
    if (ownerTask != NULL) {
        Capability *oc = Task_allocateCapability(ownerTask, (uintptr_t) task | kobjTask, 0);
        *cap = Task_getCapabilityAddress(oc);
    }
    Capability *sc = Task_allocateCapability(task, (uintptr_t) task | kobjTask, 0);
    return task;
}

/**
 * Allocates and initializes a new capability.
 * @param task Task to allocate the new capability for.
 * @param obj Address and type for the kernel object.
 * @param badge Optional badge for the capability, or 0 for an unbadged capability.
 * @return The new capability on success, or NULL on out of memory.
 */
Capability *Task_allocateCapability(Task *task, uintptr_t obj, uintptr_t badge) {
    Capability *cap = SlabAllocator_allocate(&task->capabilitySpace);
    if (UNLIKELY(cap == NULL)) return NULL;
    // TODO: mark the frame descriptor as slab allocator/capability space.
    cap->obj = obj;
    cap->badge = badge;
    cap->prev = cap;
    cap->next = cap;
    return cap;
}

/**
 * Looks up a capability in the capability space of the specified task.
 * @param task Task to look up the capability into.
 * @param index Index of the capability to look up.
 * @return Pointer to the index-th capability of the task, or NULL index exceeds the size of the capability space.
 */
Capability *Task_lookupCapability(Task *task, PhysicalAddress address) {
    FrameNumber frameNumber = floorToFrame(address);
    if (frameNumber.v >= PhysicalMemory_totalMemoryFrames) return NULL;
    Frame *frame = getFrame(frameNumber);
    if (frame->task != task || frame->virtualAddress.v != VIRTUAL_ADDRESS_CAPABILITY_SPACE.v) return NULL;
    return phys2virt(physicalAddress(address.v & ~0xF));
}

uintptr_t Task_getCapabilityAddress(const Capability *cap) {
    PhysicalAddress a = virt2phys(cap);
    assert((a.v & 0xF) == 0);
    return a.v;
}

void Task_deallocateCapability(Task *task, Capability *cap) {
    cap->prev->next = cap->next;
    cap->next->prev = cap->prev;
    cap->obj = 0;
    SlabAllocator_deallocate(&task->capabilitySpace, cap);
}
