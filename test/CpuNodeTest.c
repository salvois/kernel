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
#include "test.h"
#include "kernel.h"

static void initThread(Thread *thread, ThreadState state, unsigned effectivePriority) {
    memzero(thread, sizeof(Thread));
    thread->state = state;
    thread->queueNode.key = effectivePriority;
}

static void initCpu(Cpu *cpu, bool active, uint64_t scheduleArrival, Thread *currentThread) {
    memzero(cpu, sizeof(Cpu));
    cpu->active = active;
    cpu->scheduleArrival = scheduleArrival;
    cpu->currentThread = currentThread;
    cpu->nextThread = currentThread;
}

static void initCpuNode(CpuNode *node, Cpu **cpus, size_t cpuCount, uint64_t scheduleArrival) {
    memzero(node, sizeof(CpuNode));
    node->cpus = cpus;
    node->cpuCount = cpuCount;
    node->scheduleArrival = scheduleArrival;
    PriorityQueue_init(&node->readyQueue);
}

static void CpuNodeTest_findTargetCpu() {
    Thread threads[4];
    initThread(&threads[0], threadStateRunning, 42);
    initThread(&threads[1], threadStateRunning, 100);
    initThread(&threads[2], threadStateRunning, 100);
    initThread(&threads[3], threadStateRunning, 50);        
    Cpu cpu0;
    Cpu cpu1;
    Cpu cpu2;
    Cpu cpu3;
    Cpu *cpus[] = { &cpu0, &cpu1, &cpu2, &cpu3 };
    initCpu(&cpu0, true, 1, &threads[0]);
    initCpu(&cpu1, true, 2, &threads[1]);
    initCpu(&cpu2, true, 3, &threads[2]);
    initCpu(&cpu3, true, 4, &threads[3]);
    CpuNode node;
    initCpuNode(&node, cpus, 4, 4);
    
    Cpu *targetCpu = CpuNode_findTargetCpu(&node, NULL);
    
    ASSERT(targetCpu == &cpu1);
}

static void CpuNodeTest_addRunnableThread_alreadyRunning() {
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100);
    Cpu cpu;
    initCpu(&cpu, true, 2, &currentThread);
    Cpu *cpus[] = { &cpu };
    CpuNode node;
    initCpuNode(&node, cpus, 1, 2);
    
    CpuNode_addRunnableThread(&node, &currentThread);
    
    ASSERT(node.scheduleArrival == 2);
    ASSERT(cpu.scheduleArrival == 2);
    ASSERT(currentThread.state == threadStateRunning);
    ASSERT(cpu.nextThread == &currentThread);
    ASSERT(PriorityQueue_isEmpty(&node.readyQueue));
    ASSERT(cpu.rescheduleNeeded == false);
}

static void CpuNodeTest_addRunnableThread_higherPriority() {
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100);
    Thread newThread;
    initThread(&newThread, threadStateReady, 42);
    Cpu cpu;
    initCpu(&cpu, true, 2, &currentThread);
    CpuNode node;
    Cpu *cpus[] = { &cpu };
    initCpuNode(&node, cpus, 1, 2);
    theFakeHardware = (FakeHardware) { .currentCpu = &cpu };
    
    CpuNode_addRunnableThread(&node, &newThread);
    
    ASSERT(node.scheduleArrival == 3);
    ASSERT(cpu.scheduleArrival == 3);
    ASSERT(newThread.state == threadStateNext);
    ASSERT(cpu.nextThread == &newThread);
    ASSERT(PriorityQueue_isEmpty(&node.readyQueue));
    ASSERT(cpu.rescheduleNeeded == true);
}

static void CpuNodeTest_addRunnableThread_higherPriorityWithNextThread() {
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100);
    Thread nextThread;
    initThread(&nextThread, threadStateNext, 99);
    Thread newThread;
    initThread(&newThread, threadStateReady, 42);
    Cpu cpu;
    initCpu(&cpu, true, 2, &currentThread);
    cpu.nextThread = &nextThread;
    CpuNode node;
    Cpu *cpus[] = { &cpu };
    initCpuNode(&node, cpus, 1, 2);
    theFakeHardware = (FakeHardware) { .currentCpu = &cpu };
    
    CpuNode_addRunnableThread(&node, &newThread);
    
    ASSERT(node.scheduleArrival == 3);
    ASSERT(cpu.scheduleArrival == 3);
    ASSERT(newThread.state == threadStateNext);
    ASSERT(cpu.nextThread == &newThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_peek(&node.readyQueue)) == &nextThread);
    ASSERT(cpu.rescheduleNeeded == true);
}

static void CpuNodeTest_addRunnableThread_lowerPriority() {
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100);
    Thread newThread;
    initThread(&newThread, threadStateReady, 120);
    Cpu cpu;
    initCpu(&cpu, true, 2, &currentThread);
    CpuNode node;
    Cpu *cpus[] = { &cpu };
    initCpuNode(&node, cpus, 1, 2);
    theFakeHardware = (FakeHardware) { .currentCpu = &cpu };
    
    CpuNode_addRunnableThread(&node, &newThread);
    
    ASSERT(node.scheduleArrival == 3);
    ASSERT(cpu.scheduleArrival == 3);
    ASSERT(newThread.state == threadStateReady);
    ASSERT(cpu.nextThread == &currentThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_peek(&node.readyQueue)) == &newThread);
    ASSERT(cpu.rescheduleNeeded == false);
}

static void CpuNodeTest_addRunnableThread_samePriorityTimeSlicingDisabled() {
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100);
    Thread newThread;
    initThread(&newThread, threadStateReady, 100);
    Cpu cpu;
    initCpu(&cpu, true, 2, &currentThread);
    CpuNode node;
    Cpu *cpus[] = { &cpu };
    initCpuNode(&node, cpus, 1, 2);
    theFakeHardware = (FakeHardware) { .currentCpu = &cpu };
    
    CpuNode_addRunnableThread(&node, &newThread);
    
    ASSERT(node.scheduleArrival == 3);
    ASSERT(cpu.scheduleArrival == 3);
    ASSERT(newThread.state == threadStateReady);
    ASSERT(cpu.nextThread == &currentThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_peek(&node.readyQueue)) == &newThread);
    ASSERT(cpu.rescheduleNeeded == true);
}

void CpuNodeTest_run() {
    RUN_TEST(CpuNodeTest_findTargetCpu);
    RUN_TEST(CpuNodeTest_addRunnableThread_alreadyRunning);
    RUN_TEST(CpuNodeTest_addRunnableThread_higherPriority);
    RUN_TEST(CpuNodeTest_addRunnableThread_higherPriorityWithNextThread);
    RUN_TEST(CpuNodeTest_addRunnableThread_lowerPriority);
    RUN_TEST(CpuNodeTest_addRunnableThread_samePriorityTimeSlicingDisabled);
}
