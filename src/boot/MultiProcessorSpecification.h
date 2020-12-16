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
#ifndef BOOT_MULTIPROCESSORSPECIFICATION_H_INCLUDED
#define BOOT_MULTIPROCESSORSPECIFICATION_H_INCLUDED

#include "stdint.h"
#include "../PhysicalMemory.h"

typedef struct MpFloatingPointer {
    uint32_t signature; // "_MP_" = 0x5F504D5F
    uint32_t mpConfigPhysicalAddress; // unused if defaultConfig > 0
    uint8_t mpFloatingPointerSize; // in 16-byte units, must be 1
    uint8_t version; // 1: 1.1, 4: 1.4
    uint8_t checksum; // the sum of all bytes in this structure must be 0
    uint8_t defaultConfig; // 0: no default config, >0: one of the standard defaults
    uint8_t imcrp; // bit 7: PIC mode implemented, bit 6-0: reserved
    uint8_t reserved[3]; // must be 0
} MpFloatingPointer;

typedef struct MpConfigHeader {
    uint32_t signature; // "PCMP"
    uint16_t baseSize; // size in bytes of the base configuration table
    uint8_t version; // 1: 1.1, 4: 1.4
    uint8_t checksum; // the sum of all bytes in the base structure must be 0
    char oemId[8]; // space padded
    char productId[12]; // space padded
    uint32_t oemTablePhysicalAddress; // may be 0
    uint16_t oemTableSize; // may be 0
    uint16_t entryCount; // number of entries in the base configuration table
    uint32_t lapicPhysicalAddress;
    uint16_t extendedSize; // size in bytes of the extended configuration table
    uint8_t extendedChecksum;
    uint8_t reserved;
} MpConfigHeader;

enum MpConfigEntryType {
    mpConfigProcessor = 0,
    mpConfigBus = 1,
    mpConfigIoapic = 2,
    mpConfigIoint = 3,
    mpConfigLocalint = 4
};

struct MpConfigEntry {
    uint8_t entryType; // one of MpConfigEntryType
};

typedef struct MpConfigProcessor { // extends MpConfigEntry
    uint8_t entryType; // mpConfigProcessor
    uint8_t lapicId;
    uint8_t lapicVersion;
    uint8_t flags; // bit 0: CPU enabled, bit 1: is boot processor
    uint32_t cpuSignature;
    uint32_t featureFlags;
    uint32_t reserved[2];
} MpConfigProcessor;

typedef struct MpConfigBus { // extends MpConfigEntry
    uint8_t entryType; // mpConfigBus
    uint8_t busId;
    char busType[6];
} MpConfigBus;

typedef struct MpConfigIoapic { // extends MpConfigEntry
    uint8_t entryType; // mpConfigIoapic
    uint8_t ioapicId;
    uint8_t ioapicVersion;
    uint8_t flags; // bit 0: IOAPIC enabled
    uint32_t ioapicPhysicalAddress;
} MpConfigIoapic;

#define MPFLOATINGPOINTER_SIGNATURE 0x5F504D5F // "_MP_"

const MpFloatingPointer *MultiProcessorSpecification_searchMpFloatingPointer(PhysicalAddress begin, PhysicalAddress end);
const MpConfigHeader *MultiProcessorSpecification_searchFindMpConfig();
void MultiProcessorSpecification_scanProcessors(const MpConfigHeader *mpConfigHeader, void *closure, void (*callback)(void *closure, int lapicId));

#endif
