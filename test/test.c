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

int exitCode = 0;

extern void LibcTest_run();
extern void CpuNodeTest_run();
extern void CpuTest_run();
extern void PriorityQueueTest_run();
extern void LinkedListTest_run();
extern void PhysicalMemoryTest_run();
extern void MultibootTest_run();
extern void SlabAllocatorTest_run();
extern void AddressSpaceTest_run();
extern void AcpiTest_run();
extern void MultiProcessorSpecificationTest_run();

int Log_printf(const char *format, ...) { return 0; }
int Video_printf(const char *format, ...) { return 0; }
void panic(const char *format, ...) { }

int main() {
    RUN_SUITE(LibcTest_run);
    RUN_SUITE(CpuNodeTest_run);
    RUN_SUITE(CpuTest_run);
    RUN_SUITE(PriorityQueueTest_run);
    RUN_SUITE(LinkedListTest_run);
    RUN_SUITE(PhysicalMemoryTest_run);
    RUN_SUITE(MultibootTest_run);
    RUN_SUITE(SlabAllocatorTest_run);
    RUN_SUITE(AddressSpaceTest_run);
    RUN_SUITE(AcpiTest_run);
    RUN_SUITE(MultiProcessorSpecificationTest_run);
    return exitCode;
}
