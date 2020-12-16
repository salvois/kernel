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

Pic8259 Pic8259_theInstance;

void Pic8259_unmask(unsigned irqNo) {
    if (irqNo < 8) {
        Pic8259_theInstance.masterMask &= ~(1 << irqNo);
        outp(0x21, Pic8259_theInstance.masterMask);
    } else if (irqNo < 16) {
        Pic8259_theInstance.slaveMask &= ~(1 << (irqNo - 8));
        outp(0xA1, Pic8259_theInstance.slaveMask);
    }
}

void Pic8259_mask(unsigned irqNo) {
    if (irqNo < 8) {
        Pic8259_theInstance.masterMask |= 1 << irqNo;
        outp(0x21, Pic8259_theInstance.masterMask);
    } else if (irqNo < 16) {
        Pic8259_theInstance.slaveMask |= 1 << (irqNo - 8);
        outp(0xA1, Pic8259_theInstance.slaveMask);
    }
}

static inline void Pic8259_sendEoiToMaster() {
    asm volatile("out %%al, $0x20" : : "a" (0x20) : "memory");
}

static inline void Pic8259_sendEoiToSlave() {
    asm volatile("out %%al, $0xA0" : : "a" (0x20) : "memory");
}

void Pic8259_sendEndOfInterrupt(unsigned irqNo) {
    if (irqNo > 7) Pic8259_sendEoiToSlave();
    Pic8259_sendEoiToMaster();
}

size_t Pic8259_getVectorNumber(unsigned irqNo) {
    if (irqNo < 8) return Pic8259_theInstance.masterVector + irqNo;
    if (irqNo < 16) return Pic8259_theInstance.slaveVector + irqNo;
    return (size_t) -1;
}
