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

// Memory addresses of the Multiboot kernel image defined by the linker
extern uint8_t Boot_imageBeginPhysicalAddress;
extern uint8_t Boot_imageEndPhysicalAddress;
extern uint8_t Boot_bssBegin;
extern uint8_t Boot_bssEnd;

// Multiboot information from boot_asm.S
extern uint32_t Boot_multibootMagic;
extern uint32_t Boot_mbiPhysicalAddress;
extern uint8_t Boot_stack;

/** 
 * Search for the Multi-Processor Floating Pointer structure within
 * the specified range of physical addresses.
 */
__attribute((section(".boot"))) static const MpFloatingPointer *Boot_searchMpFloatingPointer(uintptr_t begin, uintptr_t end) {
    Log_printf("Searching for the MP Floating Pointer Structure at physical address [%p, %p).\n", begin, end);
    assert((begin & 0xF) == 0); // the structure must be aligned to a 16-byte boundary
    for ( ; begin < end; begin += 16) {
        const MpFloatingPointer *mpfp = phys2virt(begin);
        if (mpfp->signature == 0x5F504D5F) { // "_MP_"
            //log("  Signature found at %p.\n", mpfp);
            uint8_t checksum = 0;
            const uint8_t *base = (const uint8_t *) mpfp;
            for (size_t i = 0; i < mpfp->mpFloatingPointerSize * 16; i++) {
                //log("%02X ", base[i]);
                checksum += base[i];
            }
            if (checksum == 0) return mpfp;
        }
    }
    return NULL;
}

/**
 * Searches for the Multi-Processor Config structure containing CPU information.
 * @return The higher half virtual address of the MP Config, or NULL if not found.
 */
__attribute((section(".boot"))) static const MpConfigHeader *Boot_findMpConfig() {
    // Find the MP Floating Pointer structure
    uintptr_t ebda = (uintptr_t) *(const uint16_t *) phys2virt(0x40E) << 4;
    const MpFloatingPointer *mpfp = Boot_searchMpFloatingPointer(ebda, ebda + 1024);
    if (mpfp == NULL) mpfp = Boot_searchMpFloatingPointer(654336, 655360);
    if (mpfp == NULL) mpfp = Boot_searchMpFloatingPointer(523264, 524288);
    if (mpfp == NULL) mpfp = Boot_searchMpFloatingPointer(0xF0000, 0x100000);
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
    return phys2virt(mpfp->mpConfigPhysicalAddress);
}

/**
 * Early entry point to the C kernel.
 * This is called on the bootstrap processor by the assembly boot code
 * after paging is enabled and the kernel is mapped to the higher half.
 * It runs on the temp boot stack and does the minimum to set up Cpu structures.
 * @return The Cpu structure representing the bootstrap processor.
 */
__attribute((section(".boot"))) Cpu *Boot_entry() {
    Video_initialize();
    Log_initialize();
    const MultibootMbi *mbi = phys2virt(Boot_mbiPhysicalAddress);
    if ((mbi->flags & (1 << 9)) != 0) {
        Video_printf("FreeDOS-32 kernel booting from \"%s\"\n", phys2virt(mbi->boot_loader_name));
    } else {
        Video_printf("FreeDOS-32 kernel booting from an unknown bootloader\n");
    }
    if (Boot_multibootMagic != 0x2BADB002) {
        panic("Invalid MultiBoot magic value 0x%08X: could not initialize the system.\n", Boot_multibootMagic);
    }
    Log_printf("sizeof(Cpu)=%d, offsetof(Cpu, stack)=%d\n", sizeof(Cpu), offsetof(Cpu, stack));
    Log_printf("sizeof(Task)=%d\n", sizeof(Task));
    Log_printf("sizeof(Thread)=%d\n", sizeof(Thread));
    Log_printf("sizeof(Channel)=%d\n", sizeof(Channel));
    Log_printf("sizeof(Endpoint)=%d\n", sizeof(Endpoint));
    Log_printf("sizeof(Frame)=%d\n", sizeof(Frame));
    PhysicalMemory_initializeFromMultibootV1(mbi, (uintptr_t) &Boot_imageBeginPhysicalAddress, (uintptr_t) &Boot_imageEndPhysicalAddress);
    const MpConfigHeader *mpConfigHeader = Boot_findMpConfig();
    Acpi_findConfig();
    Cpu *bootCpu = Cpu_initializeCpuStructs(mpConfigHeader);
    return bootCpu;
}

