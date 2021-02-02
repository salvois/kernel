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

/** Global table of Cpu structures in use, each representing a logical processor. */
Cpu *Cpu_cpus[MAX_CPU_COUNT];
/** Global count of Cpu structures in use. */
size_t Cpu_cpuCount;
/** The one Interrupt Descriptor Table. */
CpuDescriptor Cpu_idt[256];
/** Global table of interrupt service routines for each interrupt number. */
static IsrTableEntry Cpu_isrTable[256];
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

void Cpu_logRegisters(Cpu *cpu) {
    ThreadRegisters *regs = cpu->currentThread->regs;
    uint32_t stackPointer = Cpu_readStackPointer();
    uint32_t fs = Cpu_readFs();
    uint32_t gs = Cpu_readGs();
    Log_printf("Dump of registers of Cpu 0x%02X (0x%08X) saved at %p\n"
               "    ldt    0x%08X\n"
               "    fs     0x%08X\n"
               "    gs     0x%08X\n"
               "    es     0x%08X\n"
               "    ds     0x%08X\n"
               "    edi    0x%08X\n"
               "    esi    0x%08X\n"
               "    ebp    0x%08X\n"
               "    ebx    0x%08X\n"
               "    edx    0x%08X\n"
               "    ecx    0x%08X\n"
               "    eax    0x%08X\n"
               "    vector 0x%08X\n"
               "    error  0x%08X\n"
               "    eip    0x%08X\n"
               "    cs     0x%08X\n"
               "    eflags 0x%08X\n"
               "    esp    0x%08X\n"
               "    ss     0x%08X\n"
               "Current stack pointer=0x%08X, fs=0x%08X, gs=0x%08X, kernelEntryCount=%d.\n",
            cpu->lapicId, cpu, regs, regs->ldt, regs->fs, regs->gs, regs->es, regs->ds,
            regs->edi, regs->esi, regs->ebp, regs->ebx, regs->edx, regs->ecx, regs->eax,
            regs->vector, regs->error, regs->eip, regs->cs, regs->eflags, regs->esp, regs->ss,
            stackPointer, fs, gs, cpu->kernelEntryCount);
}

/** Interrupt service routine for CPU exceptions we don't know how to handle. */
__attribute__((fastcall)) void Cpu_unhandledException(Cpu *cpu, void *param) {
    ThreadRegisters *regs = cpu->currentThread->regs;
    if (regs->vector == 14) {
        Log_printf("Faulting address: %p.\n", Cpu_getFaultingAddress());
    }
    Log_printf("Exiting %s on CPU 0x%02X.\n", __func__, cpu->lapicId);
    Cpu_logRegisters(cpu);
    panic("Unhandled exception %d on CPU 0x%02X, error code=0x%X. System halted.\n", regs->vector, cpu->lapicId, regs->error);
}

/** Interrupt service routine for interrupts we don't handle. */
__attribute__((fastcall)) static void Cpu_doNothingInterrupt(Cpu *cpu, void *param) {
    Log_printf("Unused interrupt %d on CPU 0x%02X, error code=0x%X. Continuing...\n, regs->vector, cpu->lapicId");
    Cpu_logRegisters(cpu);
}

/**
 * Makes the specified thread the current thread of the specified CPU,
 * that is assumed to be the current CPU.
 */
void Cpu_switchToThread(Cpu *cpu, Thread *next) {
    Thread *curr = cpu->currentThread;
//    Log_printf("Cpu %d switching from thread %p (prio=%d) to thread %p (prio=%d).\n", cpu->lapicId,
//            curr, curr->queueNode.key,
//            next, next->queueNode.key);
    ThreadRegisters *cr = curr->regs;
    ThreadRegisters *nr = next->regs;
    if (UNLIKELY(nr->ldt != 0)) {
        // TODO: set GDT entry
        Cpu_loadLdt(nr->ldt);
    } else if (UNLIKELY(cr->ldt != 0)) {
        Cpu_loadLdt(0);
    }
    if (!curr->kernelThread) {
        cr->fs = Cpu_readFs();
        cr->gs = Cpu_readGs();
        if (!next->kernelThread) {
            Cpu_writeFs(nr->fs);
            Cpu_writeGs(nr->gs);
        } else {
            Cpu_writeGs(kernelGS);
        }
    } else if (!next->kernelThread) {
        Cpu_writeFs(nr->fs);
        Cpu_writeGs(nr->gs);
    }
    // TODO: optionally save FPU/SSE context
    if (next->task != curr->task) {
        AddressSpace_activate(&next->task->addressSpace);
    }
    next->state = threadStateRunning;
    next->cpu = cpu;
    cpu->currentThread = next;
    cpu->nextThread = next;
    cpu->tss.esp0 = (uint32_t) nr + sizeof(ThreadRegisters); // unused for kernel-mode threads
}

