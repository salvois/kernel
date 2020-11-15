#ifndef TEST_HARDWARE_H_INCLUDED
#define TEST_HARDWARE_H_INCLUDED

typedef struct FakeHardware {
    Cpu *currentCpu;
    uint32_t localApicRegisters[1024];
    AddressSpace *currentAddressSpace;
    uint32_t fsRegister;
    uint32_t gsRegister;
    uint32_t ldtRegister;
} FakeHardware;

extern FakeHardware theFakeHardware;

static inline uint32_t Cpu_readLocalApic(size_t offset) {
    return theFakeHardware.localApicRegisters[offset / sizeof(uint32_t)];
}

static inline void Cpu_writeLocalApic(size_t offset, uint32_t value) {
    theFakeHardware.localApicRegisters[offset / sizeof(uint32_t)] = value;
}

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

#endif
