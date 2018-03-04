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

/** A structure holding base address and limit for a descriptor table, properly aligned. */
typedef struct DescriptorTableLocation {
    uint16_t pad;
    uint16_t limit;
    uint32_t base;
} DescriptorTableLocation;

/** Global table of Cpu structures in use, each representing a logical processor. */
Cpu* Cpu_cpus;
/** Global count of Cpu structures in use. */
size_t Cpu_cpuCount;
/** Global table of interrupt service routines for each interrupt number. */
static IsrTableEntry Cpu_isrTable[256];
/** The one Interrupt Descriptor Table. */
static CpuDescriptor Cpu_idt[256];
/**
 * Length of time slices in nanoseconds for each nice level.
 * Nice levels are implemented as variable length time slices, with each nice
 * level having a time slice 10% longer than the next nicer level.
 */
unsigned Cpu_timesliceLengths[NICE_LEVELS] = {
    41144675, // lowest, 41 ms
    37404250,
    34003864,
    30912604,
    28102368,
    25547608,
    23225099,
    21113727,
    19194298,
    17449362,
    15863057,
    14420961,
    13109965,
    11918150,
    10834682,
    9849711,
    8954283,
    8140258,
    7400235,
    6727487,
    6115898, // normal, 6 ms
    5559908,
    5054462,
    4594966,
    4177242,
    3797493,
    3452267,
    3138425,
    2853114,
    2593740,
    2357946,
    2143588,
    1948717,
    1771561,
    1610510,
    1464100,
    1331000,
    1210000,
    1100000,
    1000000 // nicest, 1 ms
};
/**
 * A small tolerance when checking if a time slice has elapsed.
 * We don't run a thread that has so little time slice remaining.
 */
#define TIMESLICE_TOLERANCE 10000 // 1/100 of a millisecond

extern uint8_t Cpu_firstInterruptTrampoline;  // from Cpu_asm.S
extern uint8_t Cpu_secondInterruptTrampoline; // from Cpu_asm.S
extern uint8_t Cpu_spuriousInterruptHandler;  // from Cpu_asm.S
extern uint8_t Cpu_sysenter; // from Cpu_asm.S

/** Interrupt service routine for CPU exceptions we don't know how to handle. */
__attribute__((fastcall)) static void Cpu_unhandledException(Cpu *cpu, void *param) {
    ThreadRegisters *regs = cpu->currentThread->regs;
    if (regs->vector == 14) {
        Video_printf("Faulting address: %p.\n", Cpu_getFaultingAddress());
    }
    panic("Unhandled exception %d on CPU 0x%02X, error code=0x%X\n"
            "  regs=%p\n"
            "  EAX=0x%08X\n"
            "  ECX=0x%08X\n"
            "  EDX=0x%08X\n"
            "  EBX=0x%08X\n"
            "  EBP=0x%08X\n"
            "  ESI=0x%08X\n"
            "  EDI=0x%08X\n"
            "  EIP=0x%08X\n"
            "  CS=0x%04X\n"
            "  EFLAGS=0x%08X\n"
            "  ESP=0x%08X\n"
            "  SS=0x%08X\n"
            "  DS=0x%08X\n"
            "  ES=0x%08X\n"
            "  FS=0x%08X\n"
            "  GS=0x%08X\n"
            "System halted.\n",
            regs->vector, cpu->lapicId, regs->error, regs, regs->eax, regs->ecx, regs->edx, regs->ebx,
            regs->ebp, regs->esi, regs->edi, regs->eip, regs->cs, regs->eflags, regs->esp, regs->ss, regs->ds, regs->es, regs->fs, regs->gs);
}

