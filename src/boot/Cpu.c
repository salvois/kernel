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

extern uint8_t Cpu_firstInterruptTrampoline;  // from Cpu_asm.S
extern uint8_t Cpu_secondInterruptTrampoline; // from Cpu_asm.S
extern uint8_t Cpu_spuriousInterruptHandler;  // from Cpu_asm.S
extern uint8_t Cpu_sysenter; // from Cpu_asm.S

__attribute__((section(".boot")))
static void SegmentDescriptor_set(CpuDescriptor *d, uint32_t base, uint32_t limit, uint32_t access) {
    d->word0 = (base << 16) | (limit & 0x0000FFFF);
    d->word1 = (base & 0xFF000000) | (access & 0x00F0FF00) | (limit & 0x000F0000) | ((base & 0x00FF0000) >> 16);
}

__attribute__((section(".boot")))
static void GateDescriptor_set(CpuDescriptor *d, uint16_t selector, uint32_t offset, uint16_t access) {
    d->word0 = ((uint32_t) selector << 16) | (offset & 0x0000FFFF);
    d->word1 = (offset & 0xFFFF0000) | access;
}

__attribute__((section(".boot")))
static void Cpu_setupGlobalDescriptorTable(Cpu *cpu) {
    SegmentDescriptor_set(&cpu->gdt[nullSelector], 0, 0, 0);
    SegmentDescriptor_set(&cpu->gdt[flatKernelCS >> 3], 0, 0xFFFFF, 0xC09A00); // page granular, 32-bit, non-system, code, r/x, ring 0
    SegmentDescriptor_set(&cpu->gdt[flatKernelDS >> 3], 0, 0xFFFFF, 0xC09200); // page granular, 32-bit, non-system, data, r/w, ring 0
    SegmentDescriptor_set(&cpu->gdt[flatUserCS >> 3], 0, 0xFFFFF, 0xC0FA00); // page granular, 32-bit, non-system, code, r/x, ring 3
    SegmentDescriptor_set(&cpu->gdt[flatUserDS >> 3], 0, 0xFFFFF, 0xC0F200); // page granular, 32-bit, non-system, data, r/w, ring 3
    SegmentDescriptor_set(&cpu->gdt[tssSelector >> 3], (uint32_t) &cpu->tss, sizeof(Tss) - 1, 0x408900); // byte granular, 32-bit, 32-bit TSS available
    SegmentDescriptor_set(&cpu->gdt[kernelGS >> 3], (uint32_t) cpu, sizeof(Cpu) - 1, 0x409200); // byte granular, 32-bit, 32-bit, writable
}

__attribute__((section(".boot")))
static void Cpu_setupIdleThread(Cpu *cpu) {
    cpu->idleThread.threadFunction = Cpu_idleThreadFunction;
    cpu->idleThread.cpu = cpu;
    cpu->idleThread.priority = THREAD_IDLE_PRIORITY;
    cpu->idleThread.queueNode.key = THREAD_IDLE_PRIORITY;
    cpu->idleThread.regs = &cpu->idleThread.regsBuf;
    cpu->idleThread.kernelThread = true;
    cpu->idleThread.regs->gs = kernelGS;
    cpu->idleThread.regs->eip = (uint32_t) cpu->idleThread.threadFunction;
    cpu->idleThread.regs->cs = flatKernelCS;
    cpu->idleThread.regs->eflags = CpuFlag_interruptEnable;
    cpu->idleThread.state = threadStateRunning;
}

__attribute__((section(".boot")))
static void Cpu_initialize(Cpu *cpu, size_t index, size_t lapicId) {
    memzero(cpu, sizeof(Cpu));
    cpu->index = index;
    cpu->lapicId = lapicId;
    cpu->thisCpu = cpu;
    cpu->cpuNode = &CpuNode_theInstance;
    cpu->active = true;
    Cpu_setupGlobalDescriptorTable(cpu);
    Cpu_setupIdleThread(cpu);
    cpu->currentThread = &cpu->idleThread;
    cpu->nextThread = cpu->currentThread;
    cpu->tss.ss0 = flatKernelDS;
    cpu->tss.esp0 = (uint32_t) cpu->idleThread.regs + offsetof(ThreadRegisters, edi); // Cpu_returnToUserMode starts by popping EDI
    cpu->rescheduleNeeded = true;
    PriorityQueue_init(&cpu->readyQueue);
}

typedef struct DescriptorTableLocation {
    uint16_t pad;
    uint16_t limit;
    uint32_t base;
} DescriptorTableLocation;

/** Loads the GDT (from the specified CPU structure), IDT, TSS and sysenter registers for the current processor. */
__attribute__((section(".boot")))
void Cpu_loadCpuTables(Cpu *cpu) {
    DescriptorTableLocation gdtLocation = { .limit = gdtEntryCount * 8 - 1, .base = (uint32_t) cpu->gdt };
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
    : : "m" (gdtLocation.limit), "i" (flatKernelCS), "i" (flatUserDS), "i" (kernelGS), "i" (flatKernelDS) : "eax", "memory");
    DescriptorTableLocation idtLocation = { .limit = 256 * 8 - 1, .base = (uint32_t) Cpu_idt };
    asm volatile("    lidt %0\n" : : "m" (idtLocation.limit));
    asm volatile("    ltr %0\n" : : "r" ((uint16_t) tssSelector));
    Cpu_writeMsr(msrSysenterCs, flatKernelCS);
    Cpu_writeMsr(msrSysenterEip, (uintptr_t) &Cpu_sysenter);
    Cpu_writeMsr(msrSysenterEsp, (uintptr_t) &cpu->tss.esp0);
    uint32_t a, b, c, d;
    Cpu_cpuid(1, &a, &b, &c, &d);
    Video_printf("Cpu %d features: eax=0x%08X, ebx=0x%08X, ecx=0x%08X, edx=0x%08X.\n", cpu->lapicId, a, b, c, d);
}

