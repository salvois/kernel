/*
FreeDOS-32 kernel
Copyright (C) 2008-2018  Salvatore ISAJA

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

/** ELF header, located at the beginning of the ELF file. */
typedef struct Elf32_Ehdr {
    unsigned char e_ident[16];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} Elf32_Ehdr;

/** ELF program header. */
typedef struct Elf32_Phdr {
    Elf32_Word p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} Elf32_Phdr;

/** Symbol in an ELF symbol table. */
typedef struct Elf32_Sym {
    Elf32_Word st_name;
    Elf32_Addr st_value;
    Elf32_Word st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half st_shndx;
} Elf32_Sym;

/** Relocation info in an ELF relocation table. */
typedef struct Elf32_Rel {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
} Elf32_Rel;

/** ELF relocation type. */
typedef enum Elf32_Rel_Type {
    R_386_NONE,
    R_386_32,
    R_386_PC32
} Elf32_Rel_Type;

/** Gets the symbol number from an Elf32_Rel item. */
static inline unsigned Elf32_Rel_getSymbol(const Elf32_Rel *rel) {
    return rel->r_info >> 8;
}

/** Gets the relocation type from an Elf32_Rel item. */
static inline Elf32_Rel_Type Elf32_Rel_getType(const Elf32_Rel *rel) {
    return (Elf32_Rel_Type) (rel->r_info & 0x00FF);
}

/**
 * Dynamic loader for objects in ELF format.
 * 
 * This class provides functionality to parse, relocate and dynamically link
 * object files in ELF format (see the System V Application Binary Interface).
 * 
 * <h2>Relocation</h2>
 * 
 * Relocatable object files can be loaded at an arbitrary memory address,
 * thus contain information to properly handle branches, calls and other
 * references to memory addresses.
 * The relocate() function parses this information and fixes the such
 * references so that all memory addresses are correct.
 * 
 * <h2>Dynamic linking</h2>
 * 
 * Object files may define new symbols (such as functions and global variables)
 * to enhance the functionality of the system, or may depend on symbols already
 * defined externally of the object itself. The kernel provides a global symbol
 * table to hold all these symbols, thus allowing object files to add their own
 * symbols or find the external ones.
 * 
 * The addSymbols() function adds all symbols defined by the object, that are
 * not marked for internal use only, to the global symbol table, allowing other
 * modules to use them.
 * If a symbol is already present in the global symbol table, and the symbol
 * has weak binding, the existing one is used, othewise if the symbol has global
 * binding it is an error and addSymbols() fails.
 * 
 * <h2>The "_start" function</h2>
 * 
 * As a special case, this loader searches for a symbol named "_start" in the
 * object file. If it is found, it is assumed to be a function that can be
 * called right after loading, relocating and linking the object to perform
 * module initialization. This symbol is never added to the global symbol table,
 * and the function must have the following signature:
 * <pre>extern "C" int _start();</pre>
 * Users of this class may call the main function through the main() function
 * pointer, if not NULL.
 */
typedef struct ElfLoader {
    int (*_start)();
    uint8_t * base;
    Elf32_Shdr *sectionStrings;
    uint8_t *sectionHeaders;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
} ElfLoader;

const char *ElfLoader_getSectionName(const ElfLoader *el, size_t sh_name) {
    return (const char*) (el->sectionStrings->sh_addr + sh_name);
}

Elf32_Shdr *ElfLoader_getSectionHeader(const ElfLoader *el, unsigned index) {
    if (index >= el->e_shnum) return NULL;
    return (Elf32_Shdr*) (el->sectionHeaders + index * el->e_shentsize);
}

Elf32_Sym *ElfLoader_getSymbol(Elf32_Shdr* symbolTable, unsigned index) {
    return (Elf32_Sym*) (symbolTable->sh_addr + index * symbolTable->sh_entsize);
}

const char *ElfLoader_getSymbolName(const Elf32_Shdr *symbolStrings, size_t st_name) {
    return (const char*) (symbolStrings->sh_addr + st_name);
}

/** Returns true if the ELF header of the specified loader is valid. */
static bool ElfLoader_checkElfHeader(const ElfLoader *el) {
    const Elf32_Ehdr *eh = (const Elf32_Ehdr *) el->base;
    if (*(const uint32_t *) eh->e_ident != 0x464C457F) return false; // Magic number
    if (eh->e_ident[4] != 1) return false; // EI_CLASS = ELFCLASS32
    if (eh->e_ident[5] != 1) return false; // EI_DATA = ELFDATA2LSB
    if (eh->e_ident[6] != 1) return false; // EI_VERSION = EV_CURRENT
    if (eh->e_type != 1) return false; // ET_REL, relocatable file
    if (eh->e_machine != 3) return false; // EM_386, i386
    if (eh->e_version != 1) return false; // EV_CURRENT
    return true;
}

