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
#ifndef ACPI_H_INCLUDED
#define ACPI_H_INCLUDED

#include "Types.h"

/** Frequency in Hz of the ACPI Power Management timer. */
#define ACPI_PMTIMER_FREQUENCY 3579545

typedef struct AcpiDescriptionHeader {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemId[6];
    char oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
} __attribute__((packed)) AcpiDescriptionHeader;

typedef struct AcpiAddress {
    uint8_t addressSpaceId; // 0=system memory, 1=system I/O
    uint8_t registerBitWidth;
    uint8_t registerBitOffset;
    uint8_t reserved;
    uint64_t address;
} __attribute__((packed)) AcpiAddress;

typedef struct AcpiFadt {
    AcpiDescriptionHeader header; // signature='FACP'
    uint32_t firmwareCtrlPhysicalAddress;
    uint32_t dsdtPhysicalAddress;
    uint8_t reserved1;
    uint8_t preferredPmProfile;
    uint16_t sciInt;
    uint32_t smiCmd;
    uint8_t acpiEnable;
    uint8_t acpiDisable;
    uint8_t s4biosReq;
    uint8_t pstateCnt;
    uint32_t pm1aEvtBlk;
    uint32_t pm1bEvtBlk;
    uint32_t pm1aCntBlk;
    uint32_t pm1bCntBlk;
    uint32_t pm2CntBlk;
    uint32_t pmTmrBlk; // The 32-bit physical address of the Power Management Timer
    uint32_t gpe0Blk;
    uint32_t gpe1Blk;
    uint8_t pm1EvtLen;
    uint8_t pm1CntLen;
    uint8_t pm2CntLen;
    uint8_t pmTmrLen; // Must be 4
    uint8_t gpe0BlkLen;
    uint8_t gpe1BlkLen;
    uint8_t gpe1Base;
    uint8_t cstCnt;
    uint16_t pLvl2Lat;
    uint16_t pLvl3Lat;
    uint16_t flushSize;
    uint16_t flushStride;
    uint8_t dutyOffset;
    uint8_t dutyWidth;
    uint8_t dayAlrm;
    uint8_t monAlrm;
    uint8_t century;
    uint16_t iapcBootArch;
    uint8_t reserved2;
    uint32_t flags;
    AcpiAddress resetReg;
    uint8_t resetValue;
    uint8_t reserved3[3];
    // Extended fields omitted
} __attribute__((packed)) AcpiFadt;

extern AcpiFadt Acpi_fadt;

typedef struct AcpiPmTimer {
    uint64_t timerMax; // 0x01000000 if timer is 24-bit, 0x100000000 if timer is 32-bit
    uint64_t timerMaxNanoseconds; //
    uint64_t cumulativeNanoseconds;
    uint32_t prevCount;
} AcpiPmTimer;

__attribute__((section(".boot"))) void Acpi_findConfig();

/** Reads the raw value of the ACPI Power Management timer. */
static inline uint32_t Acpi_readPmTimer() {
    return inpd(Acpi_fadt.pmTmrBlk);
}

void AcpiPmTimer_initialize();
unsigned AcpiPmTimer_busyWait(unsigned ticks);

#endif
