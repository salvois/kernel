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

void Thread_setPriority(Thread *thread, unsigned priority) {
    Log_printf("Setting priority for thread=%p to %X\n", thread, priority);
    thread->priority = priority; // TODO: replace with atomic?
}

int Thread_initialize(Task *task, Thread *thread, unsigned priority, unsigned nice, uintptr_t entry, uintptr_t stackPointer) {
    memzero(thread, sizeof(Thread));
    thread->task = task;
    thread->threadFunction = NULL;
    thread->threadFunctionParam = NULL;
    thread->state = threadStateReady;
    assert(priority < 256);
    thread->queueNode.key = priority;
    thread->priority = priority;
    thread->nice = nice;
    thread->timesliceRemaining = Cpu_timesliceLengths[nice];
    thread->threadQueue = NULL;
    thread->cpu = NULL;
    thread->runningTime = 0;
    thread->cpuAffinity = false;
    thread->kernelThread = false;
    thread->stack = NULL;
    thread->regs = &thread->regsBuf;
    thread->regs->vector = THREADREGISTERS_VECTOR_CUSTOMDSES;
    thread->regs->cs = flatUserCS;
    thread->regs->eip = entry;
    thread->regs->eflags = 1 << 9; // interrupts enabled
    thread->regs->ds = flatUserDS;
    thread->regs->es = flatUserDS;
    thread->regs->ss = flatUserDS;
    thread->regs->esp = stackPointer;
    Log_printf("Thread initialized: %p (esp=%p, eip=%p), priority=%d, nice=%d.\n", thread, thread->regs->esp, thread->regs->eip, priority, nice);
    return 0;
}

void Thread_destroy(Thread *thread) {
}

int Thread_block(Thread *thread, PriorityQueue *queue) {
    Cpu *currentCpu = Cpu_getCurrent();
    assert(currentCpu->currentThread == thread);
    assert(thread->state == threadStateRunning);
    assert(thread->threadQueue == NULL);
    thread->threadQueue = queue;
    PriorityQueue_insert(queue, &thread->queueNode);
    thread->state = threadStateBlocked;
    Cpu_exitKernel(currentCpu);
    return 0;
}
