KERNEL_CFLAGS = -Wall -O3 -m32 -std=gnu99 -pedantic-errors -nostdinc -nostartfiles -nostdlib -fno-builtin -fno-asynchronous-unwind-tables -Isrc -Iinclude -Isrc/hardware
KERNEL_SOURCES = \
  src/boot/entry.S \
  src/boot/entry.c \
  src/boot/Acpi.c \
  src/boot/Cpu.c \
  src/boot/LapicTimer.c \
  src/boot/MultiProcessorSpecification.c \
  src/boot/Multiboot.c \
  src/boot/PhysicalMemory.c \
  src/boot/Pic8259.c \
  src/Acpi.c \
  src/AddressSpace.c \
  src/Cpu.S \
  src/Cpu.c \
  src/CpuNode.c \
  src/ElfLoader.c \
  src/Formatter.c \
  src/Libc.c \
  src/LinkedList.c \
  src/PhysicalMemory.c \
  src/Pic8259.c \
  src/PortE9Log.c \
  src/PriorityQueue.c \
  src/Rs232Log.c \
  src/SlabAllocator.c \
  src/Syscall.c \
  src/Task.c \
  src/Thread.c \
  src/Tsc.c \
  src/Util.c \
  src/Video.c

TEST_CFLAGS = -Wall -g -m32 -std=gnu99 -pedantic-errors -nostdinc -fno-builtin -Isrc -Iinclude -Itest/hardware
TEST_SOURCES = \
  src/boot/Acpi.c \
  src/boot/Cpu.c \
  src/boot/LapicTimer.c \
  src/boot/MultiProcessorSpecification.c \
  src/boot/Multiboot.c \
  src/boot/PhysicalMemory.c \
  src/Acpi.c \
  src/AddressSpace.c \
  src/Cpu.c \
  src/CpuNode.c \
  src/Libc.c \
  src/LinkedList.c \
  src/PriorityQueue.c \
  src/PhysicalMemory.c \
  src/SlabAllocator.c \
  test/hardware/hardware.c \
  test/Boot_AcpiTest.c \
  test/Boot_CpuTest.c \
  test/Boot_MultibootTest.c \
  test/Boot_MultiProcessorSpecificationTest.c \
  test/Boot_PhysicalMemoryTest.c \
  test/AddressSpaceTest.c \
  test/LibcTest.c \
  test/LinkedListTest.c \
  test/CpuNodeTest.c \
  test/CpuTest.c \
  test/PriorityQueueTest.c \
  test/PhysicalMemoryTest.c \
  test/SlabAllocatorTest.c \
  test/test.c

DEMO_CFLAGS = -m32 -nostdlib -fno-asynchronous-unwind-tables -no-pie -fno-pie -s -Iinclude
DEMO_BINARIES = build/EndlessLoop build/Sysenter

.PHONY: all clean Release cleanRelease build-tests test

all: build/kernel.bin

Debug: CFLAGS += -DLOG=LOG_RS232LOG
Release: CFLAGS += -DLOG=LOG_NONE -DNDEBUG

Release: all
Debug: all

cleanRelease: clean

clean:
	rm -f build/*
	
build-tests:
	$(CC) $(TEST_CFLAGS) $(TEST_SOURCES) -o build/test
	
test: build-tests
	./build/test

build/EndlessLoop: demo/EndlessLoop.c
	$(CC) $(DEMO_CFLAGS) -O3 $< -o $@
build/Sysenter: demo/Sysenter.c
	$(CC) $(DEMO_CFLAGS) -O3 $< -o $@

build/kernel.bin: src/* include/* $(DEMO_BINARIES)
	$(CC) $(KERNEL_CFLAGS) $(KERNEL_SOURCES) -T link.ld -o $@
	objdump -D $@ > build/kernel.S
	objdump -M intel -D $@ > build/kernel.asm
	#strip $@
	#sudo mount ~/VirtualBox\ VMs/kerneltest/kerneltest-flat.vmdk /mnt -o loop,offset=1048576 -o umask=000
	#cp $@ /mnt
	#cp $(DEMO_BINARIES) /mnt
	#sudo umount /mnt
