#include "test.h"
#include "kernel.h"

static void initThread(Thread *thread, ThreadState state, unsigned effectivePriority, Task *task) {
    memzero(thread, sizeof(Thread));
    thread->state = state;
    thread->queueNode.key = effectivePriority;
    thread->regs = &thread->regsBuf;
    thread->task = task;
}

static void initCpu(Cpu *cpu, bool active, uint64_t scheduleArrival, Thread *currentThread) {
    memzero(cpu, sizeof(Cpu));
    cpu->active = active;
    cpu->scheduleArrival = scheduleArrival;
    cpu->currentThread = currentThread;
    cpu->nextThread = currentThread;
    cpu->idleThread.queueNode.key = THREAD_IDLE_PRIORITY;
}

static void initCpuNode(CpuNode *node, Cpu *cpus, size_t cpuCount, uint64_t scheduleArrival) {
    memzero(node, sizeof(CpuNode));
    node->cpus = cpus;
    node->cpuCount = cpuCount;
    node->scheduleArrival = scheduleArrival;
    PriorityQueue_init(&node->readyQueue);
    for (size_t i = 0; i < cpuCount; i++)
        cpus->cpuNode = node;
}

static void CpuTest_switchToThread_invariants() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &unimportantTask);
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(newThread.state == threadStateRunning);
    ASSERT(newThread.cpu == &cpu);
    ASSERT(cpu.currentThread == &newThread);
    ASSERT(cpu.nextThread == &newThread);
    ASSERT(cpu.tss.esp0 == (uint32_t) newThread.regs + offsetof(ThreadRegisters, ss) + sizeof(uint32_t));
}

static void CpuTest_switchToThread_sameAddressSpace() {
    Task dummyTaskToCheckAddressSpaceDidNotChange;
    Task currentTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &currentTask);
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &currentTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    theFakeHardware = (FakeHardware) { .currentAddressSpace = &dummyTaskToCheckAddressSpaceDidNotChange.addressSpace };
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(theFakeHardware.currentAddressSpace == &dummyTaskToCheckAddressSpaceDidNotChange.addressSpace);
}

static void CpuTest_switchToThread_differentAddressSpace() {
    Task currentTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &currentTask);
    Task newTask;
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &newTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    theFakeHardware = (FakeHardware) { .currentAddressSpace = &currentTask.addressSpace };
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(theFakeHardware.currentAddressSpace == &newTask.addressSpace);
}

static void CpuTest_switchToThread_userToUser() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &unimportantTask);
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &unimportantTask);
    newThread.regs->fs = 421;
    newThread.regs->gs = 422;
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    theFakeHardware = (FakeHardware) { .fsRegister = 1001, .gsRegister = 1002 };
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(currentThread.regs->fs == 1001);
    ASSERT(currentThread.regs->gs == 1002);
    ASSERT(theFakeHardware.fsRegister == 421);
    ASSERT(theFakeHardware.gsRegister == 422);
}

static void CpuTest_switchToThread_userToKernel() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &unimportantTask);
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &unimportantTask);
    newThread.kernelThread = true;
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    theFakeHardware = (FakeHardware) { .fsRegister = 1001, .gsRegister = 1002 };
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(currentThread.regs->fs == 1001);
    ASSERT(currentThread.regs->gs == 1002);
    ASSERT(theFakeHardware.fsRegister == 1001);
    ASSERT(theFakeHardware.gsRegister == kernelGS);
}

static void CpuTest_switchToThread_kernelToUser() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &unimportantTask);
    currentThread.kernelThread = true;
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &unimportantTask);
    newThread.regs->fs = 421;
    newThread.regs->gs = 422;
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    theFakeHardware = (FakeHardware) { .fsRegister = 0, .gsRegister = kernelGS };
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(currentThread.regs->fs == 0);
    ASSERT(currentThread.regs->gs == 0);
    ASSERT(theFakeHardware.fsRegister == 421);
    ASSERT(theFakeHardware.gsRegister == 422);
}

static void CpuTest_switchToThread_kernelToKernel() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &unimportantTask);
    currentThread.kernelThread = true;
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &unimportantTask);
    newThread.kernelThread = true;
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    const uint32_t unchangingValue = 1234;
    theFakeHardware = (FakeHardware) { .fsRegister = unchangingValue, .gsRegister = unchangingValue };
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(currentThread.regs->fs == 0);
    ASSERT(currentThread.regs->gs == 0);
    ASSERT(theFakeHardware.fsRegister == unchangingValue);
    ASSERT(theFakeHardware.gsRegister == unchangingValue);
}

static void CpuTest_switchToThread_withLocalDescriptorTable() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &unimportantTask);
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &unimportantTask);
    newThread.regs->ldt = 421;
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    theFakeHardware = (FakeHardware) { .ldtRegister = 1001 };
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(theFakeHardware.ldtRegister == 421);
}

