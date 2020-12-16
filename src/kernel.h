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
#ifndef KERNEL_H_INCLUDED
#define	KERNEL_H_INCLUDED

#include "Types.h"
#include "hardware.h"
#include "boot/Acpi.h"
#include "boot/Cpu.h"
#include "boot/LapicTimer.h"
#include "boot/MultiProcessorSpecification.h"
#include "boot/Multiboot.h"
#include "boot/Pic8259.h"
#include "boot/PhysicalMemory.h"
#include "Acpi.h"
#include "AddressSpace.h"
#include "Cpu.h"
#include "CpuNode.h"
#include "ElfLoader.h"
#include "Formatter.h"
#include "LapicTimer.h"
#include "PhysicalMemory.h"
#include "Pic8259.h"
#include "PriorityQueue.h"
#include "SlabAllocator.h"
#include "Spinlock.h"
#include "Task.h"
#include "Thread.h"
#include "Tsc.h"

#endif
