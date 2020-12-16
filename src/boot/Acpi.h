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
#ifndef BOOT_ACPI_H_INCLUDED
#define BOOT_ACPI_H_INCLUDED

#include "stdint.h"
#include "../PhysicalMemory.h"
#include "../Acpi.h"

/** ACPI table for the Root System Descriptor Pointer. */
typedef struct AcpiRootSystemDescPointer {
    char signature[8]; // "RSD PTR "
    uint8_t checksum;
    char oemId[6];
    uint8_t revision;
    uint32_t rsdtPhysicalAddress;
    uint32_t length;
    uint64_t xsdtPhysicalAddress;
    uint8_t extendedChecksum;
    uint8_t reserved[3];
} __attribute__((packed)) AcpiRootSystemDescPointer;

/** ACPI table for the Root System Descriptor Table. */
typedef struct AcpiRootSystemDescTable {
    AcpiDescriptionHeader header; // signature='RSDT'
    uint32_t entries[]; // physical address of table entries, count derived from header.length
} __attribute__((packed)) AcpiRootSystemDescTable;

#define ACPI_RSDT_SIGNATURE 0x54445352
#define ACPI_FADT_SIGNATURE 0x50434146 // yes, this is actually FACP
#define ACPI_HPET_SIGNATURE 0x54455048

const AcpiRootSystemDescPointer *Acpi_doSearchRootSystemDescriptorPointer(PhysicalAddress begin, PhysicalAddress end);
void *Acpi_mapToTemporaryArea(PhysicalAddress address, int tempAreaIndex);
void Acpi_doFindConfig(PhysicalAddress rsdtPhysicalAddress, void *(*mapToTemporaryArea)(PhysicalAddress address, int tempAreaIndex));
void Acpi_findConfig();
void AcpiPmTimer_initialize();

#endif
