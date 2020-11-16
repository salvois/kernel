#ifndef TEST_HARDWARE_H_INCLUDED
#define TEST_HARDWARE_H_INCLUDED

typedef struct FakeHardware {
    Cpu *currentCpu;
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
    uint32_t fsRegister;
    uint32_t gsRegister;
    uint32_t ldtRegister;
    uint64_t tscRegister;
} FakeHardware;

extern FakeHardware theFakeHardware;

uint32_t Cpu_readLocalApic(size_t offset);
void Cpu_writeLocalApic(size_t offset, uint32_t value);

static inline void *Cpu_getFaultingAddress() {
    return (void *)0;
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

static inline uint64_t Tsc_read() {
    return theFakeHardware.tscRegister;
}

#endif
