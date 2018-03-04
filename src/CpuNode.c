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

/** Global instance of the unique CPU node we currently support. */
CpuNode CpuNode_theInstance;

/**
 * Adds a runnable thread to the specified CPU node.
 * If the thread has higher priority than one running on a CPU, that CPU
 * is preempted. Otherwise it is added to the ready queue, in front of
 * other threads with the same priority, assuming it slept before consuming
 * it time slice.
 * This function must be called while holding the spinlock of the CPU node.
 */
void CpuNode_addRunnableThread(CpuNode *node, Thread *thread) {
    if (thread->state == threadStateRunning) return;
    if (thread->state == threadStateBlocked) {
        PriorityQueue_remove(thread->threadQueue, &thread->queueNode);
    }
    // Search for the CPU candidate to run the new runnable thread,
    // looking at the lowest priority one, preferring the CPU it ran last time.
    // TODO: consider HyperThreading, TurboBoost and NUMA
    Cpu *targetCpu = thread->cpu;
    if (targetCpu == NULL) targetCpu = &node->cpus[0];
    for (size_t i = 0; i < node->cpuCount; i++) {
        Cpu *c = &node->cpus[i];
        Log_printf("CPU %d active=%d, current thread %p (prio=%d), next thread %p (prio=%d).\n",
                c->lapicId, c->active, c->currentThread, c->currentThread->queueNode.key, c->nextThread, c->nextThread->queueNode.key);
        if (c != targetCpu && c->active && (Thread_isHigherPriority(targetCpu->nextThread, c->nextThread)
                || (!Thread_isHigherPriority(c->nextThread, targetCpu->nextThread) && c->scheduleArrival < targetCpu->scheduleArrival))) {
            targetCpu = c;
        }
    }
    Log_printf("Minimum priority CPU is %d.\n", targetCpu->lapicId);
    Cpu *currentCpu = Cpu_getCurrent();
    // Check if "thread" shall preempt "targetCpu"
    if (Thread_isHigherPriority(thread, targetCpu->nextThread)) {
        Log_printf("Thread %p (prio=%d) is next thread for CPU %d.\n", thread, thread->queueNode.key, targetCpu->lapicId);
        if (targetCpu->nextThread->state == threadStateNext) {
            targetCpu->nextThread->state = threadStateReady;
            PriorityQueue_insertFront(&node->readyQueue, &targetCpu->nextThread->queueNode);
        }
        thread->state = threadStateNext;
        targetCpu->nextThread = thread;
        if (targetCpu != currentCpu) {
            Cpu_sendRescheduleInterrupt(targetCpu);
        } else {
            currentCpu->rescheduleNeeded = true;
        }
    } else {
        Log_printf("Thread %p (prio=%d) added to the ready queue.\n", thread, thread->queueNode.key);
        thread->state = threadStateReady;
        PriorityQueue_insertFront(&node->readyQueue, &thread->queueNode);
        if (!Thread_isHigherPriority(targetCpu->nextThread, thread) && !targetCpu->timesliceTimerEnabled) {
            if (targetCpu != currentCpu) {
                Cpu_sendRescheduleInterrupt(targetCpu);
            } else {
                currentCpu->rescheduleNeeded = true;
            }
        }
    }
    targetCpu->scheduleArrival = ++targetCpu->cpuNode->scheduleArrival;
}
