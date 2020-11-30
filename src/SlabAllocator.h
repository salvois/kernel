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
#ifndef SLABALLOCATOR_H_INCLUDED
#define SLABALLOCATOR_H_INCLUDED

#include "Types.h"

void  SlabAllocator_initialize(SlabAllocator *sa, size_t itemSize, Task *task);
void *SlabAllocator_allocate  (SlabAllocator *sa);
void  SlabAllocator_deallocate(SlabAllocator *sa, void *item);

extern SlabAllocator taskAllocator;
extern SlabAllocator threadAllocator;
extern SlabAllocator channelAllocator;

#endif
