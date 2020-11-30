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

int  AddressSpace_initialize(Task *task);
int  AddressSpace_map(Task *task, uintptr_t virtualAddress, FrameNumber frameNumber);
int  AddressSpace_mapCopy(Task *destTask, uintptr_t destVirt, Task *srcTask, uintptr_t srcVirt);
int  AddressSpace_mapFromNewFrame(Task *task, uintptr_t virtualAddress, PhysicalMemoryRegionType preferredRegion);
void AddressSpace_unmap(Task *task, uintptr_t virtualAddress, uintptr_t payload);

#endif
