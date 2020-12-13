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

__attribute((section(".boot")))
const MpFloatingPointer *MultiProcessorSpecification_searchMpFloatingPointer(PhysicalAddress begin, PhysicalAddress end) {
    Log_printf("Searching for the MP Floating Pointer Structure at physical address [%p, %p).\n", begin, end);
    assert((begin.v & 0xF) == 0); // the structure must be aligned to a 16-byte boundary
    for ( ; begin.v < end.v; begin.v += 16) {
        const MpFloatingPointer *mpfp = phys2virt(begin);
        if (mpfp->signature == MPFLOATINGPOINTER_SIGNATURE) {
            uint8_t checksum = 0;
            const uint8_t *base = (const uint8_t *) mpfp;
            for (size_t i = 0; i < mpfp->mpFloatingPointerSize * 16; i++) {
                checksum += base[i];
            }
            if (checksum == 0) return mpfp;
        }
    }
    return NULL;
}

__attribute((section(".boot")))
const MpConfigHeader *MultiProcessorSpecification_searchFindMpConfig() {
    uint16_t ebdaRealModeSegment = *(const uint16_t *) phys2virt(physicalAddress(0x40E));
    PhysicalAddress ebda = physicalAddress((uintptr_t) ebdaRealModeSegment << 4);
    const MpFloatingPointer *mpfp = MultiProcessorSpecification_searchMpFloatingPointer(ebda, addToPhysicalAddress(ebda, 1024));
    if (mpfp == NULL) mpfp = MultiProcessorSpecification_searchMpFloatingPointer(physicalAddress(654336),  physicalAddress(655360));
    if (mpfp == NULL) mpfp = MultiProcessorSpecification_searchMpFloatingPointer(physicalAddress(523264),  physicalAddress(524288));
    if (mpfp == NULL) mpfp = MultiProcessorSpecification_searchMpFloatingPointer(physicalAddress(0xF0000), physicalAddress(0x100000));
    if (mpfp == NULL) {
        Log_printf("MP Floating Pointer Structure not found.\n");
        return NULL;
    }
    Log_printf("MP Floating Pointer Structure version %i found at %p. MP Config at %p, checksum=0x%02X, defaultConfig=0x%02X, imrcp=0x%02X.\n",
            mpfp->version, mpfp, mpfp->mpConfigPhysicalAddress,
            mpfp->checksum, mpfp->defaultConfig, mpfp->imcrp);
    if (mpfp->mpConfigPhysicalAddress == 0) {
        Log_printf("MP Config structure not found.\n");
        return NULL;
    }
    return phys2virt(physicalAddress(mpfp->mpConfigPhysicalAddress));
}

__attribute__((section(".boot")))
void MultiProcessorSpecification_scanProcessors(const MpConfigHeader *mpConfigHeader, void *closure, void (*callback)(void *closure, int lapicId)) {
    // "Processor" entries are guaranteed to be the first ones
    const MpConfigProcessor *cp = (const MpConfigProcessor *) ((const uint8_t *) mpConfigHeader + sizeof(MpConfigHeader));
    for (size_t i = 0; i < mpConfigHeader->entryCount && cp->entryType == mpConfigProcessor; i++, cp++)
        if ((cp->flags & 1) != 0)
            callback(closure, cp->lapicId);
}