static void CpuTest_switchToThread_withoutLocalDescriptorTable() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateReady, 100, &unimportantTask);
    currentThread.regs->ldt = 1001;
    Thread newThread;
    initThread(&newThread, threadStateReady, 42, &unimportantTask);
    newThread.regs->ldt = 0;
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    theFakeHardware = (FakeHardware) { .ldtRegister = currentThread.regs->ldt };
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(theFakeHardware.ldtRegister == 0);
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_runningWithNextThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100, &unimportantTask);
    Thread nextThread;
    initThread(&nextThread, threadStateReady, 150, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    cpu.nextThread = &nextThread;
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &nextThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_peek(&node.readyQueue)) == &currentThread);
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_idleWithNextThread() {
    Task unimportantTask;
    Thread nextThread;
    initThread(&nextThread, threadStateReady, 150, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &cpu.idleThread);
    cpu.nextThread = &nextThread;
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &nextThread);
    ASSERT(PriorityQueue_isEmpty(&node.readyQueue));
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_blockedWithNextThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateBlocked, 100, &unimportantTask);
    Thread nextThread;
    initThread(&nextThread, threadStateReady, 150, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    cpu.nextThread = &nextThread;
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &nextThread);
    ASSERT(PriorityQueue_isEmpty(&node.readyQueue));
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_blockedWithNoReadyThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateBlocked, 100, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &cpu.idleThread);
    ASSERT(PriorityQueue_isEmpty(&node.readyQueue));
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_blockedWithReadyThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateBlocked, 100, &unimportantTask);
    Thread readyThread;
    initThread(&readyThread, threadStateReady, 150, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    PriorityQueue_insert(&node.readyQueue, &readyThread.queueNode);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &readyThread);
    ASSERT(PriorityQueue_isEmpty(&node.readyQueue));
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_runningWithNoReadyThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &currentThread);
    ASSERT(PriorityQueue_isEmpty(&node.readyQueue));
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_runningWithLowerPriorityReadyThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100, &unimportantTask);
    Thread readyThread;
    initThread(&readyThread, threadStateReady, 150, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    PriorityQueue_insert(&node.readyQueue, &readyThread.queueNode);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &currentThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_peek(&node.readyQueue)) == &readyThread);
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_runningWithHigherPriorityReadyThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100, &unimportantTask);
    Thread readyThread;
    initThread(&readyThread, threadStateReady, 42, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    PriorityQueue_insert(&node.readyQueue, &readyThread.queueNode);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &readyThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_peek(&node.readyQueue)) == &currentThread);
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_runningWithSamePriorityReadyThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100, &unimportantTask);
    Thread readyThread;
    initThread(&readyThread, threadStateReady, 100, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    PriorityQueue_insert(&node.readyQueue, &readyThread.queueNode);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &currentThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_peek(&node.readyQueue)) == &readyThread);
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_timeslicedWithSamePriorityReadyThread() {
    Task unimportantTask;
    Thread currentThread;
    initThread(&currentThread, threadStateRunning, 100, &unimportantTask);
    Thread readyThread;
    initThread(&readyThread, threadStateReady, 100, &unimportantTask);
    Thread anotherReadyThread;
    initThread(&anotherReadyThread, threadStateReady, 100, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &currentThread);
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    PriorityQueue_insert(&node.readyQueue, &readyThread.queueNode);
    PriorityQueue_insert(&node.readyQueue, &anotherReadyThread.queueNode);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, true);
    
    ASSERT(thread == &readyThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_poll(&node.readyQueue)) == &anotherReadyThread);
    ASSERT(Thread_fromQueueNode(PriorityQueue_poll(&node.readyQueue)) == &currentThread);
}

static void CpuTest_findNextThreadAndUpdateReadyQueue_idleWithReadyThread() {
    Task unimportantTask;
    Thread readyThread;
    initThread(&readyThread, threadStateReady, 42, &unimportantTask);
    Cpu cpu;
    initCpu(&cpu, true, 3, &cpu.idleThread);
    CpuNode node;
    initCpuNode(&node, &cpu, 1, 3);
    PriorityQueue_insert(&node.readyQueue, &readyThread.queueNode);
    
    Thread *thread = Cpu_findNextThreadAndUpdateReadyQueue(&cpu, false);
    
    ASSERT(thread == &readyThread);
    ASSERT(PriorityQueue_isEmpty(&node.readyQueue));
}

void CpuTest_run() {
    RUN_TEST(CpuTest_switchToThread_invariants);
    RUN_TEST(CpuTest_switchToThread_sameAddressSpace);
    RUN_TEST(CpuTest_switchToThread_differentAddressSpace);
    RUN_TEST(CpuTest_switchToThread_userToUser);
    RUN_TEST(CpuTest_switchToThread_userToKernel);
    RUN_TEST(CpuTest_switchToThread_kernelToUser);
    RUN_TEST(CpuTest_switchToThread_kernelToKernel);
    RUN_TEST(CpuTest_switchToThread_withLocalDescriptorTable);
    RUN_TEST(CpuTest_switchToThread_withoutLocalDescriptorTable);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_runningWithNextThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_idleWithNextThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_blockedWithNextThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_blockedWithNoReadyThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_blockedWithReadyThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_runningWithNoReadyThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_runningWithLowerPriorityReadyThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_runningWithHigherPriorityReadyThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_runningWithSamePriorityReadyThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_timeslicedWithSamePriorityReadyThread);
    RUN_TEST(CpuTest_findNextThreadAndUpdateReadyQueue_idleWithReadyThread);
}
