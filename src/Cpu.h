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
#ifndef CPU_H_INCLUDED
#define CPU_H_INCLUDED

#include "Types.h"

/** The absolute maximum number of CPUs supported. */
#define MAX_CPU_COUNT 1024
/** Number of nice levels (to weight threads with same priority). */
#define NICE_LEVELS 40

#define CPU_LAPIC_DEFAULT_PHYSICAL_ADDRESS physicalAddress(0xFEE00000)


/** CPU Model Specific Registers. */
enum Msr {
    msrSysenterCs = 0x174,
    msrSysenterEsp = 0x175,
    msrSysenterEip = 0x176
};

/** Hardwired interrupt vectors. */
enum IrqVector {
    lapicTimerVector = 0xFC,
    tlbShootdownIpiVector = 0xFD,
    rescheduleIpiVector = 0xFE,
    spuriousInterruptVector = 0xFF
};

/** Offsets for Local APIC registers. */
enum Lapic {
    lapicIdRegister = 0x20,
    lapicEoi = 0xB0,
    lapicSpuriousInterrupt = 0xF0,
    lapicInterruptCommandLow = 0x300, // Interrupt Command Register, low word
    lapicInterruptCommandHigh = 0x310, // Interrupt Command Register, high word (IPI destination)
    lapicTimerLvt = 0x320,
    lapicPerformanceCounterLvt = 0x340,
    lapicLint0Lvt = 0x350,
    lapicLint1Lvt = 0x360,
    lapicTimerInitialCount = 0x380,
    lapicTimerCurrentCount = 0x390,
    lapicTimerDivider = 0x3E0
};

enum CpuFlag {
    CpuFlag_interruptEnable = 1 << 9
};

/**
 * Dummy union to check that offsets and size of the Cpu struct are consistent with constants used in assembly.
 * Courtesy of http://www.embedded.com/design/prototyping-and-development/4024941/Learn-a-new-trick-with-the-offsetof--macro
 */
union CpuChecker {
    char wrongLapicIdOffset[offsetof(Cpu, lapicId) == CPU_LAPICID_OFFSET];
    char wrongInKernelOffset[offsetof(Cpu, kernelEntryCount) == CPU_KERNELENTRYCOUNT_OFFSET];
    char wrongThisCpuOffset[offsetof(Cpu, thisCpu) == CPU_THISCPU_OFFSET];
    char wrongCurrentThreadOffset[offsetof(Cpu, currentThread) == CPU_CURRENTTHREAD_OFFSET];
    char wrongStackOffset[offsetof(Cpu, stack) == CPU_STACK_OFFSET];
    char wrongTopOfStack[offsetof(Cpu, padding1) == CPU_TOP_OF_STACK];
    char wrongCpuSize[sizeof(Cpu) == CPU_STRUCT_SIZE];
    char cpuNotFittingInPage[sizeof(Cpu) <= PAGE_SIZE];
}; 

extern uint32_t Cpu_cpuCount;
extern Cpu *Cpu_cpus[MAX_CPU_COUNT];
extern CpuDescriptor Cpu_idt[256];
extern unsigned Cpu_timesliceLengths[NICE_LEVELS];

void Cpu_logRegisters(Cpu *cpu);
__attribute__((fastcall, noreturn)) int Cpu_idleThreadFunction(void *); // from Cpu_asm.S
__attribute__((fastcall)) void Cpu_unhandledException(Cpu *cpu, void *param);
void Cpu_returnToUserMode(uintptr_t stackPointer); // from Cpu_asm.S
void Cpu_setIsr(size_t vector, Isr isr, void *param);
void Cpu_sendRescheduleInterrupt(Cpu *cpu);
void Cpu_sendTlbShootdownIpi(Cpu *cpu);
void Cpu_switchToThread(Cpu *cpu, Thread *next);
void Cpu_requestReschedule(Cpu *cpu);
bool Cpu_accountTimesliceAndCheckExpiration(Cpu *cpu);
Thread *Cpu_findNextThreadAndUpdateReadyQueue(Cpu *cpu, bool timesliced);
void Cpu_setTimesliceTimer(Cpu *cpu);
void Cpu_schedule(Cpu *currentCpu);

#endif
