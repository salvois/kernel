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

__attribute__((section(".boot")))
void MultibootMbi_scanElfSections(const MultibootMbi *mbi, void *closure, void (*callback)(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end)) {
    if (!MultibootMbi_areElfSectionsProvided(mbi)) return;
    Log_printf("ELF sections loaded at %p, num=%u, size=%u, shndx=%u\n", mbi->syms.elf.addr, mbi->syms.elf.num, mbi->syms.elf.size, mbi->syms.elf.shndx);
    const uint8_t *a = phys2virt(physicalAddress(mbi->syms.elf.addr));
    for (size_t i = 0; i < mbi->syms.elf.num; i++, a += mbi->syms.elf.size) {
        const Elf32_Shdr *sh = (const Elf32_Shdr *) a;
        Log_printf("  i=%u flags=%08X [%p..%p)\n", i, sh->sh_flags, sh->sh_addr, sh->sh_addr + sh->sh_size);
        uintptr_t begin = sh->sh_addr;
        if (begin >= HIGH_HALF_BEGIN) begin -= HIGH_HALF_BEGIN;
        callback(closure, i, physicalAddress(begin), physicalAddress(begin + sh->sh_size));
    }
}

__attribute__((section(".boot")))
void MultibootMbi_scanModules(const MultibootMbi *mbi, void *closure, void (*callback)(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end, PhysicalAddress name)) {
    if (!MultibootMbi_areModulesProvided(mbi)) return;
    const MultibootModule *m = phys2virt(physicalAddress(mbi->mods_addr));
    for (size_t i = 0; i < mbi->mods_count; i++, m++)
        callback(closure, i, physicalAddress(m->mod_start), physicalAddress(m->mod_end), physicalAddress(m->string));
}

__attribute__((section(".boot")))
void MultibootMbi_scanMemoryMap(const MultibootMbi *mbi, void *closure, void (*callback)(void *closure, uint64_t begin, uint64_t length, int type)) {
    if (MultibootMbi_isMemoryMapProvided(mbi)) {
        uintptr_t mmapEnd = mbi->mmap_addr + mbi->mmap_length;
        uintptr_t a = mbi->mmap_addr;
        while (a < mmapEnd) {
            const MultibootMemoryMap *m = phys2virt(physicalAddress(a));
            uint64_t begin = ((uint64_t) m->base_addr_high << 32) | (uint64_t) m->base_addr_low;
            uint64_t length = ((uint64_t) m->length_high << 32) | (uint64_t) m->length_low;
            callback(closure, begin, length, m->type);
            a += m->size + sizeof(m->size);
        }
    } else if (MultibootMbi_areMemLowerAndMemUpperProvided(mbi)) {
        callback(closure, 0, (uint64_t) mbi->mem_lower << 10, 1);
        callback(closure, 0x100000, (uint64_t) mbi->mem_upper << 10, 1);
    }
}
