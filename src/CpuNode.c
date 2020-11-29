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

/** Global instance of the unique CPU node we currently support. */
CpuNode CpuNode_theInstance;

static inline bool isRunningLowerPriorityThread(const Cpu *cpu, const Cpu *other) {
    return Thread_isHigherPriority(other->nextThread, cpu->nextThread)
            || (Thread_isSamePriority(cpu->nextThread, other->nextThread) && cpu->scheduleArrival < other->scheduleArrival);
}

Cpu *CpuNode_findTargetCpu(const CpuNode *node, Cpu *preferredCpu) {
    Cpu *targetCpu = preferredCpu != NULL ? preferredCpu : node->cpus[0];
    // TODO: consider HyperThreading, TurboBoost and NUMA
    for (size_t i = 0; i < node->cpuCount; i++) {
        Cpu *c = node->cpus[i];
        if (c->active && c != targetCpu && isRunningLowerPriorityThread(c, targetCpu))
            targetCpu = c;
    }
    return targetCpu;
}

void CpuNode_preempt(CpuNode *node, Cpu *cpu, Thread *thread) {
    if (cpu->nextThread->state == threadStateNext) {
        cpu->nextThread->state = threadStateReady;
        PriorityQueue_insertFront(&node->readyQueue, &cpu->nextThread->queueNode);
    }
    thread->state = threadStateNext;
    cpu->nextThread = thread;
    Cpu_requestReschedule(cpu);
}

void CpuNode_addReadyThread(CpuNode *node, Cpu *cpu, Thread *thread) {
    thread->state = threadStateReady;
    PriorityQueue_insertFront(&node->readyQueue, &thread->queueNode);
    if (Thread_isSamePriority(thread, cpu->nextThread) && !cpu->timesliceTimerEnabled)
        Cpu_requestReschedule(cpu);
}

/**
 * Adds a runnable thread to the specified CPU node.
 * If the thread has higher priority than one running on a CPU, that CPU
 * is preempted. Otherwise it is added to the ready queue, in front of
 * other threads with the same priority, assuming it slept before consuming
 * it time slice.
 * This function must be called while holding the spinlock of the CPU node.
 */
void CpuNode_addRunnableThread(CpuNode *node, Thread *thread) {
    if (thread->state == threadStateRunning)
        return;
    if (thread->state == threadStateBlocked)
        PriorityQueue_remove(thread->threadQueue, &thread->queueNode);
    Cpu *cpu = CpuNode_findTargetCpu(node, thread->cpu);
    if (Thread_isHigherPriority(thread, cpu->nextThread))
        CpuNode_preempt(node, cpu, thread);
    else
        CpuNode_addReadyThread(node, cpu, thread);
    node->scheduleArrival++;
    cpu->scheduleArrival = node->scheduleArrival;
}
