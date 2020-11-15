#ifndef TEST_HARDWARE_H_INCLUDED
#define TEST_HARDWARE_H_INCLUDED

extern Cpu *FakeHardware_currentCpu;
extern uint32_t FakeHardware_localApic[1024];
extern AddressSpace *FakeHardware_currentAddressSpace;
extern uint32_t FakeHardware_fsRegister;
extern uint32_t FakeHardware_gsRegister;
extern uint32_t FakeHardware_ldtRegister;

static inline uint32_t Cpu_readLocalApic(size_t offset) {
    return FakeHardware_localApic[offset / sizeof(uint32_t)];
}

static inline void Cpu_writeLocalApic(size_t offset, uint32_t value) {
    FakeHardware_localApic[offset / sizeof(uint32_t)] = value;
}

static inline void *Cpu_getFaultingAddress() {
    return (void *)0;
}

static inline uint32_t Cpu_readFs() {
    return FakeHardware_fsRegister;
}

static inline void Cpu_writeFs(uint32_t value) {
    FakeHardware_fsRegister = value;
}

static inline uint32_t Cpu_readGs() {
    return FakeHardware_gsRegister;
}

static inline void Cpu_writeGs(uint32_t value) {
    FakeHardware_gsRegister = value;
}

static inline void Cpu_loadLdt(uint32_t value) {
    FakeHardware_ldtRegister = value;
}

static inline Cpu *Cpu_getCurrent() {
    return FakeHardware_currentCpu;
}

static inline void AddressSpace_activate(AddressSpace *as) {
    FakeHardware_currentAddressSpace = as;
}

#endif
