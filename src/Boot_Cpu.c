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

/** A structure holding base address and limit for a descriptor table, properly aligned. */
typedef struct DescriptorTableLocation {
    uint16_t pad;
    uint16_t limit;
    uint32_t base;
} DescriptorTableLocation;

/** Global table of Cpu structures in use, each representing a logical processor. */
Cpu *Cpu_cpus[MAX_CPU_COUNT];
/** Global count of Cpu structures in use. */
size_t Cpu_cpuCount;
/** The one Interrupt Descriptor Table. */
static CpuDescriptor Cpu_idt[256];

extern uint8_t Cpu_firstInterruptTrampoline;  // from Cpu_asm.S
extern uint8_t Cpu_secondInterruptTrampoline; // from Cpu_asm.S
extern uint8_t Cpu_spuriousInterruptHandler;  // from Cpu_asm.S
extern uint8_t Cpu_sysenter; // from Cpu_asm.S

/** Sets a segment descriptor (for GDT initialization). */
__attribute__((section(".boot"))) static void SegmentDescriptor_set(CpuDescriptor *d, uint32_t base, uint32_t limit, uint32_t access) {
    d->word0 = (base << 16) | (limit & 0x0000FFFF);
    d->word1 = (base & 0xFF000000) | (access & 0x00F0FF00) | (limit & 0x000F0000) | ((base & 0x00FF0000) >> 16);
}

/** Sets a gate descriptor (for IDT initialization). */
__attribute__((section(".boot"))) static void GateDescriptor_set(CpuDescriptor *d, uint16_t selector, uint32_t offset, uint16_t access) {
    d->word0 = ((uint32_t) selector << 16) | (offset & 0x0000FFFF);
    d->word1 = (offset & 0xFFFF0000) | access;
}

/** Initializes the Cpu structure for the logical processor with the specified LAPIC id. */
__attribute__((section(".boot"))) static void Cpu_initialize(Cpu *cpu, size_t index, size_t lapicId) {
    memzero(cpu, sizeof(Cpu));
    cpu->index = index;
    cpu->lapicId = lapicId;
    cpu->thisCpu = cpu;
    cpu->cpuNode = &CpuNode_theInstance;
    cpu->active = true;
    // Setup and load the GDT
    SegmentDescriptor_set(&cpu->gdt[nullSelector], 0, 0, 0);
    SegmentDescriptor_set(&cpu->gdt[flatKernelCS >> 3], 0, 0xFFFFF, 0xC09A00); // page granular, 32-bit, non-system, code, r/x, ring 0
    SegmentDescriptor_set(&cpu->gdt[flatKernelDS >> 3], 0, 0xFFFFF, 0xC09200); // page granular, 32-bit, non-system, data, r/w, ring 0
    SegmentDescriptor_set(&cpu->gdt[flatUserCS >> 3], 0, 0xFFFFF, 0xC0FA00); // page granular, 32-bit, non-system, code, r/x, ring 3
    SegmentDescriptor_set(&cpu->gdt[flatUserDS >> 3], 0, 0xFFFFF, 0xC0F200); // page granular, 32-bit, non-system, data, r/w, ring 3
    SegmentDescriptor_set(&cpu->gdt[tssSelector >> 3], (uint32_t) &cpu->tss, sizeof(Tss) - 1, 0x408900); // byte granular, 32-bit, 32-bit TSS available
    SegmentDescriptor_set(&cpu->gdt[kernelGS >> 3], (uint32_t) cpu, sizeof(Cpu) - 1, 0x409200); // byte granular, 32-bit, 32-bit, writable
    // Initialize the idle thread
    cpu->idleThread.threadFunction = Cpu_idleThreadFunction;
    cpu->idleThread.cpu = cpu;
    cpu->idleThread.priority = THREAD_IDLE_PRIORITY;
    cpu->idleThread.stack = (uint8_t *) &cpu->idleThread.regsBuf;
    cpu->idleThread.regs = (ThreadRegisters *) (cpu->idleThreadStack + CPU_IDLE_THREAD_STACK_SIZE - offsetof(ThreadRegisters, esp)); // esp and ss are not on the stack
    cpu->idleThread.regs->vector = THREADREGISTERS_VECTOR_CUSTOMDSES;
    cpu->idleThread.regs->cs = flatKernelCS;
    cpu->idleThread.regs->eip = (uint32_t) cpu->idleThread.threadFunction;
    cpu->idleThread.regs->eflags = 1 << 9; // interrupts enabled
    cpu->idleThread.regs->ecx = (uint32_t) cpu->idleThread.threadFunctionParam;
    cpu->idleThread.regs->ds = flatUserDS;
    cpu->idleThread.regs->es = flatUserDS;
    cpu->idleThread.regs->gs = kernelGS;
    cpu->idleThread.regs->esp = (uint32_t) cpu->idleThread.regs - offsetof(ThreadRegisters, es); // Cpu_returnToUserMode starts by popping es
    cpu->idleThread.state = threadStateRunning;
    cpu->idleThread.queueNode.key = THREAD_IDLE_PRIORITY;
    cpu->currentThread = &cpu->idleThread;
    cpu->nextThread = cpu->currentThread;
    // Initialize the TSS
    cpu->tss.ss0 = flatKernelDS;
    cpu->tss.esp0 = cpu->idleThread.regs->esp;
    cpu->rescheduleNeeded = true;
    PriorityQueue_init(&cpu->readyQueue);
}