/** Initializes the ELF loader for the image located at the specified base address. */
bool ElfLoader_init(ElfLoader *el, uint8_t *base) {
    el->_start = NULL;
    el->base = base;
    const Elf32_Ehdr *eh = (const Elf32_Ehdr *) el->base;
    Log_printf("Checking ELF header... ");
    if (!ElfLoader_checkElfHeader(el)) {
        Log_printf("failed\n");
        return false;
    } else Log_printf("OK\n");
    el->sectionHeaders = el->base + eh->e_shoff;
    el->e_shentsize = eh->e_shentsize;
    el->e_shnum = eh->e_shnum;
    el->sectionStrings = ElfLoader_getSectionHeader(el, eh->e_shstrndx);
    if (el->sectionStrings == NULL) {
        Log_printf("%s: no section strings found.\n", __func__);
        return false;
    }
    // Compute section addresses and find the highest occupied address
    uint8_t* end = el->base;
    uint8_t* a = el->sectionHeaders;
    for (unsigned k = 0; k < el->e_shnum; ++k, a += el->e_shentsize) {
        Elf32_Shdr* sh = (Elf32_Shdr*) a;
        if (sh->sh_addr == 0) sh->sh_addr = (Elf32_Addr) (el->base + sh->sh_offset);
        if ((uint8_t *) sh->sh_addr + sh->sh_size > end) end = (uint8_t *) (sh->sh_addr + sh->sh_size);
    }
    // Allocate space for SHT_NOBITS sections (e.g. the bss)
    a = el->sectionHeaders;
    for (unsigned k = 0; k < el->e_shnum; ++k, a += el->e_shentsize) {
        Elf32_Shdr *sh = (Elf32_Shdr *) a;
        if (sh->sh_type == SHT_NOBITS) {
            uint8_t *sectionBegin = (uint8_t *) (((uintptr_t) end + sh->sh_addralign - 1) & ~(sh->sh_addralign - 1));
            uint8_t *sectionEnd = sectionBegin + sh->sh_size;
            uint8_t *absoluteEnd = (uint8_t *) (((uintptr_t) end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
            Log_printf("%s: SHT_NOBITS section at [%p..%p) (%u bytes) must fit within %p (%u bytes).\n",
                    __func__, sectionBegin, sectionEnd, sectionEnd - sectionBegin, absoluteEnd, absoluteEnd - sectionBegin);
            if (sectionEnd <= absoluteEnd) {
                sh->sh_addr = (Elf32_Addr) sectionBegin;
                memzero(sectionBegin, sectionEnd - sectionBegin);
                end = sectionEnd;
            } else {
                Video_printf("%s: not enough space for STH_NOBITS section %s\n", __func__, ElfLoader_getSectionName(el, sh->sh_name));
                return false;
            }
        }
    }
    return true;
}

/** Prints ELF sections for debugging purposes. */
void ElfLoader_dumpSections(const ElfLoader *el) {
    #if 1
    Log_printf("%s, listing sections:\n", __func__);
    const uint8_t *a = el->sectionHeaders;
    for (size_t i = 0; i < el->e_shnum; ++i, a += el->e_shentsize) {
        const Elf32_Shdr *sh = (const Elf32_Shdr *) a;
        Log_printf("  Section %u name=%s flags=0x%08X addr=%p size=%u offset=%p\n",
                i, ElfLoader_getSectionName(el, sh->sh_name), sh->sh_flags, sh->sh_addr, sh->sh_size, sh->sh_offset);
    }
    #endif
}

/** Prints ELF symbols for debugging purposes. */
void ElfLoader_dumpSymbols(const ElfLoader *el) {
    #if 1
    Log_printf("%s, listing symbols:\n", __func__);
    const uint8_t *a = el->sectionHeaders;
    for (size_t i = 0; i < el->e_shnum; ++i, a += el->e_shentsize) {
        const Elf32_Shdr *sh = (const Elf32_Shdr *) a;
        if (sh->sh_type == SHT_SYMTAB) {
            const Elf32_Shdr *symbolStrings = ElfLoader_getSectionHeader(el, sh->sh_link);
            for (size_t b = 0; b < sh->sh_size; b += sh->sh_entsize) {
                const Elf32_Sym *sym = (const Elf32_Sym *) (sh->sh_addr + b);
                Log_printf("  >%u: symbol value=%p symbol name=%s symbol section=%d\n",
                        b, sym->st_value, ElfLoader_getSymbolName(symbolStrings, sym->st_name), sym->st_shndx);
            }
        } else if (sh->sh_type == SHT_DYNSYM) {
            Log_printf("%s: SHT_DYNSYM symbols not supported.\n", __func__);
        }
    }
    #endif
}

/** Compute relocations for the specified ELF image loader. */
bool ElfLoader_relocate(ElfLoader *el) {
    uint8_t *a = el->sectionHeaders;
    for (unsigned k = 0; k < el->e_shnum; k++, a += el->e_shentsize) {
        Elf32_Shdr *sh = (Elf32_Shdr *) a;
        if (sh->sh_type == SHT_REL) {
            Elf32_Shdr *relocatedSection = ElfLoader_getSectionHeader(el, sh->sh_info);
            Elf32_Shdr *symbolTable = ElfLoader_getSectionHeader(el, sh->sh_link);
            Elf32_Rel  *r = (Elf32_Rel *) (el->base + sh->sh_offset);
            for (unsigned b = 0; b < sh->sh_size; b += sizeof(Elf32_Rel), ++r) {
                Elf32_Sym *sym = ElfLoader_getSymbol(symbolTable, Elf32_Rel_getSymbol(r));
                uint32_t *p = (uint32_t *) (relocatedSection->sh_addr + r->r_offset);
                switch (Elf32_Rel_getType(r)) {
                    case R_386_32: *p += sym->st_value;
                        break;
                    case R_386_PC32: *p = *p + sym->st_value - (uint32_t) p;
                        break;
                    default:
                        Log_printf("%s: unsupported relocation type %d. Only R_386_32 and R_386_PC32 are supported.\n",
                                __func__, Elf32_Rel_getType(r));
                        return false;
                }
            }
        } else if (sh->sh_type == SHT_RELA) {
            Log_printf("%s: SHT_RELA section not supported.\n", __func__);
            return false;
        }
    }
    return true;
}

/** Searches the symbol table of the specified ELF loader for the "_start" function. */
Elf32_Addr ElfLoader_findStartSymbol(ElfLoader *el) {
    uint8_t *a = el->sectionHeaders;
    for (unsigned k = 0; k < el->e_shnum; ++k, a += el->e_shentsize) {
        Elf32_Shdr *sh = (Elf32_Shdr *) a;
        if (sh->sh_type != SHT_SYMTAB && sh->sh_type != SHT_DYNSYM) continue;
        Elf32_Shdr *symbolStrings = ElfLoader_getSectionHeader(el, sh->sh_link);
        if (symbolStrings == NULL) continue;
        // Skip the first entry, reserved for the undefined symbol
        for (unsigned b = sh->sh_entsize; b < sh->sh_size; b += sh->sh_entsize) {
            Elf32_Sym *sym = (Elf32_Sym *) (sh->sh_addr + b);
            const char *name = ElfLoader_getSymbolName(symbolStrings, sym->st_name);
            // Skip symbols for the undefined section or special sections
            if (sym->st_shndx == 0 || sym->st_shndx >= 0xFF00) continue;
            Elf32_Shdr *symbolSection = ElfLoader_getSectionHeader(el, sym->st_shndx);
            if (symbolSection == NULL) continue;
            sym->st_value += symbolSection->sh_addr;
            if (strcmp(name, "_start") == 0) return sym->st_value;
        }
    }
    return 0;
}

/**
 * Uses an ELF loader to process a Multiboot module as a relocatable ELF.
 * If the image contains a "_start" function, it is executed before returning.
 * @param el Loader structure to be initialized for parsing the ELF image.
 * @param begin Base higher-half virtual address of the Multiboot module (inclusive).
 * @param end End higher-half virtual address of the Multiboot module (exclusive).
 * @param name Nul-terminated string containing the name of the Multiboot module.
 */
void ElfLoader_fromMultibootModule(ElfLoader *el, uintptr_t begin, uint32_t end, const char *name) {
    ElfLoader_init(el, (uint8_t *) begin);
    ElfLoader_dumpSections(el);
    ElfLoader_relocate(el);
    el->_start = (int(*)()) ElfLoader_findStartSymbol(el);
    ElfLoader_dumpSymbols(el);
    if (el->_start) {
        int result = el->_start();
        Log_printf("Module %s: _start returned %d\n", name, result);
    } else {
        Log_printf("Module %s has no _start function.\n", name);
    }
}

/**
 * Uses an ELF loader to process a Multiboot module as an executable ELF.
 * A new task is created and its initial thread is made runnable.
 * @param el Loader structure to be initialized for parsing the ELF image.
 * @param begin Base higher-half virtual address of the Multiboot module (inclusive).
 * @param end End higher-half virtual address of the Multiboot module (exclusive).
 * @param commandLine Nul-terminated string containing the command line passed
 *        by the boot loader (includes module name on GRUB legacy, arguments only on GRUB 2).
 * 
 * The module command line can contain the following space-separated arguments:
 * "priority" unsigned integer between 0 and 254
 * "nice" unsigned integer between 0 and 39
 * 
 * Note: this function as currently implemented is just a temporary hack.
 * It must be called with while holding the spinlock of the CPU node and it
 * will keep it locked for a long time!
 */
void ElfLoader_fromExeMultibootModule(Task *task, uintptr_t begin, uint32_t end, const char *commandLine) {
    unsigned priority = 64;
    unsigned nice = 20;
    // Quick and dirty command line parser
    const char *s = commandLine;
    while (*s != '\0') {
        const char *token = s;
        while ((*s >= 'a' && *s <= 'z')) s++;
        if (s != token) {
            if (s - token == 8 && memcmp(token, "priority", 8) == 0) {
                while (*s == ' ') s++;
                token = s;
                while ((*s >= '0' && *s <= '9')) s++;
                if (s != token) priority = atou(token, s - token);
            } else if (s - token == 4 && memcmp(token, "nice", 4) == 0) {
                while (*s == ' ') s++;
                token = s;
                while ((*s >= '0' && *s <= '9')) s++;
                if (s != token) nice = atou(token, s - token);
            }
        } else {
            s++;
        }
    }
    // Load the module
    Log_printf("Loading ELF executable for \"%s\" at [%p..%p) with priority %d and nice %d\n", commandLine, begin, end, priority, nice);
    const Elf32_Ehdr *ehdr = phys2virt(begin);
    if (ehdr->e_type != 2) { // ET_EXEC, executable file
        Log_printf("  Not an executable file.\n");
        return;
    }
    uintptr_t a = begin + ehdr->e_phoff;
    for (size_t i = 0; i < ehdr->e_phnum; i++, a += ehdr->e_phentsize) {
        const Elf32_Phdr *phdr = phys2virt(a);
        if (phdr->p_type != 1) continue; // Skip if not PT_LOAD, loadable segment
        Log_printf("  Segment %d is loadable, Offset=0x%08X, VirtAddr=%p, PhysAddr=%p, FileSize=0x%08X, MemSize=0x%08X, Flags=0x%08X, Align=0x%08X\n",
                i, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz, phdr->p_flags, phdr->p_align);
        for (size_t j = 0; j < phdr->p_memsz; j += PAGE_SIZE) {
            if (j < phdr->p_filesz) {
                AddressSpace_map(task, phdr->p_vaddr + j, (begin + phdr->p_offset + j) >> PAGE_SHIFT);
                if (j + PAGE_SIZE >= phdr->p_filesz) {
                    //memzero(phdr->p_vaddr + j);
                }
            } else {
                AddressSpace_mapFromNewFrame(task, phdr->p_vaddr + j, otherMemoryRegion);
            }
        }
    }
    uint64_t seed = Tsc_read();
    uintptr_t stackTop = HIGH_HALF_BEGIN - ((xorshift64star(&seed) & 0x7FF) << PAGE_SHIFT); // 8 MiB randomization
    // TODO: dynamically grow stack on page fault
    for (size_t i = 1048576; i > 0; i -= PAGE_SIZE) {
        AddressSpace_mapFromNewFrame(task, stackTop - i, otherMemoryRegion);
    }
    Log_printf("  Stack allocated and mapped at [%p..%p).\n", stackTop - 1048576, stackTop);
    Thread *thread = SlabAllocator_allocate(&threadAllocator);
    if (thread == NULL) {
        Video_printf("  Not enough memory for task %p.", task);
        // TODO: do some serious cleanup
        return;
    }
    Thread_initialize(task, thread, priority, nice, ehdr->e_entry, stackTop);
    SlabAllocator_initialize(&task->capabilitySpace, sizeof(Capability), task);
    CpuNode_addRunnableThread(&CpuNode_theInstance, thread);
}
