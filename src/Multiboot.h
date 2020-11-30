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
#ifndef MULTIBOOT_H_INCLUDED
#define MULTIBOOT_H_INCLUDED

#include "Types.h"

typedef struct MultibootModule {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} MultibootModule;

typedef struct MultibootMemoryMap {
    uint32_t size; // not including 'size' itself
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
} MultibootMemoryMap;

typedef struct MultibootMbi {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr; // physical address of an array of Module structures
    union {
        struct AoutSymbolTable {
            uint32_t tabsize;
            uint32_t strsize;
            uint32_t addr;
            uint32_t reserved;
        } aout;

        struct ElfSectionHeaderTable {
            uint32_t num;
            uint32_t size;
            uint32_t addr;
            uint32_t shndx;
        } elf;
    } syms;
    uint32_t mmap_length;
    uint32_t mmap_addr; // physical address of an array of MemoryMap structures
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
} MultibootMbi;

#define MULTIBOOTMBI_MEMLOWER_MEMUPPER_PROVIDED (1 << 0)
#define MULTIBOOTMBI_MODULES_PROVIDED (1 << 3)
#define MULTIBOOTMBI_ELF_SECTIONS_PROVIDED (1 << 5)
#define MULTIBOOTMBI_MEMORY_MAP_PROVIDED (1 << 6)

static inline bool MultibootMbi_areMemLowerAndMemUpperProvided(const MultibootMbi *mbi) {
    return mbi->flags & MULTIBOOTMBI_MEMLOWER_MEMUPPER_PROVIDED;
}

static inline bool MultibootMbi_areModulesProvided(const MultibootMbi *mbi) {
    return mbi->flags & MULTIBOOTMBI_MODULES_PROVIDED;
}

static inline bool MultibootMbi_areElfSectionsProvided(const MultibootMbi *mbi) {
    return mbi->flags & MULTIBOOTMBI_ELF_SECTIONS_PROVIDED;
}

static inline bool MultibootMbi_isMemoryMapProvided(const MultibootMbi *mbi) {
    return mbi->flags & MULTIBOOTMBI_MEMORY_MAP_PROVIDED;
}

void MultibootMbi_scanElfSections(const MultibootMbi *mbi, void *closure, void (*callback)(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end));
void MultibootMbi_scanModules(const MultibootMbi *mbi, void *closure, void (*callback)(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end, PhysicalAddress name));
void MultibootMbi_scanMemoryMap(const MultibootMbi *mbi, void *closure, void (*callback)(void *closure, uint64_t begin, uint64_t length, int type));

#endif
