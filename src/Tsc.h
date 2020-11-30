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
#ifndef TSC_H_INCLUDED
#define TSC_H_INCLUDED

#include "Types.h"
#include "hardware.h"

/** Converts the specified count of ticks of the TSC to nanoseconds. */
static inline uint64_t Tsc_convertTicksToNanoseconds(const Tsc *tsc, uint32_t ticks) {
    return mul(ticks, tsc->nsPerTick) >> 20;
}

__attribute__((section(".boot"))) void Tsc_initialize(Tsc *tsc);

#endif
