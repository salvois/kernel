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
#ifndef BOOT_PHYSICALMEMORY_H_INCLUDED
#define BOOT_PHYSICALMEMORY_H_INCLUDED

#include "../PhysicalMemory.h"
#include "Multiboot.h"

void PhysicalMemoryRegion_initialize(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end);
void PhysicalMemoryRegion_add(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end);
void PhysicalMemoryRegion_remove(PhysicalMemoryRegion *pmr, FrameNumber begin, FrameNumber end);

void PhysicalMemory_add(PhysicalAddress begin, PhysicalAddress end);
void PhysicalMemory_remove(PhysicalAddress begin, PhysicalAddress end);
void PhysicalMemory_initializeRegions();
PhysicalAddress PhysicalMemory_findKernelEnd(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress imageEnd);
PhysicalAddress PhysicalMemory_findMultibootModulesEnd(const MultibootMbi *mbi);
PhysicalAddress PhysicalMemory_findPhysicalAddressUpperBound(const MultibootMbi *mbi);
void PhysicalMemory_addFreeMemoryBlocks(const MultibootMbi *mbi);
void PhysicalMemory_markInitialMemoryAsAllocated(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress kernelEnd);
void PhysicalMemory_initializeFromMultibootV1(const MultibootMbi *mbi, PhysicalAddress imageBegin, PhysicalAddress imageEnd);

#endif