bool Cpu_accountTimesliceAndCheckExpiration(Cpu *cpu) {
    uint64_t t = Tsc_read();
    uint64_t dt = t - cpu->lastScheduleTime; // may overflow if we run for 10s of years :)
    cpu->lastScheduleTime = t;
    if (dt > UINT32_MAX) dt = UINT32_MAX;
    uint64_t dtNanoseconds = Tsc_convertTicksToNanoseconds(&cpu->tsc, dt);
    if (cpu->currentThread->timesliceRemaining <= TIMESLICE_TOLERANCE + dtNanoseconds) {
        cpu->currentThread->timesliceRemaining = Cpu_timesliceLengths[cpu->currentThread->nice];
        return true;
    }
    cpu->currentThread->timesliceRemaining -= dtNanoseconds;
    return false; 
}

Thread *Cpu_findNextThreadAndUpdateReadyQueue(Cpu *cpu, bool timesliced) {
    Thread *curr = cpu->currentThread;
    PriorityQueue *readyQueue = &cpu->cpuNode->readyQueue;
    if (cpu->nextThread != curr) {
        if (curr->state != threadStateBlocked && curr != &cpu->idleThread)
            PriorityQueue_insertFront(readyQueue, &curr->queueNode);
        return cpu->nextThread;
    }
    if (curr->state == threadStateBlocked)
        return !PriorityQueue_isEmpty(readyQueue)
                ? Thread_fromQueueNode(PriorityQueue_poll(readyQueue))
                : &cpu->idleThread;
    assert(curr->state == threadStateRunning);
    if (!PriorityQueue_isEmpty(readyQueue)) {
        Thread *h = Thread_fromQueueNode(PriorityQueue_peek(readyQueue));
        if (Thread_isHigherPriority(h, curr) || (Thread_isSamePriority(h, curr) && timesliced)) {
            curr->state = threadStateReady;
            return curr != &cpu->idleThread
                    ? Thread_fromQueueNode(PriorityQueue_pollAndInsert(readyQueue, &curr->queueNode, !timesliced))
                    : Thread_fromQueueNode(PriorityQueue_poll(readyQueue));
        }
    }
    return curr;
}

