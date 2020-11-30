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
#ifndef TASK_H_INCLUDED
#define TASK_H_INCLUDED

#include "Types.h"

struct Task {
    Task          *ownerTask;
    AddressSpace   addressSpace;
    SlabAllocator  capabilitySpace;
    size_t         threadCount;
};

/**
 * Dummy union to check the size of the Task struct.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union TaskChecker {
    char wrongSize[(sizeof(Task) & 0xF) == 0];
}; 

Task       *Task_create(Task *ownerTask, uintptr_t *cap);
Capability *Task_allocateCapability(Task *task, uintptr_t obj, uintptr_t badge);
Capability *Task_lookupCapability(Task *task, PhysicalAddress address);
uintptr_t   Task_getCapabilityAddress(const Capability *cap);
void        Task_deallocateCapability(Task *task, Capability *cap);

#endif