/** Loads the GDT (from the specified CPU structure), IDT, TSS and sysenter registers for the current processor. */
__attribute__((section(".boot"))) void Cpu_loadCpuTables(Cpu *cpu) {
    DescriptorTableLocation l;
    l.limit = gdtEntryCount * 8 - 1;
    l.base = (uint32_t) cpu->gdt;
    asm volatile(
    "    lgdt %0\n"
    "    ljmp %1, $1f\n"
    "1:  mov %2, %%eax\n"
    "    mov %%ax, %%ds\n"
    "    mov %%ax, %%es\n"
    "    xor %%eax, %%eax\n"
    "    mov %%ax, %%fs\n"
    "    mov %3, %%ax\n"
    "    mov %%ax, %%gs\n"
    "    mov %4, %%eax\n"
    "    mov %%ax, %%ss\n"
    : : "m" (l.limit), "i" (flatKernelCS), "i" (flatUserDS), "i" (kernelGS), "i" (flatKernelDS) : "eax", "memory");
    l.limit = 256 * 8 - 1;
    l.base = (uint32_t) Cpu_idt;
    asm volatile("    lidt %0\n" : : "m" (l.limit));
    asm volatile("    ltr %0\n" : : "r" ((uint16_t) tssSelector));
    Cpu_writeMsr(msrSysenterCs, flatKernelCS);
    Cpu_writeMsr(msrSysenterEip, (uintptr_t) &Cpu_sysenter);
    Cpu_writeMsr(msrSysenterEsp, (uintptr_t) &cpu->tss.esp0);
    uint32_t a, b, c, d;
    Cpu_cpuid(1, &a, &b, &c, &d);
    Video_printf("Cpu %d features: eax=0x%08X, ebx=0x%08X, ecx=0x%08X, edx=0x%08X.\n", cpu->lapicId, a, b, c, d);
}

/** Maps the Local APIC base address to virtual memory. */
__attribute__((section(".boot"))) static void Cpu_mapLocalApic(const MpConfigHeader *mpConfigHeader) {
    PageTableEntry *pd = phys2virt(physicalAddress((uintptr_t) &Boot_kernelPageDirectoryPhysicalAddress));
    FrameNumber ptFrameNumber = PhysicalMemory_allocate(NULL, permamapMemoryRegion);
    if (ptFrameNumber.v == 0) panic("Unable to allocate memory to map the Local APIC.");
    PageTableEntry *pt = frame2virt(ptFrameNumber);
    pd[CPU_LAPIC_VIRTUAL_ADDRESS >> 22] = (ptFrameNumber.v << PAGE_SHIFT) | ptPresent | ptWriteable;
    pt[(CPU_LAPIC_VIRTUAL_ADDRESS >> 12) & 0x3FF] = ((mpConfigHeader != NULL) ? mpConfigHeader->lapicPhysicalAddress : 0xFEE00000)
            | ptPresent | ptWriteable | ptGlobal | ptCacheDisable;
    //for (int i = 0; i < 1024; i++) Log_printf("Page directory %i: %08X\n", i, pd[i]);
    //for (int i = 0; i < 1024; i++) Log_printf("Page table %i: %08X\n", i, pt[i]);
}

/**
 * Creates and initializes the table of Cpu structures.
 * Called on the bootstrap processor during early kernel initialization.
 * @return Pointer to the Cpu structure of the bootstrap processor.
 */
