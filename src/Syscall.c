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

int Syscall_createChannel(Task *task) {
    Channel *channel = SlabAllocator_allocate(&channelAllocator);
    if (channel == NULL) return -ENOMEM;
    Capability *cap = Task_allocateCapability(task, (uintptr_t) channel | kobjChannel, 0);
    if (cap == NULL) {
        SlabAllocator_deallocate(&channelAllocator, channel);
        return -ENOMEM;
    }
    return Task_getCapabilityAddress(cap);
}

int Syscall_deleteCapability(Task *task, CapabilityAddress index) {
    Capability *cap = Task_lookupCapability(task, index);
    if (cap == NULL) return -EINVAL;
    KobjType kobjType = Capability_getObjectType(cap);
    if (kobjType == kobjNone) return -EINVAL;
    if (cap->next == cap) {
        switch (kobjType) {
            default:
                assert(false);
        }
    }
    Task_deallocateCapability(task, cap);
    return 0;
}
