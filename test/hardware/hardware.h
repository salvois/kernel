#ifndef TEST_HARDWARE_H_INCLUDED
#define TEST_HARDWARE_H_INCLUDED

extern Cpu *FakeHardware_currentCpu;
extern uint32_t FakeHardware_localApic[1024];

static inline uint32_t Cpu_readLocalApic(size_t offset) {
    return FakeHardware_localApic[offset / sizeof(uint32_t)];
}

static inline void Cpu_writeLocalApic(size_t offset, uint32_t value) {
    FakeHardware_localApic[offset / sizeof(uint32_t)] = value;
}

static inline void *Cpu_getFaultingAddress() {
    return (void *)0;
}

static inline Cpu *Cpu_getCurrent() {
    return FakeHardware_currentCpu;
}

#endif
