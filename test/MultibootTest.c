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
#include "test.h"
#include "kernel.h"

static void MultibootTest_scanModules_callback(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end, PhysicalAddress name) {
    size_t *callCount = (size_t *) closure;
    (*callCount)++;
    ASSERT(index < 3);
    ASSERT(index != 0 || (begin.v == 1 && end.v == 2 && name.v == 10));
    ASSERT(index != 1 || (begin.v == 2 && end.v == 3 && name.v == 11));
    ASSERT(index != 2 || (begin.v == 4 && end.v == 5 && name.v == 12));
}

static void MultibootTest_scanModules() {
    MultibootModule modules[3] = {
        (MultibootModule) { .mod_start = 1, .mod_end = 2, .string = 10 },
        (MultibootModule) { .mod_start = 2, .mod_end = 3, .string = 11 },
        (MultibootModule) { .mod_start = 4, .mod_end = 5, .string = 12 }
    };
    MultibootMbi mbi = { .flags = MULTIBOOTMBI_MODULES_PROVIDED, .mods_count = 3, .mods_addr = virt2phys(modules).v };
    size_t callCount = 0;
    MultibootMbi_scanModules(&mbi, &callCount, MultibootTest_scanModules_callback);
    ASSERT(callCount == 3);
}

static void MultibootTest_scanModulesWithNoModulesProvided() {
    MultibootMbi mbi = { .flags = 0 };
    size_t callCount = 0;
    MultibootMbi_scanModules(&mbi, &callCount, NULL);
    ASSERT(callCount == 0);
}

static void MultibootTest_scanMemoryMap_callback(void *closure, uint64_t begin, uint64_t length, int type) {
    size_t *callCount = (size_t *) closure;
    ASSERT(*callCount < 3);
    ASSERT(*callCount != 0 || (begin == 0x0000000100000010 && length == 0x0000000200000020 && type == 1));
    ASSERT(*callCount != 1 || (begin == 0x0000000300000030 && length == 0x0000000400000040 && type == 2));
    ASSERT(*callCount != 2 || (begin == 0x0000000500000050 && length == 0x0000000600000060 && type == 3));
    (*callCount)++;
}

static void MultibootTest_scanMemoryMap() {
    MultibootMemoryMap memoryMaps[3] = {
        (MultibootMemoryMap) { .size = 20, .base_addr_high = 1, .base_addr_low = 0x10, .length_high = 2, .length_low = 0x20, .type = 1 },
        (MultibootMemoryMap) { .size = 20, .base_addr_high = 3, .base_addr_low = 0x30, .length_high = 4, .length_low = 0x40, .type = 2 },
        (MultibootMemoryMap) { .size = 20, .base_addr_high = 5, .base_addr_low = 0x50, .length_high = 6, .length_low = 0x60, .type = 3 }
    };
    MultibootMbi mbi = { .flags = MULTIBOOTMBI_MEMORY_MAP_PROVIDED, .mmap_length = 3 * sizeof(MultibootMemoryMap), .mmap_addr = virt2phys(memoryMaps).v };
    size_t callCount = 0;
    MultibootMbi_scanMemoryMap(&mbi, &callCount, MultibootTest_scanMemoryMap_callback);
    ASSERT(callCount == 3);
}

static void MultibootTest_scanMemoryMapWithMemoryMapNotProvided_callback(void *closure, uint64_t begin, uint64_t length, int type) {
    size_t *callCount = (size_t *) closure;
    ASSERT(*callCount < 2);
    ASSERT(*callCount != 0 || (begin == 0 && length == 0xA0000 && type == 1));
    ASSERT(*callCount != 1 || (begin == 0x100000 && length == 0x3FF00000 && type == 1));
    (*callCount)++;
}

static void MultibootTest_scanMemoryMapWithMemoryMapNotProvided() {
    MultibootMbi mbi = { .flags = MULTIBOOTMBI_MEMLOWER_MEMUPPER_PROVIDED, .mem_lower = 640, .mem_upper = 1047552 };
    size_t callCount = 0;
    MultibootMbi_scanMemoryMap(&mbi, &callCount, MultibootTest_scanMemoryMapWithMemoryMapNotProvided_callback);
    ASSERT(callCount == 2);
}

static void MultibootTest_scanMemoryMapWithNoMemoryInfoProvided() {
    MultibootMbi mbi = { .flags = 0 };
    size_t callCount = 0;
    MultibootMbi_scanMemoryMap(&mbi, &callCount, NULL);
    ASSERT(callCount == 0);
}
static void MultibootTest_scanElfSections_callback(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end) {
    size_t *callCount = (size_t *) closure;
    ASSERT(*callCount < 3);
    ASSERT(index != 0 || (begin.v == 1 && end.v == 11));
    ASSERT(index != 1 || (begin.v == 2 && end.v == 22));
    ASSERT(index != 2 || (begin.v == 3 && end.v == 33));
    (*callCount)++;
}

static void MultibootTest_scanElfSections() {
    Elf32_Shdr sectionHeaders[3] = {
        (Elf32_Shdr) { .sh_addr = 1, .sh_size = 10 },
        (Elf32_Shdr) { .sh_addr = HIGH_HALF_BEGIN + 2, .sh_size = 20 },
        (Elf32_Shdr) { .sh_addr = 3, .sh_size = 30 },
    };
    MultibootMbi mbi = { .flags = MULTIBOOTMBI_ELF_SECTIONS_PROVIDED, .syms.elf.num = 3, .syms.elf.size = sizeof(Elf32_Shdr), .syms.elf.addr = virt2phys(sectionHeaders).v };
    size_t callCount = 0;
    MultibootMbi_scanElfSections(&mbi, &callCount, MultibootTest_scanElfSections_callback);
    ASSERT(callCount == 3);
}

static void MultibootTest_scanElfSectionsWithNoSectionsProvided() {
    MultibootMbi mbi = { .flags = 0 };
    size_t callCount = 0;
    MultibootMbi_scanElfSections(&mbi, &callCount, NULL);
    ASSERT(callCount == 0);
}

void MultibootTest_run() {
    RUN_TEST(MultibootTest_scanModules);
    RUN_TEST(MultibootTest_scanModulesWithNoModulesProvided);
    RUN_TEST(MultibootTest_scanMemoryMap);
    RUN_TEST(MultibootTest_scanMemoryMapWithMemoryMapNotProvided);
    RUN_TEST(MultibootTest_scanMemoryMapWithNoMemoryInfoProvided);
    RUN_TEST(MultibootTest_scanElfSections);
    RUN_TEST(MultibootTest_scanElfSectionsWithNoSectionsProvided);
}
