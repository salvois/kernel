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
#include "PhysicalMemory.h"

extern uint8_t Boot_imageBeginPhysicalAddress; // from link.ld
extern uint8_t Boot_imageEndPhysicalAddress; // from link.ld
extern uint32_t Boot_multibootMagic; // from Boot_asm.S
extern PhysicalAddress Boot_mbiPhysicalAddress; // from Boot_asm.S

__attribute((section(".boot")))
static void waitForAllCpus(Cpu* cpu){
    Video_printf("Now waiting for other CPUs to boot...\n");
    AtomicWord_set(&cpu->initialized, 1);
    while (true) {
        bool allInitialized = true;
        for (size_t i = 0; i < Cpu_cpuCount; i++) {
            Cpu *c = Cpu_cpus[i];
            Log_printf("Checking if CPU #%d is alive. initialized=%d\n", i, AtomicWord_get(&c->initialized));
            if (c != cpu && AtomicWord_get(&c->initialized) == 0) {
                allInitialized = false;
                break;
            }
        }
        if (allInitialized) break;
    }
    Video_printf("All CPUs are running.\n");
}

__attribute((section(".boot")))
static void testMultibootModulesCallback(void *closure, size_t index, PhysicalAddress begin, PhysicalAddress end, PhysicalAddress name) {
    Log_printf("Testing multiboot module %d at [%p-%p), string=%p \"%s\".\n", index, begin.v, end.v, name.v, phys2virt(name));
    Task *task = Task_create(NULL, NULL);
    Video_printf("Task at %p\n", task);
    Video_printf("Task address space root at %p.\n", task->addressSpace.root);
    AddressSpace_activate(&task->addressSpace);
    ElfLoader_fromExeMultibootModule(task, begin, end, phys2virt(name));
}

__attribute((section(".boot")))
static void testMultibootModules() {
    Spinlock_lock(&CpuNode_theInstance.lock); // see comments on ElfLoader_fromExeMultibootModule()
    const MultibootMbi *mbi = phys2virt(Boot_mbiPhysicalAddress);
    MultibootMbi_scanModules(mbi, NULL, testMultibootModulesCallback);
    Spinlock_unlock(&CpuNode_theInstance.lock);
}

/**
 * Entry point to the C kernel for the bootstrap processor.
 * This is called by the assembly boot code on the stack of the bootstrap
 * processor, completing initialization of the kernel and starting other CPUs.
 * @return The ThreadRegisters structure for the thread to run.
 */
__attribute((section(".boot")))
ThreadRegisters *Boot_bspEntry() {
    Cpu *currentCpu = Cpu_getCurrent();
    Log_printf("Welcome from CPU %d.\n", currentCpu->index);
    Cpu_loadCpuTables(currentCpu);
    AcpiPmTimer_initialize();
    Cpu_setupInterruptDescriptorTable();
    Log_printf("Enabling LAPIC on bootstrap processor (CPU #%d, LAPIC ID 0x%02X, Cpu struct at %p).\n", currentCpu->index, currentCpu->lapicId, currentCpu);
    Cpu_writeLocalApic(lapicSpuriousInterrupt, 0x1FF); // LAPIC enabled, Focus Check disabled, spurious vector 0xFF
    Cpu_startOtherCpus();
    Pic8259_initialize(0x50, 0x70);
    SlabAllocator_initialize(&taskAllocator, sizeof(Task), NULL);
    SlabAllocator_initialize(&threadAllocator, sizeof(Thread), NULL);
    SlabAllocator_initialize(&channelAllocator, sizeof(Channel), NULL);
    LapicTimer_initialize(&currentCpu->lapicTimer);
    Tsc_initialize(&currentCpu->tsc);
    waitForAllCpus(currentCpu);
    testMultibootModules();
    Cpu_schedule(currentCpu);
    return currentCpu->currentThread->regs;
}

/**
 * Entry point to the C kernel for CPUs other than the bootstrap processor.
 * This is called by the assembly boot code after the CPU has been switched
 * to protected mode, paging is enabled, the appropriate Cpu structure
 * has been found, on the per-CPU kernel stack.
 * @return The ThreadRegisters structure for the thread to run.
 */
__attribute__((section(".boot")))
ThreadRegisters *Boot_apEntry() {
    Cpu *currentCpu = Cpu_getCurrent();
    Log_printf("Welcome from CPU %d.\n", currentCpu->index);
    Cpu_loadCpuTables(currentCpu);
    Log_printf("Enabling LAPIC on CPU #%d (LAPIC ID 0x%02X, Cpu struct at %p).\n", currentCpu->index, currentCpu->lapicId, currentCpu);
    Cpu_writeLocalApic(lapicSpuriousInterrupt, 0x1FF); // LAPIC enabled, Focus Check disabled, spurious vector 0xFF
    LapicTimer_initialize(&currentCpu->lapicTimer);
    Tsc_initialize(&currentCpu->tsc);
    AtomicWord_set(&currentCpu->initialized, 1);
    Cpu_schedule(currentCpu);
    return currentCpu->currentThread->regs;
}

/**
 * Early entry point to the C kernel.
 * This is called on the bootstrap processor by the assembly boot code
 * after paging is enabled and the kernel is mapped to the higher half.
 * It runs on the temp boot stack and does the minimum to set up Cpu structures.
 * @return The Cpu structure representing the bootstrap processor.
 */
__attribute((section(".boot")))
Cpu *Boot_entry() {
    Video_initialize();
    Log_initialize();
    const MultibootMbi *mbi = phys2virt(Boot_mbiPhysicalAddress);
    if ((mbi->flags & (1 << 9)) != 0) {
        Video_printf("FreeDOS-32 kernel booting from \"%s\"\n", phys2virt(physicalAddress(mbi->boot_loader_name)));
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
    PhysicalMemory_initializeRegions();
    PhysicalMemory_initializeFromMultibootV1(
            mbi,
            physicalAddress((uintptr_t) &Boot_imageBeginPhysicalAddress),
            physicalAddress((uintptr_t) &Boot_imageEndPhysicalAddress));
    Acpi_findConfig();
    const MpConfigHeader *mpConfigHeader = MultiProcessorSpecification_searchFindMpConfig();
    Cpu *bootCpu = Cpu_initializeCpuStructs(mpConfigHeader);
    return bootCpu;
}