/**
 * Entry point to the C kernel for the bootstrap processor.
 * This is called by the assembly boot code on the stack of the bootstrap
 * processor, completing initialization of the kernel and starting other CPUs.
 * @return The ThreadRegisters structure for the thread to run.
 */
__attribute((section(".boot"))) ThreadRegisters *Boot_bspEntry() {
    Cpu *cpu = Cpu_getCurrent();
    Cpu_loadCpuTables(cpu);
    AcpiPmTimer_initialize();
    Cpu_setupIdt();
    Cpu_startOtherCpus();
    Pic8259_initialize(0x50, 0x70);
    LapicTimer_initialize(&cpu->lapicTimer);
    Tsc_initialize(&cpu->tsc);
    SlabAllocator_initialize(&taskAllocator, sizeof(Task), NULL);
    SlabAllocator_initialize(&threadAllocator, sizeof(Thread), NULL);
    SlabAllocator_initialize(&channelAllocator, sizeof(Channel), NULL);
    Video_printf("Now waiting for other CPUs to boot...\n");
    AtomicWord_set(&cpu->initialized, 1);
    while (true) {
        bool allInitialized = true;
        for (size_t i = 0; i < Cpu_cpuCount; i++) {
            Cpu *c = &Cpu_cpus[i];
            Log_printf("Checking if CPU #%d is alive. initialized=%d\n", i, AtomicWord_get(&c->initialized));
            if (c != cpu && AtomicWord_get(&c->initialized) == 0) {
                allInitialized = false;
                break;
            }
        }
        if (allInitialized) break;
    }
    //panic("Stop the world.\n");
    Video_printf("All CPUs are running.\n");
    Spinlock_lock(&CpuNode_theInstance.lock); // see comments on ElfLoader_fromExeMultibootModule()
    // Test Multiboot modules
    const MultibootMbi *mbi = phys2virt(Boot_mbiPhysicalAddress);
    if (mbi->flags & (1 << 3)) { // Boot modules provided
        const MultibootModule *m = phys2virt(mbi->mods_addr);
        for (size_t i = 0; i < mbi->mods_count; i++, m++) {
            Log_printf("Testing multiboot module %d at [%p-%p), string=%p \"%s\".\n", i, m->mod_start, m->mod_end, m->string, phys2virt(m->string));
            Task *task = Task_create(NULL, NULL);
            Video_printf("Task at %p\n", task);
            Video_printf("Task address space root at %p.\n", task->addressSpace.root);
            AddressSpace_activate(&task->addressSpace);
            ElfLoader_fromExeMultibootModule(task, m->mod_start, m->mod_end, phys2virt(m->string));
        }
    }
    Spinlock_unlock(&CpuNode_theInstance.lock); // see comments on ElfLoader_fromExeMultibootModule()
    Cpu_exitKernel(cpu);
    return cpu->currentThread->regs;
}

/**
 * Entry point to the C kernel for CPUs other than the bootstrap processor.
 * This is called by the assembly boot code after the CPU has been switched
 * to protected mode, paging is enabled, the appropriate Cpu structure
 * has been found, on the per-CPU kernel stack.
 * @return 
 */
__attribute__((section(".boot"))) ThreadRegisters *Boot_apEntry() {
    Cpu *cpu = Cpu_getCurrent();
    Log_printf("Welcome from CPU %d.\n", cpu - Cpu_cpus);
    Cpu_loadCpuTables(cpu);
    Log_printf("Enabling LAPIC on CPU #%d (LAPIC ID 0x%02X, Cpu struct at %p).\n", cpu - Cpu_cpus, cpu->lapicId, cpu);
    Cpu_writeLocalApic(lapicSpuriousInterrupt, 0x1FF); // LAPIC enabled, Focus Check disabled, spurious vector 0xFF
    LapicTimer_initialize(&cpu->lapicTimer);
    Tsc_initialize(&cpu->tsc);
    AtomicWord_set(&cpu->initialized, 1);
    Cpu_exitKernel(cpu);
    return cpu->currentThread->regs;
}
