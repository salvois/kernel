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
#ifndef PIC8259_H_INCLUDED
#define PIC8259_H_INCLUDED

#include "stdint.h"

typedef struct Pic8259 {
    uint8_t masterVector;
    uint8_t masterMask;
    uint8_t slaveVector;
    uint8_t slaveMask;
} Pic8259;

extern Pic8259 Pic8259_theInstance;

void Pic8259_unmask(unsigned irqNo);
void Pic8259_mask(unsigned irqNo);
void Pic8259_sendEndOfInterrupt(unsigned irqNo);
size_t Pic8259_getVectorNumber(unsigned irqNo);

#endif