__attribute__((section(".boot")))
static void Cpu_mapLocalApic(const MpConfigHeader *mpConfigHeader) {
    PageTable *pd = (PageTable *) &Boot_kernelPageDirectory;
    FrameNumber ptFrameNumber = PhysicalMemory_allocate(NULL, permamapMemoryRegion);
    if (ptFrameNumber.v == 0)
        panic("Unable to allocate memory to map the Local APIC.");
    PageTable *pt = frame2virt(ptFrameNumber);
    pd->entries[CPU_LAPIC_VIRTUAL_ADDRESS >> 22] = (ptFrameNumber.v << PAGE_SHIFT) | ptPresent | ptWriteable;
    pt->entries[(CPU_LAPIC_VIRTUAL_ADDRESS >> 12) & 0x3FF] =
            ((mpConfigHeader != NULL) ? mpConfigHeader->lapicPhysicalAddress : CPU_LAPIC_DEFAULT_PHYSICAL_ADDRESS.v) | ptPresent | ptWriteable | ptGlobal | ptCacheDisable;
}

typedef struct CpuInitializationClosure {
    int currentLapicId;
    Cpu *bootCpu;
} CpuInitializationClosure;

__attribute__((section(".boot")))
static void Cpu_allocateAndInitialize(void *closure, int lapicId) {
    CpuInitializationClosure *cpuInitializationClosure = (CpuInitializationClosure *) closure;
    Log_printf("Initializing CPU #%d with LAPIC ID 0x%02X\n", Cpu_cpuCount, lapicId);
    FrameNumber frameNumber = PhysicalMemory_allocate(NULL, permamapMemoryRegion);
    if (frameNumber.v == 0)
        panic("Unable to allocate memory for CPU %d. Aborting.\n", Cpu_cpuCount);
    Cpu_cpus[Cpu_cpuCount] = frame2virt(frameNumber);
    Cpu *cpu = Cpu_cpus[Cpu_cpuCount];
    Cpu_initialize(cpu, Cpu_cpuCount, lapicId); // TODO: cp->lapicVersion?
    if (cpuInitializationClosure->currentLapicId == lapicId)
        cpuInitializationClosure->bootCpu = cpu;
    Cpu_cpuCount++;
}

/**
 * Creates and initializes the table of Cpu structures.
 * Called on the bootstrap processor during early kernel initialization.
 * @return Pointer to the Cpu structure of the bootstrap processor.
 */
__attribute__((section(".boot")))
Cpu *Cpu_initializeCpuStructs(const MpConfigHeader *mpConfigHeader) {
    memzero(Cpu_cpus, sizeof(Cpu_cpus));
    Cpu_cpuCount = 0;
    Cpu_mapLocalApic(mpConfigHeader);
    CpuInitializationClosure closure = { .currentLapicId = Cpu_readLocalApic(lapicIdRegister) >> 24, .bootCpu = NULL };
    Log_printf("The LAPIC ID of the current CPU is 0x%02X.\n", closure.currentLapicId);
    if (mpConfigHeader != NULL)
        MultiProcessorSpecification_scanProcessors(mpConfigHeader, &closure, Cpu_allocateAndInitialize);
    else
        Cpu_allocateAndInitialize(&closure, closure.currentLapicId);
    if (closure.bootCpu == NULL)
        panic("Unable to identify the boot CPU. Aborting\n");
    Video_printf("Found %u enabled CPUs.\n", Cpu_cpuCount);
    CpuNode_theInstance.cpus = Cpu_cpus;
    CpuNode_theInstance.cpuCount = Cpu_cpuCount;
    Spinlock_init(&CpuNode_theInstance.lock);
    PriorityQueue_init(&CpuNode_theInstance.readyQueue);
    return closure.bootCpu;
}

__attribute__((section(".boot")))
void Cpu_setupInterruptDescriptorTable() {
    for (size_t i = 0; i < 32; i++)
        Cpu_setIsr(i, Cpu_unhandledException, NULL);
    uintptr_t b = (uintptr_t) &Cpu_firstInterruptTrampoline;
    ptrdiff_t s = &Cpu_secondInterruptTrampoline - &Cpu_firstInterruptTrampoline;
    const uint16_t access = 0x8E00; // present, privilege0, is32bits, interruptGate
    for (size_t i = 0; i < 256; i++, b += s)
        GateDescriptor_set(&Cpu_idt[i], flatKernelCS, b, access);
    GateDescriptor_set(&Cpu_idt[spuriousInterruptVector],
                       flatKernelCS,
                       (uintptr_t) &Cpu_spuriousInterruptHandler, access);
}

__attribute__((section(".boot")))
void Cpu_startOtherCpus() {
    // Relocate the real-mode startup code for application processors
    memcpy(phys2virt(physicalAddress(0x4000)), phys2virt(physicalAddress(0x104000)), 1024);
    // Prepare for real-mode startup of application processors
    outp(0x70, 0x0F); // Select the "reset code" CMOS register
    outp(0x80, 0); // Just a small delay
    outp(0x71, 0x0A); // Select "resume execution by jump via 40h:0067h"
    *(uint32_t *) phys2virt(physicalAddress(0x467)) = 0x04000000; // Start execution from physical address 0x4000
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
