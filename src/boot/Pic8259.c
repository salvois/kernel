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

/**
 * Reprograms the PICs to use custom base interrupt vectors.
 * @param masterVector Interrupt vector of the first IRQ of master PIC (IRQ 0, BIOS default is INT 8), must be multiple of 8.
 * @param slaveVector Interrupt vector of the first IRQ of slave PIC (IRQ 8, BIOS default is INT 0x70), must be multiple of 8.
 *
 * Allows to reprogram the PICs so that their interrupt vectors do not conflict
 * with IA-32 reserved vectors (that are 0 to 31). In fact, the default BIOS mapping
 * of the master PIC (on vector 8) does conflict.
 * To restore BIOS defaults, reprogram with vectors 8 and 0x70 for master and slave respectively.
 */
__attribute__((section(".boot")))
void Pic8259_initialize(uint8_t masterVector, uint8_t slaveVector) {
    assert((masterVector & 3) == 0);
    assert((slaveVector & 3) == 0);
    Pic8259_theInstance.masterVector = masterVector;
    Pic8259_theInstance.masterMask = 0xFB; // all IRQs but IR 2 masked (disabled)
    Pic8259_theInstance.slaveVector = slaveVector;
    Pic8259_theInstance.slaveMask = 0xFF; // all IRQs masked (disabled)

    outp(0x20, 0x11); // ICW1: edge triggered, cascade, ICW4 present
    outp(0x21, masterVector); // ICW2: base interrupt vector
    outp(0x21, 0x04); // ICW3: a slave is connected on IR 2
    outp(0x21, 0x01); // ICW4: not special fully nested, non buffered, manual EOI, 8086 mode
    outp(0x21, Pic8259_theInstance.masterMask); // OCW1

    outp(0xA0, 0x11); // ICW1: edge triggered, cascade, ICW4 present
    outp(0xA1, slaveVector); // ICW2: base interrupt vector
    outp(0xA1, 0x02); // ICW3: connected on IR 2 of the master
    outp(0xA1, 0x01); // ICW4: not special fully nested, non buffered, manual EOI, 8086 mode
    outp(0xA1, Pic8259_theInstance.slaveMask); // OCW1
}
