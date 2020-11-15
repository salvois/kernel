#include "kernel.h"

Cpu *FakeHardware_currentCpu;
uint32_t FakeHardware_localApic[1024];
AddressSpace *FakeHardware_currentAddressSpace;
uint32_t FakeHardware_fsRegister;
uint32_t FakeHardware_gsRegister;
uint32_t FakeHardware_ldtRegister;