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
#ifndef TEST_HARDWARE_H_INCLUDED
#define TEST_HARDWARE_H_INCLUDED

typedef struct FakeHardware {
    Cpu *currentCpu;
    uint32_t lapicIdRegister;
    uint32_t lapicEoi;
    uint32_t lapicSpuriousInterrupt;
    uint32_t lapicInterruptCommandLow;
    uint32_t lapicInterruptCommandHigh;
    uint32_t lapicTimerLvt;
    uint32_t lapicPerformanceCounterLvt;
    uint32_t lapicLint0Lvt;
    uint32_t lapicLint1Lvt;
    uint32_t lapicTimerInitialCount;
    uint32_t lapicTimerCurrentCount;
    uint32_t lapicTimerDivider;
    AddressSpace *currentAddressSpace;
    int tlbInvalidationCount;
    VirtualAddress lastTlbInvalidationAddress;
    uint32_t fsRegister;
    uint32_t gsRegister;
    uint32_t ldtRegister;
    uint64_t tscRegister;
    uint64_t msrSysenterCs;
    uint64_t msrSysenterEsp;
    uint64_t msrSysenterEip;
} FakeHardware;

extern FakeHardware theFakeHardware;

uint32_t Cpu_readLocalApic(size_t offset);
void Cpu_writeLocalApic(size_t offset, uint32_t value);
uint64_t Cpu_readMsr(uint32_t msr);
void Cpu_writeMsr(uint32_t msr, uint64_t value);

static inline void *Cpu_getFaultingAddress() {
    return (void *)0;
}

static inline void Cpu_relax() {
    asm volatile("pause" : : : "memory");
}

static inline uint32_t Cpu_readFs() {
    return theFakeHardware.fsRegister;
}

static inline void Cpu_writeFs(uint32_t value) {
    theFakeHardware.fsRegister = value;
}

static inline uint32_t Cpu_readGs() {
    return theFakeHardware.gsRegister;
}

static inline void Cpu_writeGs(uint32_t value) {
    theFakeHardware.gsRegister = value;
}

static inline void Cpu_loadLdt(uint32_t value) {
    theFakeHardware.ldtRegister = value;
}

static inline Cpu *Cpu_getCurrent() {
    return theFakeHardware.currentCpu;
}

static inline void AddressSpace_activate(AddressSpace *as) {
    theFakeHardware.currentAddressSpace = as;
}

static inline void AddressSpace_invalidateTlb() {
    theFakeHardware.tlbInvalidationCount++;
}

static inline void AddressSpace_invalidateTlbAddress(VirtualAddress a) {
    theFakeHardware.lastTlbInvalidationAddress = a;
}

static inline uint64_t Tsc_read() {
    return theFakeHardware.tscRegister;
}

static inline void Cpu_cpuid(uint32_t level, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    *a = 0x000806EB;
    *b = 0x00020800;
    *c = 0x56DA2203;
    *d = 0x178BFBFF;
}

#endif