/** Interrupt service routine for interrupts we don't handle. */
__attribute__((fastcall)) static void Cpu_doNothingInterrupt(Cpu *cpu, void *param) {
    ThreadRegisters *regs = cpu->currentThread->regs;
    Log_printf("Unused interrupt %d on CPU 0x%02X, error code=0x%X\n"
            "  EAX=0x%08X\n"
            "  ECX=0x%08X\n"
            "  EDX=0x%08X\n"
            "  EBX=0x%08X\n"
            "  EBP=0x%08X\n"
            "  ESI=0x%08X\n"
            "  EDI=0x%08X\n"
            "  EIP=0x%08X\n"
            "  CS=0x%04X\n"
            "  EFLAGS=0x%08X\n"
            "Continuing.\n",
            regs->vector, cpu->lapicId, regs->error, regs->eax, regs->ecx, regs->edx, regs->ebx,
            regs->ebp, regs->esi, regs->edi, regs->eip, regs->cs, regs->eflags);
}

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
__attribute__((section(".boot"))) static void Cpu_initialize(Cpu *cpu, size_t lapicId) {
    memzero(cpu, sizeof(Cpu));
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
    cpu->idleThread.priority = 255;
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
    cpu->idleThread.queueNode.key = 0xFF;
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
    PageTableEntry *pd = phys2virt((uintptr_t) &Boot_kernelPageDirectoryPhysicalAddress);
    PageTableEntry *pt = frame2virt(PhysicalMemory_alloc(NULL, permamapMemoryRegion));
    if (pt == NULL) panic("Unable to allocate memory to map the Local APIC.");
    pd[CPU_LAPIC_VIRTUAL_ADDRESS >> 22] = (virt2frame(pt) << PAGE_SHIFT) | ptPresent | ptWriteable;
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
    // Allocate the array of CPU structures
    uintptr_t b = virt2frame(PhysicalMemory_frameDescriptors) + ceilToFrame(PhysicalMemory_totalMemoryFrames * sizeof(Frame));
    size_t s = ceilToFrame(Cpu_cpuCount * sizeof(Cpu));
    size_t a = PhysicalMemory_allocBlock(b, b + s);
    if (a != s) {
        panic("Unable to mark memory for CPU structures as allocated. Aborting.\n");
    }
    Cpu_cpus = frame2virt(b);
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
                Cpu *cpu = &Cpu_cpus[cpuIndex];
                Cpu_initialize(cpu, cp->lapicId); // TODO: cp->lapicVersion?
                if (currentLapicId == cp->lapicId) bootCpu = cpu;
                cpuIndex++;
            }
        }
    } else {
        Log_printf("Initializing the sole CPU #0 with LAPIC ID 0x%02X\n", currentLapicId);
        Cpu *cpu = &Cpu_cpus[0];
        Cpu_initialize(cpu, currentLapicId);
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
    memcpy(phys2virt(0x4000), phys2virt(0x104000), 1024);
    // Prepare for real-mode startup of application processors
    outp(0x70, 0x0F); // Select the "reset code" CMOS register
    outp(0x80, 0); // Just a small delay
    outp(0x71, 0x0A); // Select "resume execution by jump via 40h:0067h"
    *(uint32_t *) phys2virt(0x467) = 0x04000000; // Start execution from physical address 0x4000
    // Initialize the Local APIC of the bootstrap (this) processor
    Cpu *cpu = Cpu_getCurrent();
    Log_printf("Enabling LAPIC on bootstrap processor (CPU #%d, LAPIC ID 0x%02X, Cpu struct at %p).\n", cpu - Cpu_cpus, cpu->lapicId, cpu);
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

/**
 * Makes the specified thread the current thread of the specified CPU,
 * that is assumed to be the current CPU.
 */
static void Cpu_switchToThread(Cpu *cpu, Thread *next) {
    Thread *curr = cpu->currentThread;
    Log_printf("Cpu %d switching from thread %p (prio=%d) to thread %p (prio=%d).\n", cpu->lapicId,
            curr, curr->queueNode.key,
            next, next->queueNode.key);
    ThreadRegisters *cr = curr->regs;
    ThreadRegisters *nr = next->regs;
    if (UNLIKELY(nr->ldt != 0)) {
        // TODO: set GDT entry
        asm volatile("lldt %0" : : "r" ((uint16_t) nr->ldt));
    } else if (UNLIKELY(cr->ldt != 0)) {
        asm volatile("lldt %0" : : "r" ((uint16_t) 0));
    }
    if (!curr->kernelThread) {
        if (!next->kernelThread) {
            asm volatile("mov %%fs, %0" : "=g" (cr->fs));
            asm volatile("mov %0, %%fs" : : "g" (nr->fs));
            asm volatile("mov %%gs, %0" : "=g" (cr->gs));
            asm volatile("mov %0, %%gs" : : "g" (nr->gs));
        } else {
            asm volatile("mov %0, %%gs" : : "r" (kernelGS));
        }
    } else if (!next->kernelThread) {
        asm volatile("mov %0, %%fs" : : "g" (nr->fs));
        asm volatile("mov %0, %%gs" : : "g" (nr->gs));
    }
    // TODO: optionally save FPU/SSE context
    if (next->task != curr->task) {
        AddressSpace_activate(&next->task->addressSpace);
    }
    next->state = threadStateRunning;
    next->cpu = cpu;
    cpu->currentThread = next;
    cpu->tss.esp0 = (uint32_t) nr + sizeof(ThreadRegisters); // unused for kernel-mode threads
}

/**
 * Schedules the specified CPU, that is assumed to be the current CPU.
 *
 * Each thread has a priority, ranging from 0 (highest priority) to 254 (lowest
 * priority), 255 being reserved for the built-in idle thread. A thread with
 * higher priority always preempt a thread with lower priority. Threads with
 * the same priority are run round robin on the scheduler timer.
 * A thread has a nominal priority and an effective priority. The effective
 * priority may be different than the nominal priority when a thread receives
 * a message from another thread, inheriting its (effective) priority.
 *
 * Whenever a new thread is made the current thread, the scheduler timer
 * is reprogrammed. The scheduler timer is not started if the current thread
 * is the idle thread, or there are no ready threads, or the first ready thread
 * has a lower priority level than the current thread.
 * This means that the CPU operates in tickless mode unless there are multiple
 * threads with the same (effective) priority level.
 *
 * Each thread is given a time slice, whose length is inversely proportional
 * to the nice level of the thread. When the time slice elapses, the thread
 * is put on the back of the ready queue among threads with the same priority.
 * For fairness, when the thread is preempted or blocks it is put on the front.
 * This means that a thread that consumes little of its time slice before
 * sleeping is very likely to run soon when it is awakened.
 *
 * Each CPU knows the thread it currently runs (possibly its idle thread)
 * and the next thread candidate to run (possibly the current thread itself).
 * The next thread can be selected by the CPU itself or another CPU when
 * awakening threads (if the thread being awakened has to preempt a CPU),
 * or by the scheduler.
 * A ready queue shared by all CPUs of a node holds threads ready to run.
 * The ready queue does not contain current threads nor next threads.
 * The scheduler kicks in on a CPU because either:
 * - the next thread is different than the current thread (a higher priority
 *   thread has been awakened);
 * - the current thread is going to block;
 * - the per-CPU scheduler timer interrupt has fired.
 * 
 * Benchmarks:
 * Atom N450 1x Sysenter: crash with exception 6
 * Atom N450 2x EndlessLoop: 584 TSC tick per interrupt
 * Atom N450 8x EndlessLoop: 617 TSC tick per interrupt
 * Atom N450 16x EndlessLoop: 620 TSC tick per interrupt
 * Atom N450 64x EndlessLoop: 657 TSC tick per interrupt
 * Atom N450 256x EndlessLoop: 729 TSC tick per interrupt
 * i5-3570K 1x Sysenter: 170 TSC ticks per system call
 * i5-3570K 8x EndlessLoop: 664-707 TSC tick per interrupt
 * i5-3570K 16x EndlessLoop: 647-680 TSC tick per interrupt
 * i5-3570K 64x EndlessLoop: 696-710 TSC tick per interrupt
 * i5-3570K 256x EndlessLoop: 681-701 TSC tick per interrupt
 * i5-3570K 8x EndlessLoop 1 CPU: 327 TSC tick per interrupt
 * i5-3570K 8x EndlessLoop 2 CPUs: 588-599 TSC tick per interrupt
 * i5-3570K 8x EndlessLoop 3 CPUs: 641-688 TSC tick per interrupt
 */
static void Cpu_schedule(Cpu *cpu) {
    cpu->rescheduleNeeded = false;
    PriorityQueue *rq = &cpu->cpuNode->readyQueue;
    Thread *curr = cpu->currentThread;
    // Compute elapsed time
    bool timesliced = false;
    uint64_t t = Tsc_read();
    uint64_t dt = t - cpu->lastScheduleTime; // may overflow if we run for 10s of years :)
    cpu->lastScheduleTime = t;
    if (dt > UINT32_MAX) dt = UINT32_MAX;
    unsigned dtns = Tsc_convertTicksToNanoseconds(&cpu->tsc, dt);
    if (curr->timesliceRemaining <= TIMESLICE_TOLERANCE + dtns) {
        timesliced = true;
        curr->timesliceRemaining = Cpu_timesliceLengths[curr->nice];
    } else {
        curr->timesliceRemaining -= dtns;
    }
    Spinlock_lock(&cpu->cpuNode->lock);
    // Search for next thread
    if (cpu->nextThread == curr) {
        Thread *next = curr;
        if (curr->state == threadStateBlocked) {
            next = !PriorityQueue_isEmpty(rq) ? Thread_fromQueueNode(PriorityQueue_poll(rq)) : &cpu->idleThread;
        } else {
            assert(curr->state == threadStateRunning);
            if (!PriorityQueue_isEmpty(rq)) {
                Thread *h = Thread_fromQueueNode(PriorityQueue_peek(rq));
                Log_printf("Cpu %d: curr=%d h=%d timesliced=%d.\n", cpu->lapicId, curr->queueNode.key, h->queueNode.key, timesliced);
                if (Thread_isHigherPriority(h, curr) || (!Thread_isHigherPriority(curr, h) && timesliced)) {
                    if (curr != &cpu->idleThread) {
                        next = Thread_fromQueueNode(PriorityQueue_pollAndInsert(rq, &curr->queueNode, !timesliced));
                    } else {
                        next = Thread_fromQueueNode(PriorityQueue_poll(rq));
                    }
                    curr->state = threadStateReady;
                }
            } // otherwise do nothing, currentThread (== nextThread) is made running again
        }
        cpu->nextThread = next;
    } else {
        if (curr->state != threadStateBlocked && curr != &cpu->idleThread) {
            PriorityQueue_insertFront(rq, &curr->queueNode);
        }
    }
    if (cpu->nextThread != curr) {
        Cpu_switchToThread(cpu, cpu->nextThread);
        timesliced = true;
    }
    // Setup timeslice interrupt
    if (timesliced) {
        cpu->timesliceTimerEnabled = cpu->currentThread != &cpu->idleThread && !PriorityQueue_isEmpty(rq)
                && PriorityQueue_peek(rq)->key == cpu->currentThread->queueNode.key;
        Spinlock_unlock(&cpu->cpuNode->lock);
        if (cpu->timesliceTimerEnabled) {
            assert(cpu->currentThread->timesliceRemaining > TIMESLICE_TOLERANCE);
            uint32_t ticks = LapicTimer_convertNanosecondsToTicks(&cpu->lapicTimer, cpu->currentThread->timesliceRemaining);
            Log_printf("Cpu %d enabling timeslice in %d LAPIC timer ticks.\n", cpu->lapicId, ticks);
            Cpu_writeLocalApic(lapicTimerInitialCount, ticks);
        }
    } else {
        Spinlock_unlock(&cpu->cpuNode->lock);
    }
}

/**
 * Called before exiting the kernel, either after a system call or an interrupt,
 * to reschedule the specified CPU (assumed to be the current CPU) if needed.
 */
void Cpu_exitKernel(Cpu *cpu) {
    if (cpu->rescheduleNeeded) {
        Cpu_schedule(cpu);
    }
    Log_printf("Cpu %d exiting kernel with ThreadRegisters at %p.\n", cpu->lapicId, cpu->currentThread->regs);
}

/**
 * Dispatches the appropriate system call ont he specified CPU, that is
 * assumed to be the current CPU.
 */
static void Cpu_handleSysenter(Cpu *cpu) {
    ThreadRegisters *regs = cpu->currentThread->regs;
    switch (regs->eax & 0xFF) {
        case 1:
            regs->eax = Syscall_createChannel(cpu->currentThread->task);
            break;
        case 2:
            regs->eax = Syscall_deleteCapability(cpu->currentThread->task, regs->ebx);
            break;
#if 0
        case 3:
            regs->eax = Syscall_sendMessage(cpu, regs->eax >> 8, regs->ebx);
            break;
        case 4:
            regs->eax = Syscall_receiveMessage(cpu->currentThread, regs->ebx, (uint8_t *) regs->esi, regs->edi);
            break;
        case 5:
            regs->eax = Syscall_readMessage(cpu->currentThread, regs->ebx, regs->ebp, (uint8_t *) regs->esi, regs->edi);
            break;
#endif
        case 127:
            Video_printf("System call on Cpu 0x%02X\n"
                    "  regs=%p\n"
                    "  vector=0x%08X\n"
                    "  EAX=0x%08X\n"
                    "  ECX=0x%08X\n"
                    "  EDX=0x%08X\n"
                    "  EBX=0x%08X\n"
                    "  EBP=0x%08X\n"
                    "  ESI=0x%08X\n"
                    "  EDI=0x%08X\n"
                    "  EIP=0x%08X\n"
                    "  CS=0x%08X\n"
                    "  DS=0x%08X\n"
                    "  ES=0x%08X\n"
                    "  EFLAGS=0x%08X\n"
                    "Continuing.\n",
                    cpu->lapicId, regs, regs->vector, regs->eax, regs->ecx, regs->edx, regs->ebx,
                    regs->ebp, regs->esi, regs->edi, regs->eip, regs->cs, regs->ds, regs->es, regs->eflags);
            break;
        default:
            regs->eax = ENOSYS;
            break;
    }
    Cpu_exitKernel(cpu);
}

/**
 * Dispatches the appropriate interrupt handler or invokes the system call dispatcher.
 * Called in Cpu_asm.S.
 * @param cpu The current CPU.
 * @return Pointer to the ThreadRegister structure of the thread to resume.
 */
__attribute__((fastcall)) ThreadRegisters *Cpu_handleSyscallOrInterrupt(Cpu *cpu) {
    unsigned vector = cpu->currentThread->regs->vector & THREADREGISTERS_VECTOR_MASK;
    Log_printf("%s: vector %d.\n", __func__, vector);
    if (vector == THREADREGISTERS_VECTOR_SYSENTER) {
        Cpu_handleSysenter(cpu);
    } else {
        uint64_t beginTsc = Tsc_read();
        cpu->interruptCount++;
        assert(cpu->currentThread->regs->vector < 256);
        switch (cpu->currentThread->regs->vector) {
            case lapicTimerVector:
                cpu->rescheduleNeeded = true;
                Cpu_writeLocalApic(lapicEoi, 0);
                break;
            case rescheduleIpiVector:
                Video_printf("Reschedule IPI on CPU 0x%02X.\n", cpu->lapicId);
                cpu->rescheduleNeeded = true;
                Cpu_writeLocalApic(lapicEoi, 0);
                break;
            default:
                Cpu_unhandledException(cpu, NULL);
                break;
        }
        //IsrTableEntry *isrTableEntry = &Cpu_isrTable[cpu->currentThread->regs->vector];
        //isrTableEntry->isr(isrTableEntry->param, cpu->currentThread->regs);
        //uint32_t returnSp = (uint32_t) cpu->currentThread->regs + offsetof(ThreadRegisters, es);
        Cpu_exitKernel(cpu);
        cpu->interruptTsc += Tsc_read() - beginTsc;
        if ((cpu->interruptCount & 0xFFF) == 0) {
            Video_printf("Cpu %d interrupt TSC=0x%016llX, count=0x%016llX.\n", cpu->lapicId, cpu->interruptTsc, cpu->interruptCount);
        }
    }
    return cpu->currentThread->regs;
}

/** Sets the interrupt service routine for the specified interrupt vector. */
void Cpu_setIsr(size_t vector, Isr isr, void *param) {
    if (vector < 256) {
        Cpu_isrTable[vector].isr = Cpu_doNothingInterrupt;
        writeBarrier();
        Cpu_isrTable[vector].param = param;
        writeBarrier();
        Cpu_isrTable[vector].isr = isr;
    }
}

/** Sends a reschedule interprocessor interrupt to the specified CPU. */
void Cpu_sendRescheduleInterrupt(Cpu *cpu) {
    Cpu_writeLocalApic(lapicInterruptCommandHigh, cpu->lapicId << 24); // Destination processor
    Cpu_writeLocalApic(lapicInterruptCommandLow, 0x4000 | rescheduleIpiVector); // IPI to physical destination (no shorthand), assert, fixed vector
}

/** Sends a TLB shootdown interprocessor interrupt to the specified CPU. */
void Cpu_sendTlbShootdownIpi(Cpu *cpu) {
    Cpu_writeLocalApic(lapicInterruptCommandHigh, cpu->lapicId << 24); // Destination processor
    Cpu_writeLocalApic(lapicInterruptCommandLow, 0x4000 | tlbShootdownIpiVector); // IPI to physical destination (no shorthand), assert, fixed vector
}