void Cpu_setTimesliceTimer(Cpu *cpu) {
    cpu->timesliceTimerEnabled = cpu->currentThread != &cpu->idleThread
            && !PriorityQueue_isEmpty(&cpu->cpuNode->readyQueue)
            && PriorityQueue_peek(&cpu->cpuNode->readyQueue)->key == cpu->currentThread->queueNode.key;
    if (cpu->timesliceTimerEnabled) {
        assert(cpu->currentThread->timesliceRemaining > TIMESLICE_TOLERANCE);
        uint32_t ticks = LapicTimer_convertNanosecondsToTicks(&cpu->lapicTimer, cpu->currentThread->timesliceRemaining);
//        Log_printf("Cpu %d enabling timeslice in %d LAPIC timer ticks.\n", cpu->lapicId, ticks);
        Cpu_writeLocalApic(lapicTimerInitialCount, ticks);
    }
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
void Cpu_schedule(Cpu *currentCpu) {
    if (!currentCpu->rescheduleNeeded) return;
    Spinlock_lock(&currentCpu->cpuNode->lock);
    currentCpu->rescheduleNeeded = false;
    bool timesliced = Cpu_accountTimesliceAndCheckExpiration(currentCpu);
    Thread *next = Cpu_findNextThreadAndUpdateReadyQueue(currentCpu, timesliced); // ~35 TSC ticks
    if (next != currentCpu->currentThread) {
        Cpu_switchToThread(currentCpu, next); // ~200 TSC ticks
        Cpu_setTimesliceTimer(currentCpu); // ~15 TSC ticks
    } else if (timesliced) {
        Cpu_setTimesliceTimer(currentCpu);
    }
    Spinlock_unlock(&currentCpu->cpuNode->lock);
}

static void Cpu_handleSysenter(Cpu *currentCpu) {
    ThreadRegisters *regs = currentCpu->currentThread->regs;
    switch (regs->eax & 0xFF) {
#if 0
        case 1:
            regs->eax = Syscall_createChannel(currentCpu->currentThread->task);
            break;
        case 2:
            regs->eax = Syscall_deleteCapability(currentCpu->currentThread->task, regs->ebx);
            break;
        case 3:
            regs->eax = Syscall_sendMessage(currentCpu, regs->eax >> 8, regs->ebx);
            break;
        case 4:
            regs->eax = Syscall_receiveMessage(currentCpu->currentThread, regs->ebx, (uint8_t *) regs->esi, regs->edi);
            break;
        case 5:
            regs->eax = Syscall_readMessage(currentCpu->currentThread, regs->ebx, regs->ebp, (uint8_t *) regs->esi, regs->edi);
            break;
#endif
        case 127:
            Log_printf("System call on Cpu 0x%02X\n"
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
                    currentCpu->lapicId, regs, regs->vector, regs->eax, regs->ecx, regs->edx, regs->ebx,
                    regs->ebp, regs->esi, regs->edi, regs->eip, regs->cs, regs->ds, regs->es, regs->eflags);
            break;
        default:
            regs->eax = ENOSYS;
            break;
    }
    Cpu_schedule(currentCpu);
}

static void Cpu_handleInterrupt(Cpu *currentCpu) {
    const int logMaxInterruptCount = 12;
    const int maxInterruptCount = 1 << logMaxInterruptCount;
    uint64_t beginTsc = Tsc_read();
    currentCpu->interruptCount++;
    switch (currentCpu->currentThread->regs->vector & THREADREGISTERS_VECTOR_MASK) {
        case lapicTimerVector:
            currentCpu->rescheduleNeeded = true;
            Cpu_writeLocalApic(lapicEoi, 0);
            break;
        case rescheduleIpiVector:
            Log_printf("Reschedule IPI on CPU 0x%02X.\n", currentCpu->lapicId);
            currentCpu->rescheduleNeeded = true;
            Cpu_writeLocalApic(lapicEoi, 0);
            break;
        default:
            Cpu_unhandledException(currentCpu, NULL);
            break;
    }
    //IsrTableEntry *isrTableEntry = &Cpu_isrTable[cpu->currentThread->regs->vector];
    //isrTableEntry->isr(isrTableEntry->param, cpu->currentThread->regs);
    //uint32_t returnSp = (uint32_t) cpu->currentThread->regs + offsetof(ThreadRegisters, es);
    Cpu_schedule(currentCpu); // ~440 TSC ticks
    currentCpu->interruptTsc += Tsc_read() - beginTsc;
    if (currentCpu->interruptCount == maxInterruptCount) {
        Video_printf("Cpu %d interrupt TSC=0x%016llX (%d).\n", currentCpu->lapicId, currentCpu->interruptTsc >> logMaxInterruptCount, (int) (currentCpu->interruptTsc >> logMaxInterruptCount));
        currentCpu->interruptCount = 0;
        currentCpu->interruptTsc = 0;
    }
}

/**
 * Dispatches the appropriate interrupt handler or invokes the system call dispatcher.
 * This is the C entry point of the kernel, called in Cpu_asm.S.
 * @param currentCpu The current CPU.
 * @return Pointer to the ThreadRegister structure of the thread to resume.
 */
__attribute__((fastcall)) ThreadRegisters *Cpu_handleSyscallOrInterrupt(Cpu *currentCpu) {
    while (true) {
        currentCpu->currentThread->kernelRestartNeeded = false;
        if (currentCpu->currentThread->regs->vector & THREADREGISTERS_VECTOR_SYSENTER) {
            Cpu_handleSysenter(currentCpu);
        } else {
            Cpu_handleInterrupt(currentCpu);
        }
        if (!currentCpu->currentThread->kernelRestartNeeded) break;
        Cpu_enableInterrupts();
        Cpu_disableInterrupts();
    }
    return currentCpu->currentThread->regs;
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

void Cpu_requestReschedule(Cpu *cpu) {
    if (cpu != Cpu_getCurrent())
        Cpu_sendRescheduleInterrupt(cpu);
    else
        cpu->rescheduleNeeded = true;
}
