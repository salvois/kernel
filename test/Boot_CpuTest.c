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
#include "hardware/hardware.h"

static void initializePhysicalMemory(uint8_t *fakePhysicalMemory, Frame *frames, size_t frameCount) {
    memzero(frames, frameCount * sizeof(Frame));
    PhysicalAddress base = virt2phys(fakePhysicalMemory);
    FrameNumber baseFrame = floorToFrame(base);
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[0], baseFrame, addToFrameNumber(baseFrame, frameCount));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[1], addToFrameNumber(baseFrame, frameCount), addToFrameNumber(baseFrame, frameCount));
    PhysicalMemoryRegion_initialize(&PhysicalMemory_regions[2], addToFrameNumber(baseFrame, frameCount), addToFrameNumber(baseFrame, frameCount));
    PhysicalMemory_frameDescriptors = frames;
    PhysicalMemory_firstFrame = baseFrame;
    PhysicalMemory_add(base, addToPhysicalAddress(base, frameCount * PAGE_SIZE));
}

static void assertCpuProperlyInitialized(Cpu *cpu, int index, int lapicId) {
    ThreadRegisters expectedIdleThreadRegisters = {
        .gs = kernelGS,
        .eip = (uint32_t) Cpu_idleThreadFunction,
        .cs = flatKernelCS,
        .eflags = CpuFlag_interruptEnable
    };
    ASSERT(cpu->index == index);
    ASSERT(cpu->lapicId == lapicId);
    ASSERT(cpu->active == true);
    ASSERT(cpu->rescheduleNeeded == true);
    ASSERT(cpu->thisCpu == cpu);
    ASSERT(cpu->currentThread == &cpu->idleThread);
    ASSERT(cpu->nextThread == &cpu->idleThread);
    ASSERT(cpu->tss.ss0 == flatKernelDS);
    ASSERT(cpu->tss.esp0 == (uint32_t) &cpu->idleThread.regsBuf + offsetof(ThreadRegisters, edi));
    ASSERT(cpu->idleThread.cpu == cpu);
    ASSERT(memcmp(cpu->idleThread.regs, &expectedIdleThreadRegisters, sizeof(ThreadRegisters)) == 0);
}

static void Boot_CpuTest_initializeCpuStructs_multiProcessor() {
    const size_t totalMemoryFrames = 3;
    struct {
        Cpu cpu1;
        Cpu cpu0;
        PageTable lapicPageTable;
    } __attribute__ ((aligned(PAGE_SIZE))) fakePhysicalMemory;
    Frame frames[totalMemoryFrames];
    initializePhysicalMemory((uint8_t *) &fakePhysicalMemory, frames, totalMemoryFrames);
    theFakeHardware = (FakeHardware) { .lapicIdRegister = 0x02000000 };
    struct {
        MpConfigHeader header;
        MpConfigProcessor processors[2];
    } fakeMultiProcessorSpecification = {
        .header = { .entryCount = 2, .lapicPhysicalAddress = 0x7AF1C000 },
        .processors = {
            { .entryType = mpConfigProcessor, .lapicId = 0x00, .flags = 1 },
            { .entryType = mpConfigProcessor, .lapicId = 0x02, .flags = 1 },
        }
    };

    Cpu *bootCpu = Cpu_initializeCpuStructs(&fakeMultiProcessorSpecification.header);

    ASSERT(Boot_kernelPageDirectory.entries[CPU_LAPIC_VIRTUAL_ADDRESS >> 22] == (virt2phys(&fakePhysicalMemory.lapicPageTable).v | ptPresent | ptWriteable));
    ASSERT(fakePhysicalMemory.lapicPageTable.entries[(CPU_LAPIC_VIRTUAL_ADDRESS >> 12) & 0x3FF] == (0x7AF1C000 | ptPresent | ptWriteable | ptGlobal | ptCacheDisable));
    ASSERT(bootCpu == &fakePhysicalMemory.cpu1);
    assertCpuProperlyInitialized(&fakePhysicalMemory.cpu0, 0, 0x00);
    assertCpuProperlyInitialized(&fakePhysicalMemory.cpu1, 1, 0x02);
}

static void Boot_CpuTest_initializeCpuStructs_noMultiProcessorSpecification() {
    const size_t totalMemoryFrames = 2;
    struct {
        Cpu cpu0;
        PageTable lapicPageTable;
    } __attribute__ ((aligned(PAGE_SIZE))) fakePhysicalMemory;
    Frame frames[totalMemoryFrames];
    initializePhysicalMemory((uint8_t *) &fakePhysicalMemory, frames, totalMemoryFrames);
    theFakeHardware = (FakeHardware) { .lapicIdRegister = 0x00 };

    Cpu *bootCpu = Cpu_initializeCpuStructs(NULL);

    ASSERT(Boot_kernelPageDirectory.entries[CPU_LAPIC_VIRTUAL_ADDRESS >> 22] == (virt2phys(&fakePhysicalMemory.lapicPageTable).v | ptPresent | ptWriteable));
    ASSERT(fakePhysicalMemory.lapicPageTable.entries[(CPU_LAPIC_VIRTUAL_ADDRESS >> 12) & 0x3FF] == (CPU_LAPIC_DEFAULT_PHYSICAL_ADDRESS.v  | ptPresent | ptWriteable | ptGlobal | ptCacheDisable));
    ASSERT(bootCpu == &fakePhysicalMemory.cpu0);
    assertCpuProperlyInitialized(&fakePhysicalMemory.cpu0, 0, 0x00);
}

void Boot_CpuTest_run() {
    RUN_TEST(Boot_CpuTest_initializeCpuStructs_multiProcessor);
    RUN_TEST(Boot_CpuTest_initializeCpuStructs_noMultiProcessorSpecification);
}