__attribute__((section(".boot"))) Cpu *Cpu_initializeCpuStructs(const MpConfigHeader *mpConfigHeader) {
    // Count active processors
    if (mpConfigHeader != NULL) {
        const MpConfigProcessor *cp = (const MpConfigProcessor *) ((const uint8_t *) mpConfigHeader + sizeof(MpConfigHeader));
        Cpu_cpuCount = 0;
        for (size_t i = 0; i < mpConfigHeader->entryCount && cp->entryType == mpConfigProcessor; i++, cp++) {
            if ((cp->flags & 1) != 0) Cpu_cpuCount++;
        }
    } else {
        Cpu_cpuCount = 1;
    }
    Video_printf("Found %u enabled CPUs.\n", Cpu_cpuCount);
    // Initialize each CPU structure
    Cpu_mapLocalApic(mpConfigHeader);
    uint32_t currentLapicId = Cpu_readLocalApic(0x20) >> 24;
    Log_printf("The LAPIC ID of the current CPU is 0x%02X.\n", currentLapicId);
    Cpu *bootCpu = NULL;
    if (mpConfigHeader != NULL) {
        size_t cpuIndex = 0;
        const MpConfigProcessor *cp = (const MpConfigProcessor *) ((const uint8_t *) mpConfigHeader + sizeof(MpConfigHeader));
        for (size_t i = 0; i < mpConfigHeader->entryCount && cp->entryType == mpConfigProcessor; i++, cp++) {
            if ((cp->flags & 1) != 0) {
                Log_printf("Initializing CPU #%d with LAPIC ID 0x%02X, LAPIC version 0x%02X\n", cpuIndex, cp->lapicId, cp->lapicVersion);
                FrameNumber frameNumber = PhysicalMemory_allocate(NULL, permamapMemoryRegion);
                if (frameNumber.v == 0)
                    panic("Unable to allocate memory for CPU %d. Aborting.\n", cpuIndex);
                Cpu_cpus[cpuIndex] = frame2virt(frameNumber);
                Cpu *cpu = Cpu_cpus[cpuIndex];
                Cpu_initialize(cpu, cpuIndex, cp->lapicId); // TODO: cp->lapicVersion?
                if (currentLapicId == cp->lapicId) bootCpu = cpu;
                cpuIndex++;
            }
        }
    } else {
        Log_printf("Initializing the sole CPU #0 with LAPIC ID 0x%02X\n", currentLapicId);
        Cpu *cpu = Cpu_cpus[0];
        Cpu_initialize(cpu, 0, currentLapicId);
        bootCpu = cpu;
    }
    if (bootCpu == NULL) {
        panic("Unable to identify the boot CPU. Aborting\n");
    }
    CpuNode_theInstance.cpus = Cpu_cpus;
    CpuNode_theInstance.cpuCount = Cpu_cpuCount;
    Spinlock_init(&CpuNode_theInstance.lock);
    PriorityQueue_init(&CpuNode_theInstance.readyQueue);
    return bootCpu;
}

/** Sets up the Interrupt Descriptor Table. */
__attribute__((section(".boot"))) void Cpu_setupIdt() {
    for (size_t i = 0; i < 32; i++) Cpu_setIsr(i, Cpu_unhandledException, NULL);
    uintptr_t b = (uintptr_t) &Cpu_firstInterruptTrampoline;
    ptrdiff_t s = &Cpu_secondInterruptTrampoline - &Cpu_firstInterruptTrampoline;
    for (size_t i = 0; i < 256; i++, b += s) {
        GateDescriptor_set(&Cpu_idt[i], flatKernelCS, b, 0x8E00); // present, privilege0, is32bits, interruptGate
    }
    GateDescriptor_set(&Cpu_idt[spuriousInterruptVector], flatKernelCS,
            (uintptr_t) &Cpu_spuriousInterruptHandler, 0x8E00); // present, privilege0, is32bits, interruptGate
}

/** Starts processors other than the boot processor. */
__attribute__((section(".boot"))) void Cpu_startOtherCpus() {
    // Relocate the real-mode startup code for application processors
    memcpy(phys2virt(physicalAddress(0x4000)), phys2virt(physicalAddress(0x104000)), 1024);
    // Prepare for real-mode startup of application processors
    outp(0x70, 0x0F); // Select the "reset code" CMOS register
    outp(0x80, 0); // Just a small delay
    outp(0x71, 0x0A); // Select "resume execution by jump via 40h:0067h"
    *(uint32_t *) phys2virt(physicalAddress(0x467)) = 0x04000000; // Start execution from physical address 0x4000
    // Initialize the Local APIC of the bootstrap (this) processor
    Cpu *cpu = Cpu_getCurrent();
    Log_printf("Enabling LAPIC on bootstrap processor (CPU #%d, LAPIC ID 0x%02X, Cpu struct at %p).\n", cpu->index, cpu->lapicId, cpu);
    Cpu_writeLocalApic(lapicSpuriousInterrupt, 0x1FF); // LAPIC enabled, Focus Check disabled, spurious vector 0xFF
    Log_printf("Sending INIT IPI.\n");
    Cpu_writeLocalApic(lapicInterruptCommandLow, 0xC4500); // INIT assert, level triggered, to all excluding self
    Log_printf("Sending INIT De-assert IPI.\n");
    Cpu_writeLocalApic(lapicInterruptCommandLow, 0xCC500); // INIT de-assert, level triggered, to all including self
    AcpiPmTimer_busyWait(ACPI_PMTIMER_FREQUENCY * 10E-3); // 10ms
    Log_printf("Broadcasting the first SIPI.\n");
    Cpu_writeLocalApic(lapicInterruptCommandLow, 0xC4604); // SIPI to all excluding self, start from physical address 0x04000
    AcpiPmTimer_busyWait(ACPI_PMTIMER_FREQUENCY * 200E-6); // 200us
    Log_printf("Broadcasting the second SIPI.\n");
    Cpu_writeLocalApic(lapicInterruptCommandLow, 0xC4604); // SIPI to all excluding self, start from physical address 0x04000
    AcpiPmTimer_busyWait(ACPI_PMTIMER_FREQUENCY * 200E-6); // 200us
    Log_printf("Multiprocessor initialization by BSP completed.\n");
}
