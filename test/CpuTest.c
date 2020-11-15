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
    AddressSpace_activate(&dummyTaskToCheckAddressSpaceDidNotChange.addressSpace);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(FakeHardware_currentAddressSpace == &dummyTaskToCheckAddressSpaceDidNotChange.addressSpace);
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
    AddressSpace_activate(&currentTask.addressSpace);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(FakeHardware_currentAddressSpace == &newTask.addressSpace);
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
    Cpu_writeFs(1001);
    Cpu_writeGs(1002);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(currentThread.regs->fs == 1001);
    ASSERT(currentThread.regs->gs == 1002);
    ASSERT(Cpu_readFs() == 421);
    ASSERT(Cpu_readGs() == 422);
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
    Cpu_writeFs(1001);
    Cpu_writeGs(1002);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(currentThread.regs->fs == 1001);
    ASSERT(currentThread.regs->gs == 1002);
    ASSERT(Cpu_readFs() == 1001);
    ASSERT(Cpu_readGs() == kernelGS);
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
    Cpu_writeFs(0);
    Cpu_writeGs(kernelGS);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(currentThread.regs->fs == 0);
    ASSERT(currentThread.regs->gs == 0);
    ASSERT(Cpu_readFs() == 421);
    ASSERT(Cpu_readGs() == 422);
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
    Cpu_writeFs(0);
    Cpu_writeGs(kernelGS);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(currentThread.regs->fs == 0);
    ASSERT(currentThread.regs->gs == 0);
    ASSERT(Cpu_readFs() == 0);
    ASSERT(Cpu_readGs() == kernelGS);
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
    Cpu_loadLdt(1001);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(FakeHardware_ldtRegister == 421);
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
    Cpu_loadLdt(currentThread.regs->ldt);
    
    Cpu_switchToThread(&cpu, &newThread);
    
    ASSERT(FakeHardware_ldtRegister == 0);
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
}
