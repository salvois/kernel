CFLAGS = -Wall -O3 -m32 -std=gnu99 -pedantic-errors -nostdinc -nostartfiles -nostdlib -fno-builtin -fno-asynchronous-unwind-tables -Isrc -Iinclude -Isrc/hardware
ASMSOURCES = src/Boot_asm.S src/Cpu_asm.S
CSOURCES = \
  src/Boot.c \
  src/Acpi.c \
  src/Boot_Cpu.c \
  src/AddressSpace.c \
  src/Cpu.c \
  src/CpuNode.c \
  src/ElfLoader.c \
  src/Formatter.c \
  src/LapicTimer.c \
  src/Libc.c \
  src/LinkedList.c \
  src/Multiboot.c \
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
TESTS_CSOURCES = \
  src/Acpi.c \
  src/AddressSpace.c \
  src/Cpu.c \
  src/CpuNode.c \
  src/Libc.c \
  src/LinkedList.c \
  src/Multiboot.c \
  src/PriorityQueue.c \
  src/PhysicalMemory.c \
  src/SlabAllocator.c \
  test/hardware/hardware.c \
  test/AcpiTest.c \
  test/AddressSpaceTest.c \
  test/LibcTest.c \
  test/LinkedListTest.c \
  test/MultibootTest.c \
  test/CpuNodeTest.c \
  test/CpuTest.c \
  test/PriorityQueueTest.c \
  test/PhysicalMemoryTest.c \
  test/SlabAllocatorTest.c \
  test/test.c
OBJECTS = $(patsubst src/%.S, build/%.o, $(ASMSOURCES)) $(patsubst src/%.c, build/%.o, $(CSOURCES))
DEMOS_CFLAGS = -m32 -nostdlib -fno-asynchronous-unwind-tables -no-pie -fno-pie -s -Iinclude
DEMOS = build/EndlessLoop build/Sysenter
TESTS_CFLAGS = -Wall -g -m32 -std=gnu99 -pedantic-errors -nostdinc -fno-builtin -Isrc -Iinclude -Itest/hardware

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
	$(CC) $(TESTS_CFLAGS) $(TESTS_CSOURCES) -o build/test
	
test: build-tests
	./build/test

build/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.S include/kernel_asm.h
	$(CC) $(CFLAGS) -c $< -o $@

build/%: demo/%.c
	$(CC) $(DEMOS_CFLAGS) $< -o $@

build/Sysenter: demo/Sysenter.c
	$(CC) $(DEMOS_CFLAGS) -O3 $< -o $@

build/kernel.bin: $(OBJECTS) $(DEMOS)
	$(LD) -m elf_i386 -T link.ld $(OBJECTS) -o $@
	objdump -D $@ > build/kernel.S
	objdump -M intel -D $@ > build/kernel.asm
	#strip $@
	#sudo mount ~/VirtualBox\ VMs/kerneltest/kerneltest-flat.vmdk /mnt -o loop,offset=1048576 -o umask=000
	#cp $@ /mnt
	#cp $(DEMOS) /mnt
	#sudo umount /mnt
